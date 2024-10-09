#include "defines.h"
#include "lang.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <array>
#include <memory>

class LanguageManager {
private:
    static LanguageManager* instance;
    std::unordered_map<LANG_STR::LANG_STRINGS, std::string> langStrings;
    std::array<std::string, 30> languageNames;
    bool initialized = false;

    LanguageManager() {
        initializeLanguageNames();
    }

    void initializeLanguageNames() {
        languageNames = {
            "Japanese", "English (United States)", "French (France)", "Spanish (Spain)", "German",
            "Italian", "Dutch", "Portuguese (Portugal)", "Russian", "Korean",
            "Chinese (Traditional)", "Chinese (Simplified)", "Finnish", "Swedish", "Danish",
            "Norwegian", "Polish", "Portuguese (Brazil)", "English (United Kingdom)", "Turkish",
            "Spanish (Latin America)", "Arabic", "French (Canada)", "Czech", "Hungarian",
            "Greek", "Romanian", "Thai", "Vietnamese", "Indonesian"
        };
    }

public:
    static LanguageManager& getInstance() {
        if (!instance) {
            instance = new LanguageManager();
        }
        return *instance;
    }

    bool initialize(int langCode) {
        if (initialized) return true;

        std::string langFilePath = fmt::format("{0:}/{1:d}/lang.ini", LANG_DIR, langCode);
        if (!if_exists(langFilePath.c_str())) {
            log_error("Lang file not found! %s", langFilePath.c_str());
            return false;
        }

        int error = ini_parse(langFilePath.c_str(), 
            [](void* user, const char*, const char* name, const char* value) -> int {
                auto* manager = static_cast<LanguageManager*>(user);
                manager->langStrings[static_cast<LANG_STR::LANG_STRINGS>(std::stoi(name))] = value;
                return 1;
            }, this);

        if (error < 0) {
            log_error("Bad config file (first error on line %d)!", error);
            return false;
        }

        initialized = true;
        return true;
    }

    std::string getLanguageString(LANG_STR::LANG_STRINGS str) const {
        auto it = langStrings.find(str);
        if (it != langStrings.end()) {
            return it->second;
        }
        return langStrings.at(LANG_STR::STR_NOT_FOUND);
    }

    std::string getLanguageName(int code) const {
        if (code >= 0 && code < static_cast<int>(languageNames.size())) {
            return languageNames[code];
        }
        return "UNKNOWN";
    }

    bool isInitialized() const { return initialized; }
};


// Backwards compatibility
LanguageManager* LanguageManager::instance = nullptr;

bool lang_is_initialized = false;
std::vector<std::string> gm_p_text(20);
std::vector<std::string> ITEMZ_SETTING_TEXT(NUMBER_OF_SETTINGS + 10);

std::string Language_GetName(int m_Code) {
    return LanguageManager::getInstance().getLanguageName(m_Code);
}

int32_t PS4GetLang() {
    int32_t lang = 1;
#if defined(__ORBIS__)
    if (sceSystemServiceParamGetInt(1, &lang) != 0) {
        log_error("Unable to get language code");
        return 0x1;
    }
#endif
    return lang;
}

std::string getLangSTR(LANG_STR::LANG_STRINGS str) {
    return LanguageManager::getInstance().getLanguageString(str);
}

void fill_menu_text() {
    if (numb_of_settings > NUMBER_OF_SETTINGS) {
        log_info("In Advanced menu, skipping..");
        return;
    }

    auto& manager = LanguageManager::getInstance();
    for (int i = 0; i < 12; ++i) {
        ITEMZ_SETTING_TEXT[i] = fmt::format("{0:.30}", manager.getLanguageString(static_cast<LANG_STR::LANG_STRINGS>(LANG_STR::SETTINGS_1 + i)));
    }

    for (int i = 0; i < 6; ++i) {
        ITEMZ_SETTING_TEXT[i + 12] = fmt::format("{0:.30}", manager.getLanguageString(static_cast<LANG_STR::LANG_STRINGS>(LANG_STR::EXTRA_SETTING_1 + i)));
    }

    for (int i = 0; i < 10; ++i) {
        gm_p_text[i] = fmt::format("{0:.20}", manager.getLanguageString(static_cast<LANG_STR::LANG_STRINGS>(LANG_STR::LAUNCH_GAME + i)));
    }
}

bool load_embdded_eng() {
#if defined(__ORBIS__)
    if (!if_exists("/user/app/ITEM00001/lang.ini")) {
        int fd = open("/user/app/ITEM00001/lang.ini", O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (fd > 0) {
            write(fd, lang_ini, lang_ini_sz);
            close(fd);
        } else {
            return false;
        }
    }
#endif

    auto& manager = LanguageManager::getInstance();
    if (manager.initialize(1)) { // 1 is code for English
        lang_is_initialized = true;
        fill_menu_text();
        return true;
    }
    return false;
}

bool LoadLangs(int LangCode) {
    auto& manager = LanguageManager::getInstance();

#ifndef TEST_INI_LANGS
    if (lang_is_initialized) {
        return true;
    }
#endif

    if (manager.initialize(LangCode)) {
        lang_is_initialized = true;
        fill_menu_text();
        return true;
    }
    return false;
}