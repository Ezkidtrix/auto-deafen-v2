#include "Geode/utils/Keyboard.hpp"
#include <Geode/Geode.hpp>
#include <Geode/loader/Mod.hpp>

#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

#include <Geode/modify/Modify.hpp>
#include <Geode/ui/Popup.hpp>

#include <matjson.hpp>
#include <winuser.h>

using namespace geode::prelude;

int currentID = 0;
int currentType = 0;

bool hasDeafened = false;

struct Settings {
  bool enabled = true;
  int defaultPercent = 50;

  std::vector<geode::Keybind> keybind = Mod::get()->getSettingValue<std::vector<geode::Keybind>>("keybind");
};
static Settings settings;

struct LevelSettings {
  bool enabled = false;

  int type = 0;
  int percent = 50;
};
static LevelSettings levelSettings;

template<>
struct matjson::Serialize<LevelSettings> {
  static Result<LevelSettings> fromJson(matjson::Value const& value) {
    GEODE_UNWRAP_INTO(bool enabled, value["enabled"].asBool());

    GEODE_UNWRAP_INTO(int type, value["type"].asInt());
    GEODE_UNWRAP_INTO(int percent, value["percent"].asInt());
    
    return Ok(LevelSettings{ enabled, type, percent });
  }

  static matjson::Value toJson(LevelSettings const& value) {
    auto obj = matjson::Value();
    obj["enabled"] = value.enabled;

    obj["type"] = value.type;
    obj["percent"] = value.percent;

    return obj;
  }
};

void saveSettings(int id, LevelSettings settings) {
  std::string name = "level:" + std::to_string(currentType) + "_" + std::to_string(id);
  Mod::get()->setSavedValue(name, settings);
}

LevelSettings getSettings(int id) {
  std::string name = "level:" + std::to_string(currentType) + "_" + std::to_string(id);
  return Mod::get()->getSavedValue<LevelSettings>(name, LevelSettings{ false, currentType, settings.defaultPercent });
}

int getKeyCode(std::string key) {
  std::unordered_map<std::string, int> keysMap = {
    { "Escape", VK_ESCAPE },
    { "F1", VK_F1 }, { "F2", VK_F2 }, { "F3", VK_F3 }, { "F4", VK_F4 },
    { "F5", VK_F5 }, { "F6", VK_F6 }, { "F7", VK_F7 }, { "F8", VK_F8 },
    { "F9", VK_F9 }, { "F10", VK_F10 }, { "F11", VK_F11 }, { "F12", VK_F12 },

    { "1", '1' }, { "2", '2' }, { "3", '3' }, { "4", '4' }, { "5", '5' },
    { "6", '6' }, { "7", '7' }, { "8", '8' }, { "9", '9' }, { "0", '0' },

    { "Q", 'Q' }, { "W", 'W' }, { "E", 'E' }, { "R", 'R' }, { "T", 'T' },
    { "Y", 'Y' }, { "U", 'U' }, { "I", 'I' }, { "O", 'O' }, { "P", 'P' },

    { "A", 'A' }, { "S", 'S' }, { "D", 'D' }, { "F", 'F' }, { "G", 'G' },
    { "H", 'H' }, { "J", 'J' }, { "K", 'K' }, { "L", 'L' },

    { "Z", 'Z' }, { "X", 'X' }, { "C", 'C' }, { "V", 'V' }, { "B", 'B' },
    { "N", 'N' }, { "M", 'M' },

    { "Insert", VK_INSERT }, { "Delete", VK_DELETE },
    { "Home", VK_HOME }, { "End", VK_END },
    { "PageUp", VK_PRIOR }, { "PageDown", VK_NEXT }
  };
  auto it = keysMap.find(key);

  if (it != keysMap.end()) return it->second;
  return -1;
}

void triggerDeafen() {
  if (!settings.enabled || !levelSettings.enabled) return;
  std::string key = settings.keybind[0].toString();

  auto code = getKeyCode(key);
  if (!code) return;

  // log::info("Triggered deafen keybind: {} {}", key, code);

  INPUT input = {};
  input.type = INPUT_KEYBOARD;

  input.ki.wVk = code;
  SendInput(1, &input, sizeof(INPUT));

  input.ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(1, &input, sizeof(INPUT));
}

int getLevelType(GJGameLevel* level) {
	if (level->m_levelType != GJLevelType::Saved) return 1;
	else if (level->m_dailyID > 0) return 2;
	else if (level->m_gauntletLevel) return 3;

	return 0;
}

class $modify(PlayLayer) {
  bool init(GJGameLevel* level, bool p1, bool p2) {
    PlayLayer::init(level, p1, p2);
    if (!settings.enabled) return true;

    currentID = m_level->m_levelID.value();
    currentType = getLevelType(level);

    levelSettings = getSettings(currentID);

    if (hasDeafened) triggerDeafen();
    hasDeafened = false;

    // log::info("Level Info: {} {}", currentID, currentType);
    return true;
  }

