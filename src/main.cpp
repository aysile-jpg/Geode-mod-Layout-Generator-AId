#include <Geode/Geode.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include "LayoutPopup.hpp"

using namespace geode::prelude;

class $modify(LayoutAIPause, EditorPauseLayer) {
	bool init(LevelEditorLayer* editor) {
		if (!EditorPauseLayer::init(editor)) return false;

		auto menu = CCMenu::create();
		auto btn = CCMenuItemSpriteExtra::create(
			ButtonSprite::create("AI Layout", 90, true, "bigFont.fnt", "GJ_button_04.png", 30.f, 0.6f),
			this, menu_selector(LayoutAIPause::onAILayout)
		);
		menu->addChild(btn);
		auto ws = CCDirector::get()->getWinSize();
		menu->setPosition({ws.width - 70.f, ws.height - 30.f});
		this->addChild(menu);
		return true;
	}

	void onAILayout(CCObject*) {
		LayoutPopup::create(m_editorLayer)->show();
	}
};
