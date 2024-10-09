#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <unordered_map>
#include <fstream>
#include <locale>
#include <codecvt>
#include <filesystem>
#include "game_saves.h"
#include "patcher.h"
#include "net.h"

namespace fs = std::filesystem;

class PatchManager {
private:
    static constexpr unsigned char FALSE_DATA[2] = { 0x30, 0xa }; // "0\n"
    static constexpr unsigned char TRUE_DATA[2] = { 0x31, 0xa }; // "1\n"
    std::string patch_file;
    std::string patch_path_base = "/data/GoldHEN";

    static u64 djb2(const std::string& str) {
        u64 hash = 5381;
        for (char c : str) {
            hash = hash * 33 ^ c;
        }
        return hash;
    }

    bool writeToFile(const std::string& filename, const std::string& data) {
        std::ofstream file(filename);
        if (!file) return false;
        file << data;
        return true;
    }

    std::string readFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file) return "";
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

public:
    u64 patch_hash_calc(const std::string& title, const std::string& name, const std::string& app_ver, 
                        const std::string& title_id, const std::string& name_elf) {
        std::string pre_hash = title + name + app_ver + title_id + name_elf;
        u64 hash_data = djb2(pre_hash);
        log_debug("hash_input: %s", pre_hash.c_str());
        log_debug("hash_data: 0x%016lx", hash_data);
        return hash_data;
    }

    void write_enable(const std::string& cfg_file, const std::string& patch_name) {
        std::string content = readFromFile(cfg_file);
        if (content.empty()) return;

        bool new_state = (content[0] == '0');
        std::string new_content = new_state ? "1\n" : "0\n";
        
        if (writeToFile(cfg_file, new_content)) {
            log_debug("Setting %s %s", cfg_file.c_str(), new_state ? "true" : "false");
            std::string patch_state = fmt::format("{} {}", 
                new_state ? getLangSTR(LANG_STR::PATCH_SET1) : getLangSTR(LANG_STR::PATCH_SET0),
                patch_name);
            ani_notify(NOTIFI::GAME, patch_state, "");
        }
    }

    bool get_patch_path(const std::string& path) {
        patch_file = fmt::format("{}/patches/xml/{}.xml", patch_path_base, path);
        log_info("Got patch path: %s", patch_file.c_str());
        return fs::exists(patch_file);
    }

    void get_metadata1(struct _trainer_struct *tmp,
                       const std::string& patch_app_ver,
                       const std::string& patch_app_elf,
                       const std::string& patch_title,
                       const std::string& patch_ver,
                       const std::string& patch_name,
                       const std::string& patch_author,
                       const std::string& patch_note) 
    {
        u64 hash_ret = patch_hash_calc(patch_title, patch_name, patch_app_ver, patch_file, patch_app_elf);
        std::string hash_id = fmt::format("{:#016x}", hash_ret);
        std::string hash_file = fmt::format("{}/patches/settings/{}.txt", patch_path_base, hash_id);

        std::string content = readFromFile(hash_file);
        if (content.empty()) {
            log_warn("File %s not found, initializing false", hash_file.c_str());
            writeToFile(hash_file, "0\n");
            content = "0\n";
        }

        bool cfg_state = (content[0] == '1');

        tmp->patcher_app_ver.push_back(patch_app_ver);
        tmp->patcher_app_elf.push_back(patch_app_elf);
        tmp->patcher_title.push_back(patch_title);
        tmp->patcher_patch_ver.push_back(patch_ver);
        tmp->patcher_name.push_back(patch_name);
        tmp->patcher_author.push_back(patch_author);
        tmp->patcher_note.push_back(patch_note);
        tmp->patcher_enablement.push_back(cfg_state);
    }

    void dl_patches() {
        std::string patch_zip = "patch1.zip";
        std::string patch_url = "https://github.com/GoldHEN/GoldHEN_Patch_Repository/raw/gh-pages/" + patch_zip;
        std::string patch_path = patch_path_base + "/" + patch_zip;
        std::string patch_build_path = patch_path_base + "/patches/misc/patch_ver.txt";
        
        loadmsg(getLangSTR(LANG_STR::PATCH_DL_QUESTION_YES));
        
        int ret = dl_from_url(patch_url.c_str(), patch_path.c_str());
        if (ret != 0) {
            log_error("dl_from_url for %s failed with %i", patch_url.c_str(), ret);
            msgok(MSG_DIALOG::NORMAL, fmt::format("Download failed with error code: {}", ret));
            return;
        }

        if (!extract_zip(patch_path.c_str(), patch_path_base.c_str())) {
            msgok(MSG_DIALOG::NORMAL, fmt::format("Failed to extract patch zip to {}", patch_path_base));
            return;
        }

        std::string patch_ver = readFromFile(patch_build_path);
        std::string patch_dl_msg = fmt::format("{}\n{}", getLangSTR(LANG_STR::PATCH_DL_COMPLETE), patch_ver);
        
        sceMsgDialogTerminate();
        msgok(MSG_DIALOG::NORMAL, patch_dl_msg);
    }
};

// For backwards compability 
PatchManager g_patchManager;


u64 patch_hash_calc(const std::string& title, const std::string& name, const std::string& app_ver, 
                    const std::string& title_id, const std::string& name_elf) {
    return g_patchManager.patch_hash_calc(title, name, app_ver, title_id, name_elf);
}

void write_enable(const std::string& cfg_file, const std::string& patch_name) {
    g_patchManager.write_enable(cfg_file, patch_name);
}

bool get_patch_path(const std::string& path) {
    return g_patchManager.get_patch_path(path);
}

void get_metadata1(struct _trainer_struct *tmp,
                   const std::string& patch_app_ver,
                   const std::string& patch_app_elf,
                   const std::string& patch_title,
                   const std::string& patch_ver,
                   const std::string& patch_name,
                   const std::string& patch_author,
                   const std::string& patch_note) 
{
    g_patchManager.get_metadata1(tmp, patch_app_ver, patch_app_elf, patch_title, patch_ver, patch_name, patch_author, patch_note);
}

void dl_patches() {
    g_patchManager.dl_patches();
}

// We don't need it for now
std::string get_patch_data(const std::string& path) { return ""; }
bool get_metadata0(s32& pid, u64 ex_start) { return false; }
void trainer_launcher() {}