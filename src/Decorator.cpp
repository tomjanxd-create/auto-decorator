#include "Decorator.hpp"

#include <cmath>
#include <random>
#include <unordered_map>
#include <unordered_set>

using namespace geode::prelude;

namespace {
    constexpr float CELL = 30.f;
    constexpr int MAX_PLACED = 6000;

    // Color channels used for the decoration itself
    constexpr int CH_TOP = 990;
    constexpr int CH_BOTTOM = 991;
    constexpr int CH_BACK = 992;
    constexpr int CH_FLOOR = 993;

    struct Piece {
        int id = 0;
        CCSize size;
        bool valid = false;
    };

    std::string stripPng(std::string name) {
        if (name.ends_with(".png")) name.resize(name.size() - 4);
        return name;
    }

    // Resolves a sprite frame name to a decoration object ID using the game's
    // own object table. Anything that isn't a pure decoration object (no
    // hitbox) is rejected so the decorator can never alter gameplay.
    Piece const& lookupPiece(char const* frame) {
        static std::unordered_map<std::string, int> frameToID;
        static std::unordered_map<std::string, Piece> cache;

        if (frameToID.empty()) {
            for (auto const& [id, name] : ObjectToolbox::sharedState()->m_allKeys) {
                frameToID.emplace(stripPng(std::string(name)), id);
            }
        }

        auto key = stripPng(frame);
        if (auto it = cache.find(key); it != cache.end()) {
            return it->second;
        }

        Piece piece;
        if (auto it = frameToID.find(key); it != frameToID.end()) {
            if (auto obj = GameObject::createWithKey(it->second)) {
                if (obj->m_objectType == GameObjectType::Decoration) {
                    piece = { it->second, obj->getContentSize(), true };
                }
            }
        }
        if (!piece.valid) {
            log::warn("Decoration piece '{}' is unavailable, skipping it", frame);
        }
        return cache.emplace(key, piece).first->second;
    }

    std::vector<Piece> resolvePieces(std::vector<char const*> const& frames) {
        std::vector<Piece> pieces;
        for (auto frame : frames) {
            auto const& piece = lookupPiece(frame);
            if (piece.valid) pieces.push_back(piece);
        }
        return pieces;
    }

    bool hasDecoTag(GameObject* obj) {
        if (!obj->m_groups) return false;
        for (auto group : *obj->m_groups) {
            if (group == DECO_GROUP) return true;
        }
        return false;
    }

    bool isNonGameplay(GameObjectType type) {
        switch (type) {
            case GameObjectType::Decoration:
            case GameObjectType::Modifier:
            case GameObjectType::Special:
            case GameObjectType::EnterEffectObject:
            case GameObjectType::CollisionObject:
                return true;
            default:
                return false;
        }
    }

    int64_t cellKey(int x, int y) {
        return (static_cast<int64_t>(x) << 32) ^ static_cast<uint32_t>(y);
    }

    void setColor(GJEffectManager* manager, int channel, ccColor3B color, float opacity = 1.f, bool blending = false) {
        if (!manager) return;
        auto action = manager->getColorAction(channel);
        if (!action) return;
        action->m_color = color;
        action->m_fromColor = color;
        action->m_toColor = color;
        action->m_currentOpacity = opacity;
        action->m_fromOpacity = opacity;
        action->m_toOpacity = opacity;
        action->m_blending = blending;
        action->m_playerColor = 0;
        action->m_copyID = 0;
        action->m_copyOpacity = false;
        action->m_legacyHSV = false;
    }

    void applyColors(GJEffectManager* manager, DecoStyle const& style) {
        setColor(manager, 1000, style.bg);
        setColor(manager, 1001, style.groundColor);
        setColor(manager, 1002, style.line);
        setColor(manager, 1004, style.obj);
        setColor(manager, 1009, style.ground2);
        setColor(manager, 1013, style.bg);
        setColor(manager, 1014, style.ground2);
        setColor(manager, CH_TOP, style.topColor);
        setColor(manager, CH_BOTTOM, style.bottomColor);
        setColor(manager, CH_BACK, style.backColor, style.backOpacity, style.backBlending);
        setColor(manager, CH_FLOOR, style.groundDecoColor);
    }

