#include "StylePopup.hpp"
#include "Decorator.hpp"

using namespace geode::prelude;

StylePopup* StylePopup::create(LevelEditorLayer* editor) {
    auto ret = new StylePopup();
    if (ret->init(editor)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool StylePopup::init(LevelEditorLayer* editor) {
    if (!Popup::init(440.f, 270.f)) return false;
    m_editor = editor;

    this->setTitle("Auto Decorate");

    auto subtitle = CCLabelBMFont::create("Pick a style to decorate the whole level", "chatFont.fnt");
    subtitle->setScale(.8f);
    m_mainLayer->addChildAtPosition(subtitle, Anchor::Top, ccp(0, -45));

    auto const& styles = getDecoStyles();
    constexpr int columns = 5;
    for (size_t i = 0; i < styles.size(); i++) {
        auto const& style = styles[i];

        auto spr = ButtonSprite::create(style.name, "bigFont.fnt", "GJ_button_04.png", .7f);
        if (spr->m_BGSprite) spr->m_BGSprite->setColor(style.bg);
        constexpr float maxWidth = 76.f;
        if (spr->getContentWidth() > maxWidth) {
            spr->setScale(maxWidth / spr->getContentWidth());
        }

        auto btn = CCMenuItemExt::createSpriteExtra(spr, [this, i](auto) { this->onStyle(i); });
        btn->setID(fmt::format("style-{}", i));

        int col = static_cast<int>(i) % columns;
        int row = static_cast<int>(i) / columns;
        m_buttonMenu->addChildAtPosition(btn, Anchor::Center, ccp((col - 2) * 82.f, 45.f - row * 42.f));
    }

    this->addOptionToggle("Remove old deco", "clear-old", ccp(-200, -95));
    this->addOptionToggle("Change colors & BG", "apply-colors", ccp(-70, -95));

    auto removeSpr = ButtonSprite::create("Remove Deco", "goldFont.fnt", "GJ_button_06.png", .7f);
    removeSpr->setScale(.75f);
    auto removeBtn = CCMenuItemExt::createSpriteExtra(removeSpr, [this](auto) { this->onRemove(); });
    m_buttonMenu->addChildAtPosition(removeBtn, Anchor::Center, ccp(150, -95));

    return true;
}

void StylePopup::addOptionToggle(char const* label, char const* saveKey, CCPoint pos) {
    std::string key = saveKey;
    auto toggle = CCMenuItemExt::createTogglerWithStandardSprites(.6f, [key](CCMenuItemToggler* toggler) {
        // The callback runs before the toggle flips its state
        Mod::get()->setSavedValue(key, !toggler->isToggled());
    });
    toggle->toggle(Mod::get()->getSavedValue<bool>(key, true));
    m_buttonMenu->addChildAtPosition(toggle, Anchor::Center, pos);

    auto text = CCLabelBMFont::create(label, "bigFont.fnt");
    text->setScale(.3f);
    text->setAnchorPoint({ 0.f, .5f });
    m_buttonMenu->addChildAtPosition(text, Anchor::Center, pos + ccp(14, 0));
}

void StylePopup::onStyle(size_t index) {
    auto const& style = getDecoStyles().at(index);

    DecorateOptions options;
    options.clearOld = Mod::get()->getSavedValue<bool>("clear-old", true);
    options.applyColors = Mod::get()->getSavedValue<bool>("apply-colors", true);

    auto result = decorateLevel(m_editor, style, options);

    std::string message = fmt::format("{} style: placed {} objects", style.name, result.placed);
    if (result.removed > 0) {
        message += fmt::format(", removed {} old", result.removed);
    }
    Notification::create(message, NotificationIcon::Success)->show();
    this->onClose(nullptr);
}

void StylePopup::onRemove() {
    int removed = removeDecoration(m_editor);
    Notification::create(
        removed > 0 ? fmt::format("Removed {} decoration objects", removed) : "No auto decoration to remove",
        removed > 0 ? NotificationIcon::Success : NotificationIcon::Info
    )->show();
}
