#pragma once

#include <Geode/Geode.hpp>

class StylePopup : public geode::Popup {
protected:
    LevelEditorLayer* m_editor = nullptr;

    bool init(LevelEditorLayer* editor);
    void addOptionToggle(char const* label, char const* saveKey, cocos2d::CCPoint pos);
    void onStyle(size_t index);
    void onRemove();

public:
    static StylePopup* create(LevelEditorLayer* editor);
};