    void applyLevelLook(LevelEditorLayer* editor, DecoStyle const& style) {
        auto settings = editor->m_levelSettings;
        if (!settings) return;

        settings->m_backgroundIndex = style.background;
        settings->m_groundIndex = style.ground;
        settings->m_groundLineIndex = style.groundLine;
        settings->m_middleGroundIndex = style.middleground;

        applyColors(settings->m_effectManager, style);
        if (editor->m_effectManager && editor->m_effectManager != settings->m_effectManager) {
            applyColors(editor->m_effectManager, style);
        }

        // Same refresh the editor does after closing the level settings menu
        editor->levelSettingsUpdated();
    }

    // Objects in a level string are stored relative to the ground, while the
    // editor works in layer coordinates. Measure the offset instead of
    // assuming it.
    CCPoint measureStringOffset(LevelEditorLayer* editor) {
        CCPoint offset = { 0.f, 90.f };
        auto created = editor->createObjectsFromString("1,1,2,0,3,0;", true, true);
        if (created && created->count() > 0) {
            offset = static_cast<GameObject*>(created->objectAtIndex(0))->getPosition();
            for (auto obj : CCArrayExt<GameObject*>(created)) {
                editor->removeObject(obj, true);
            }
        }
        return offset;
    }
}

int removeDecoration(LevelEditorLayer* editor) {
    std::vector<GameObject*> tagged;
    for (auto obj : CCArrayExt<GameObject*>(editor->m_objects)) {
        if (hasDecoTag(obj)) tagged.push_back(obj);
    }
    if (tagged.empty()) return 0;

    if (editor->m_editorUI) editor->m_editorUI->deselectAll();
    for (auto obj : tagged) {
        editor->removeObject(obj, true);
    }
    return static_cast<int>(tagged.size());
}

