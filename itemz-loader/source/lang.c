#include "lang.h"
#include "ini.h"
#include "Header.h"
#include <unordered_map>
#include <string>
#include <array>
#include <memory>

class LanguageManager {
private:
    std::array<std::string, LANG_NUM_OF_STRINGS> langStrings;
    bool initialized = false;

    LanguageManager() = default;

    static int iniHandler(void* user, const char* section, const char* name, const char* value) {
        auto* langManager = static_cast<LanguageManager*>(user);
        if (strcmp(section, "LOADER") == 0) {
            for (int i = 0; i < LANG_NUM_OF_STRINGS; ++i) {
                if (strcmp(name, langKeyNames[i]) == 0) {
                    langManager->langStrings[i] = value;
                    return 1;
                }
            }
        }
        return 0;
    }

public:
    static LanguageManager& getInstance() {
        static LanguageManager instance;
        return instance;
    }

    bool initialize(int langCode) {
        if (initialized) return true;

        char langFilePath[100];
        snprintf(langFilePath, sizeof(langFilePath), LANG_DIR, langCode);

        int error = ini_parse(langFilePath, iniHandler, this);
        if (error != 0) {
            logshit("Bad config file (first error on line %d)!", error);
            return false;
        }

        initialized = true;
        return true;
    }

    const std::string& getLanguageString(Lang_STR str) const {
        if (!initialized) {
            static const std::string errorMsg = "ERROR: Lang has not loaded";
            return errorMsg;
        }

        if (str >= LANG_NUM_OF_STRINGS) {
            return langStrings[STR_NOT_FOUND];
        }

        return langStrings[str].empty() ? langStrings[STR_NOT_FOUND] : langStrings[str];
    }

    bool isInitialized() const { return initialized; }
};

// For backward compatibility
bool lang_is_initialized = false;

int32_t PS4GetLang() {
    int32_t lang;
    if (sceSystemServiceParamGetInt(1, &lang) != 0) {
        logshit("Unable to get language code");
        return 0x1;
    }
    return lang;
}

const char* getLangSTR(Lang_STR str) {
    return LanguageManager::getInstance().getLanguageString(str).c_str();
}

bool LoadLangs(int LangCode) {
    auto& langManager = LanguageManager::getInstance();
    if (langManager.initialize(LangCode)) {
        lang_is_initialized = true;
        return true;
    }
    msgok("LoadLangs failed");
    return false;
}

// Array of language key names for INI parsing
const char* langKeyNames[LANG_NUM_OF_STRINGS] = {
    "REINSTALL_PKG", "LOADER_ERROR", "MORE_INFO", "DOWNLOADING_UPDATE",
    "INI_FAIL", "UPDATE_REQ", "UPDATE_APPLIED", "OPT_UPDATE",
    "LOADER_FATAL", "FATAL_JB", "FATAL_REJAIL", "EXT_NOT_SUPPORTED",
    "SWU_ERROR", "RSA_LOAD", "RSA_FAILED", "SECURE_FAIL",
    "STR_NOT_FOUND", "PING_FAILED"
};