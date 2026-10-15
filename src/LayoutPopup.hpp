#pragma once
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <random>
#include "Generator.hpp"

using namespace geode::prelude;

class LayoutPopup : public Popup<LevelEditorLayer*> {
protected:
	LevelEditorLayer* m_editor = nullptr;
	TextInput* m_songInput = nullptr;
	TextInput* m_nameInput = nullptr;
	TextInput* m_bpmInput = nullptr;
	TextInput* m_lenInput = nullptr;
	CCMenuItemSpriteExtra* m_genBtn = nullptr;
	bool m_modes[8] = {true, false, false, false, false, false, false, false};
	bool m_speeds[5] = {false, true, false, false, false};
	LayoutParams m_params;
	bool m_busy = false;
	EventListener<web::WebTask> m_listener;

	static constexpr char const* MODE_NAMES[8] = {"cube", "ship", "ball", "ufo", "wave", "robot", "spider", "swing"};

	bool setup(LevelEditorLayer* editor) override {
		m_editor = editor;
		this->setTitle("AI Layout");

		float w = 360.f;
		auto menu = CCMenu::create();
		menu->setPosition({0, 0});
		m_mainLayer->addChild(menu);

		auto label = [&](char const* text, float x, float y, float scale, CCPoint anchor) {
			auto l = CCLabelBMFont::create(text, "bigFont.fnt");
			l->setScale(scale);
			l->setAnchorPoint(anchor);
			l->setPosition({x, y});
			m_mainLayer->addChild(l);
		};

		char const* names[] = {"Song ID", "Song name", "BPM", "Seconds"};
		char const* defs[] = {"0", "", "120", "60"};
		TextInput** inputs[] = {&m_songInput, &m_nameInput, &m_bpmInput, &m_lenInput};
		for (int i = 0; i < 4; i++) {
			float cx = 50.f + i * 86.f;
			label(names[i], cx, 245.f, 0.3f, {0.5f, 0.5f});
			auto in = TextInput::create(76.f, names[i], "bigFont.fnt");
			in->setPosition({cx, 220.f});
			in->setString(defs[i]);
			if (i == 0 || i == 3) in->setFilter(CommonFilter::Uint);
			if (i == 2) in->setFilter(CommonFilter::Float);
			m_mainLayer->addChild(in);
			*inputs[i] = in;
		}

		char const* modeLabels[] = {"Cube", "Ship", "Ball", "UFO", "Wave", "Robot", "Spider", "Swing"};
		label("Modes", w / 2, 190.f, 0.4f, {0.5f, 0.5f});
		for (int i = 0; i < 8; i++) {
			float cx = 40.f + (i % 4) * 80.f;
			float cy = 165.f - (i / 4) * 35.f;
			auto t = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(LayoutPopup::onMode), 0.6f);
			t->setTag(i);
			t->setPosition({cx, cy});
			t->toggle(m_modes[i]);
			menu->addChild(t);
			label(modeLabels[i], cx + 18.f, cy, 0.3f, {0.f, 0.5f});
		}