  void postUpdate(float dt) {
    PlayLayer::postUpdate(dt);
    
    if (!settings.enabled) return;
    int percent = static_cast<int>(getCurrentPercent());

    if (percent >= levelSettings.percent && percent < 100 && !hasDeafened) {
      hasDeafened = true;
      triggerDeafen();
    }
  }

  void destroyPlayer(PlayerObject* player, GameObject* object) {
    PlayLayer::destroyPlayer(player, object);
    if (!settings.enabled) return;

    if (hasDeafened) triggerDeafen();
    hasDeafened = false;
  }

  void levelComplete() {
    PlayLayer::levelComplete();
    if (!settings.enabled) return;

    if (hasDeafened) triggerDeafen();
    hasDeafened = false;
  }

  void onQuit() {
    PlayLayer::onQuit();
    if (!settings.enabled) return;

    if (hasDeafened) triggerDeafen();
    hasDeafened = false;

    saveSettings(currentID, levelSettings);
  }
};

class ConfigPopup : public geode::Popup {
protected:
  TextInput* m_input;
  CCMenuItemToggler* m_checkbox;

  bool m_checked = false;

  bool setup(ConfigPopup* popup) {
    popup->m_noElasticity = true;

    auto winSize = popup->getContentSize();
    auto menu = CCMenu::create();

    menu->setPosition({0, 0});
    menu->setID("checkbox-menu"); 

    this->setKeyboardEnabled(true);
    this->setTitle("Auto Deafen");

    m_checkbox = CCMenuItemToggler::create(
      CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png"), CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png"),
      this, menu_selector(ConfigPopup::onToggle)
    );
    m_checkbox->toggle(levelSettings.enabled);

    m_checkbox->setAnchorPoint({ 0.5f, 0.5f });
    m_checkbox->setPosition(CCPoint{ winSize.width / 2 - 50.f, winSize.height / 2 - 10.f });

    menu->addChild(m_checkbox);
    popup->addChild(menu);

    m_input = TextInput::create(90.f, "Percentage");
    m_input->setString(std::to_string(levelSettings.percent));

    m_input->setAnchorPoint({ 0.5f, 0.5f });
    m_input->setPosition(CCPoint{ winSize.width / 2 + 30.f, winSize.height / 2 - 10.f });

    m_input->setFilter("0123456789");
    m_input->setMaxCharCount(3);

    popup->addChild(m_input);
    return true;
  }

  void onToggle(CCObject* sender) {
    m_checked = !m_checkbox->isToggled();
    levelSettings.enabled = m_checked;

    // log::info("Mod enabled for level: {}", m_checked);
  }

  void onClose(CCObject* sender) override {
    Popup::onClose(sender);

    std::string inputPercent = m_input->getString();
    if (inputPercent == "") inputPercent = levelSettings.percent;

    levelSettings.percent = std::stoi(inputPercent);
    saveSettings(currentID, levelSettings);
  }

public:
  static ConfigPopup* create() {
    auto ret = new ConfigPopup();

    if (ret->init(180.f, 100.f)) {
      ret->setup(ret);
      ret->autorelease();

      return ret;
    }

    delete ret;
    return nullptr;
  }
};

class $modify(MyPauseLayer, PauseLayer) {
  void customSetup() {
    PauseLayer::customSetup();
    if (!settings.enabled) return;

    auto menu = this->getChildByID("left-button-menu");
    if (!menu) return;

    this->setupButton();
    menu->updateLayout();
  }

  void setupButton() {
    if (!settings.enabled) return;

    auto menu = this->getChildByID("left-button-menu");
    if (!menu) return;

    auto spr = ButtonSprite::create("AD");
    auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(MyPauseLayer::openPopup));

    btn->setID("auto-deafen-btn"_spr);
    auto options = AxisLayoutOptions::create()->setScaleLimits(0.6f, 1.0f);

    btn->setLayoutOptions(options);
    menu->addChild(btn);
  }

  void openPopup(CCObject* sender) {
    ConfigPopup::create()->show();
  }
};

$on_mod(Loaded) {
  settings.enabled = Mod::get()->getSettingValue<bool>("enabled");
  settings.defaultPercent = Mod::get()->getSettingValue<int>("default-percent");

  settings.keybind = Mod::get()->getSettingValue<std::vector<geode::Keybind>>("keybind");

  listenForSettingChanges<bool>("enabled", [](bool value) {
    settings.enabled = value;
  });
  listenForSettingChanges<int>("default-percent", [](int value) {
    settings.defaultPercent = value;
  });

  listenForSettingChanges<std::vector<geode::Keybind>>("keybind", [](std::vector<geode::Keybind> value) {
    settings.keybind = value;
  });
}