DecorateResult decorateLevel(LevelEditorLayer* editor, DecoStyle const& style, DecorateOptions const& options) {
    DecorateResult result;
    if (!editor) return result;

    if (options.clearOld) {
        result.removed = removeDecoration(editor);
    }
    if (options.applyColors) {
        applyLevelLook(editor, style);
    }

    auto const offset = measureStringOffset(editor);

    // --- Scan the level ---------------------------------------------------
    struct SolidCell {
        float top = -1e9f;
        float bottom = 1e9f;
    };
    std::unordered_map<int64_t, SolidCell> solids;
    std::unordered_set<int64_t> occupied;
    float levelEnd = 0.f;

    for (auto obj : CCArrayExt<GameObject*>(editor->m_objects)) {
        if (hasDecoTag(obj)) continue;
        auto type = obj->m_objectType;
        auto pos = obj->getPosition() - offset;
        levelEnd = std::max(levelEnd, pos.x);
        if (isNonGameplay(type)) continue;

        if (type == GameObjectType::Solid) {
            auto rect = obj->getObjectRect();
            rect.origin = rect.origin - offset;
            int x0 = static_cast<int>(std::floor(rect.getMinX() / CELL));
            int x1 = static_cast<int>(std::floor((rect.getMaxX() - 0.01f) / CELL));
            int y0 = static_cast<int>(std::floor(rect.getMinY() / CELL));
            int y1 = static_cast<int>(std::floor((rect.getMaxY() - 0.01f) / CELL));
            // Huge scaled blocks: only track their outline-sized footprint
            if (x1 - x0 > 64 || y1 - y0 > 64) {
                x0 = x1 = static_cast<int>(std::floor(pos.x / CELL));
                y0 = y1 = static_cast<int>(std::floor(pos.y / CELL));
            }
            for (int x = x0; x <= x1; x++) {
                for (int y = y0; y <= y1; y++) {
                    auto& cell = solids[cellKey(x, y)];
                    cell.top = std::max(cell.top, rect.getMaxY());
                    cell.bottom = std::min(cell.bottom, rect.getMinY());
                    occupied.insert(cellKey(x, y));
                }
            }
        }
        else {
            occupied.insert(cellKey(
                static_cast<int>(std::floor(pos.x / CELL)),
                static_cast<int>(std::floor(pos.y / CELL))
            ));
        }
    }
    levelEnd = std::max(levelEnd + CELL * 10, CELL * 30);

    auto top = resolvePieces(style.top);
    auto bottom = resolvePieces(style.bottom);
    auto floorPieces = resolvePieces(style.floor);
    auto back = resolvePieces(style.back);

    // --- Build the object string -------------------------------------------
    std::mt19937 rng(std::random_device{}());
    auto chance = [&](float p) { return std::uniform_real_distribution<float>(0.f, 1.f)(rng) < p; };
    auto range = [&](float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(rng); };
    auto pick = [&](std::vector<Piece> const& pieces) -> Piece const& {
        return pieces[std::uniform_int_distribution<size_t>(0, pieces.size() - 1)(rng)];
    };

    std::string out;
    int count = 0;
    auto place = [&](Piece const& piece, float x, float y, float rotation, float scale, bool flipX, int channel, ZLayer zLayer, int zOrder) {
        if (count >= MAX_PLACED) return;
        out += fmt::format(
            "1,{},2,{:.2f},3,{:.2f},4,{},6,{:.0f},32,{:.2f},128,{:.2f},129,{:.2f},21,{},22,{},24,{},25,{},57,{};",
            piece.id, x, y, flipX ? 1 : 0, rotation, scale, scale, scale,
            channel, channel, static_cast<int>(zLayer), zOrder, DECO_GROUP
        );
        count++;
    };

    // Solid surfaces: decoration on top, hanging decoration underneath
    for (auto const& [key, cell] : solids) {
        int cx = static_cast<int>(key >> 32);
        int cy = static_cast<int>(static_cast<int32_t>(key & 0xffffffff));
        float centerX = cx * CELL + CELL / 2;

        if (!top.empty() && !occupied.contains(cellKey(cx, cy + 1)) && chance(style.topChance)) {
            auto const& piece = pick(top);
            float scale = range(0.75f, 1.1f);
            place(piece, centerX + range(-4.f, 4.f), cell.top + piece.size.height * scale / 2 - 3.f,
                0.f, scale, chance(0.5f), CH_TOP, ZLayer::B1, -2);
        }
        if (!bottom.empty() && cy >= 2 &&
            !occupied.contains(cellKey(cx, cy - 1)) && !occupied.contains(cellKey(cx, cy - 2)) &&
            chance(style.bottomChance)
        ) {
            auto const& piece = pick(bottom);
            float scale = range(0.8f, 1.f);
            place(piece, centerX, cell.bottom - piece.size.height * scale / 2 + 3.f,
                180.f, scale, chance(0.5f), CH_BOTTOM, ZLayer::B1, -2);
        }
    }

    // Ground
    if (!floorPieces.empty()) {
        int lastCell = static_cast<int>(levelEnd / CELL);
        for (int cx = 0; cx <= lastCell; cx++) {
            if (occupied.contains(cellKey(cx, 0)) || !chance(style.floorChance)) continue;
            auto const& piece = pick(floorPieces);
            float scale = range(0.7f, 1.1f);
            place(piece, cx * CELL + CELL / 2 + range(-5.f, 5.f), piece.size.height * scale / 2 - 2.f,
                0.f, scale, chance(0.5f), CH_FLOOR, ZLayer::B2, -1);
        }
    }

    // Background art
    if (!back.empty()) {
        float spacing = std::max(style.backSpacing, 1.f) * CELL;
        for (float x = range(0.f, spacing); x < levelEnd; x += spacing * range(0.6f, 1.4f)) {
            auto const& piece = pick(back);
            place(piece, x, range(60.f, 360.f), 0.f, range(style.backScaleMin, style.backScaleMax),
                chance(0.5f), CH_BACK, ZLayer::B4, -10);
        }
    }

    if (count > 0) {
        auto created = editor->createObjectsFromString(out, false, false);
        result.placed = created ? static_cast<int>(created->count()) : 0;
    }
    return result;
}
