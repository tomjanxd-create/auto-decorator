#include <Geode/Geode.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>

#include "StylePopup.hpp"

using namespace geode::prelude;

class $modify(DecoPauseLayer, EditorPauseLayer) {
    bool init(LevelEditorLayer* editor) {
        if (!EditorPauseLayer::init(editor)) return false;

        auto spr = ButtonSprite::create("Auto Decorate", "goldFont.fnt", "GJ_button_01.png", .8f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(DecoPauseLayer::onAutoDecorate));
        btn->setID("auto-decorate-button"_spr);

        if (auto menu = this->getChildByID("resume-menu")) {
            if (auto resume = menu->getChildByID("resume-button")) {
                menu->insertAfter(btn, resume);
            }
            else {
                menu->addChild(btn);
            }
            menu->updateLayout();
        }
        else {
            // Node IDs missing for some reason: fall back to our own menu
            auto winSize = CCDirector::get()->getWinSize();
            auto menu = CCMenu::create();
            menu->setID("auto-decorate-menu"_spr);
            menu->addChild(btn);
            menu->setPosition({ winSize.width / 2, 30.f });
            this->addChild(menu);
        }

        return true;
    }

    void onAutoDecorate(CCObject*) {
        if (auto popup = StylePopup::create(m_editorLayer)) {
            popup->show();
        }
    }
};