		char const* speedNames[] = {"0.5x", "1x", "2x", "3x", "4x"};
		label("Speeds", w / 2, 100.f, 0.4f, {0.5f, 0.5f});
		for (int i = 0; i < 5; i++) {
			float cx = 35.f + i * 65.f;
			auto t = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(LayoutPopup::onSpeed), 0.6f);
			t->setTag(i);
			t->setPosition({cx, 75.f});
			t->toggle(m_speeds[i]);
			menu->addChild(t);
			label(speedNames[i], cx + 18.f, 75.f, 0.3f, {0.f, 0.5f});
		}

		m_genBtn = CCMenuItemSpriteExtra::create(
			ButtonSprite::create("Generate"), this, menu_selector(LayoutPopup::onGenerate)
		);
		m_genBtn->setPosition({w / 2, 28.f});
		menu->addChild(m_genBtn);
		return true;
	}

	void onMode(CCObject* s) {
		auto t = static_cast<CCMenuItemToggler*>(s);
		m_modes[t->getTag()] = !t->isToggled();
	}

	void onSpeed(CCObject* s) {
		auto t = static_cast<CCMenuItemToggler*>(s);
		m_speeds[t->getTag()] = !t->isToggled();
	}

	void applyPlan(std::vector<Segment> const& plan) {
		if (m_params.songID > 0) m_editor->m_level->m_songID = m_params.songID;
		auto specs = buildLayout(m_params, plan);
		for (auto& o : specs) {
			auto obj = m_editor->createObject(o.id, ccp(o.x, o.y), false);
			if (obj && o.rot != 0.f) obj->setRotation(o.rot);
		}
		Notification::create(
			fmt::format("Layout готов: {} объектов", specs.size()),
			NotificationIcon::Success
		)->show();
		this->onClose(nullptr);
	}

	void fallback(std::string const& reason) {
		Notification::create(reason + " — локальная генерация", NotificationIcon::Warning)->show();
		applyPlan(randomPlan(m_params));
	}

	std::string buildUserPrompt(std::string const& songName) {
		static char const* speedNames[] = {"0.5x", "1x", "2x", "3x", "4x"};
		std::string modes, speeds;
		for (auto m : m_params.modes) modes += std::string(modes.empty() ? "" : ", ") + MODE_NAMES[(int)m];
		for (auto s : m_params.speeds) speeds += fmt::format("{}{}={}", speeds.empty() ? "" : ", ", s, speedNames[s]);
		return fmt::format(
			"Song: \"{}\" (ID {}). BPM: {}. Requested length: {} seconds. "
			"Allowed modes: [{}]. Allowed speed indices: [{}]. Design the layout plan.",
			songName.empty() ? "unknown" : songName, m_params.songID, m_params.bpm,
			m_params.lengthSec, modes, speeds
		);
	}

	std::vector<Segment> parsePlan(std::string content) {
		std::vector<Segment> plan;
		auto a = content.find('{');
		auto b = content.rfind('}');
		if (a == std::string::npos || b == std::string::npos || b <= a) return plan;
		auto root = matjson::parse(content.substr(a, b - a + 1));
		if (!root) return plan;
		auto arr = (*root)["segments"].asArray();
		if (!arr) return plan;

		std::mt19937 rng(std::random_device{}());
		for (auto item : arr.unwrap()) {
			Segment s;
			std::string ms = item["mode"].asString().unwrapOr("cube");
			std::transform(ms.begin(), ms.end(), ms.begin(), [](unsigned char c) { return std::tolower(c); });
			bool found = false;
			for (int i = 0; i < 8; i++) {
				if (ms == MODE_NAMES[i]) { s.mode = static_cast<Mode>(i); found = true; }
			}
			// режим должен быть разрешён пользователем
			if (!found || std::find(m_params.modes.begin(), m_params.modes.end(), s.mode) == m_params.modes.end()) {
				s.mode = m_params.modes[rng() % m_params.modes.size()];
			}
			s.speed = item["speed"].asInt().unwrapOr(1);
			if (std::find(m_params.speeds.begin(), m_params.speeds.end(), s.speed) == m_params.speeds.end()) {
				s.speed = m_params.speeds[rng() % m_params.speeds.size()];
			}
			s.beats = std::clamp((int)item["beats"].asInt().unwrapOr(12), 4, 32);
			s.density = std::clamp((int)item["density"].asInt().unwrapOr(60), 10, 90);
			plan.push_back(s);
		}
		return plan;
	}

	void onResponse(web::WebResponse* res) {
		if (!res->ok()) {
			return this->fallback(fmt::format("API вернул ошибку {}", res->code()));
		}
		auto json = res->json();
		if (!json) return this->fallback("Не удалось прочитать ответ API");
		auto content = (*json)["choices"][std::size_t(0)]["message"]["content"].asString();
		if (!content) return this->fallback("Пустой ответ нейросети");
		auto plan = this->parsePlan(content.unwrap());
		if (plan.empty()) return this->fallback("Нейросеть вернула неверный JSON");
		this->applyPlan(plan);
	}

	void onGenerate(CCObject*) {
		if (m_busy) return;

		m_params = LayoutParams();
		m_params.songID = numFromString<int>(m_songInput->getString()).unwrapOr(0);
		m_params.bpm = numFromString<float>(m_bpmInput->getString()).unwrapOr(120.f);
		m_params.lengthSec = std::clamp(numFromString<int>(m_lenInput->getString()).unwrapOr(60), 10, 600);
		for (int i = 0; i < 8; i++) if (m_modes[i]) m_params.modes.push_back(static_cast<Mode>(i));
		for (int i = 0; i < 5; i++) if (m_speeds[i]) m_params.speeds.push_back(i);
		if (m_params.modes.empty()) m_params.modes.push_back(Mode::Cube);
		if (m_params.speeds.empty()) m_params.speeds.push_back(1);
		m_params.seed = std::random_device{}();

		auto mod = Mod::get();
		bool useAi = mod->getSettingValue<bool>("use-ai");
		auto key = mod->getSettingValue<std::string>("api-key");
		auto url = mod->getSettingValue<std::string>("api-url");
		auto model = mod->getSettingValue<std::string>("model");

		if (!useAi) return this->applyPlan(randomPlan(m_params));
		if (key.empty()) return this->fallback("API key не задан (настройки мода)");

		static char const* SYSTEM_PROMPT = R"PROMPT(You are an expert Geometry Dash level designer. Respond ONLY with minified JSON, no markdown, no commentary.
Schema: {"segments":[{"mode":"cube|ship|ball|ufo|wave|robot|spider|swing","speed":0-4,"beats":8-24,"density":10-90}]}
Speed index: 0=0.5x, 1=1x, 2=2x, 3=3x, 4=4x. Density = amount of obstacles (higher = harder).
Use only the allowed modes and speed indices. Shape the energy like a song: calm intro, build-up, intense drops, calm outro.
The total duration (sum of beats*60/bpm) should be close to the requested length.)PROMPT";

		matjson::Value sys = matjson::makeObject({{"role", "system"}, {"content", std::string(SYSTEM_PROMPT)}});
		matjson::Value usr = matjson::makeObject({{"role", "user"}, {"content", this->buildUserPrompt(m_nameInput->getString())}});
		auto body = matjson::makeObject({
			{"model", model},
			{"messages", std::vector<matjson::Value>{sys, usr}},
			{"temperature", 0.7}
		});

		web::WebRequest req;
		req.header("Authorization", fmt::format("Bearer {}", key));
		req.bodyJSON(body);
		req.timeout(std::chrono::seconds(60));

		m_listener.bind([this](web::WebTask::Event* e) {
			if (auto* res = e->getValue()) {
				m_busy = false;
				this->onResponse(res);
			} else if (e->isCancelled()) {
				m_busy = false;
				this->fallback("Запрос отменён");
			}
		});
		m_busy = true;
		m_genBtn->setEnabled(false);
		m_genBtn->setOpacity(120);
		Notification::create("Нейросеть думает...", NotificationIcon::Loading, 2.f)->show();
		m_listener.setFilter(req.post(url));
	}

public:
	static LayoutPopup* create(LevelEditorLayer* editor) {
		auto ret = new LayoutPopup();
		if (ret->initAnchored(360.f, 290.f, editor)) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}
};
