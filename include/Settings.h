#include "SKSEMCP/SKSEMenuFramework.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// RapidJSON headers
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h> 
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>

// Miniz (Zip)
#include <miniz.h>

namespace OARConverterUI {
	inline bool NPCOnlyCombat = true;
    namespace fs = std::filesystem;

    void LoadLanguage();
    const char* GetLoc(const std::string& key, const char* defaultVal);
    void LoadSettings();
    void SaveSettings();

    // Estrutura para segurar o conteúdo do arquivo na memória antes de exportar
    struct ConvertedFile {
        fs::path originalPath;
        std::string modifiedContent;
    };

    inline std::wstring ToLowerW(std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(), ::towlower);
        return s;
    }

    // 1. Extração do Direcional (Retorna 0 se não for Keytrace, ou o número da direção 1, 3, 5, 7)
    struct KeytraceInfo {
        bool isValid = false;
        bool isDirectional = false;
        int dirValue = 0;
        std::string boolVarName = "";
    };

    inline KeytraceInfo ParseKeytraceCondition(const rapidjson::Value& cond) {
        KeytraceInfo info;
        if (!cond.IsObject()) return info;
        if (!cond.HasMember("condition") || !cond["condition"].IsString()) return info;

        std::string conditionType = cond["condition"].GetString();
        if (conditionType != "HasMagicEffect") return info;

        const rapidjson::Value* me = nullptr;
        if (cond.HasMember("Magic Effect")) me = &cond["Magic Effect"];
        else if (cond.HasMember("Magic effect")) me = &cond["Magic effect"];
        else if (cond.HasMember("magicEffect")) me = &cond["magicEffect"];
        else if (cond.HasMember("Effect")) me = &cond["Effect"];
        else if (cond.HasMember("effect")) me = &cond["effect"];

        if (!me || !me->IsObject()) return info;

        if (me->HasMember("pluginName") && (*me)["pluginName"].IsString() && me->HasMember("formID")) {
            std::string pluginName = (*me)["pluginName"].GetString();
            std::transform(pluginName.begin(), pluginName.end(), pluginName.begin(), ::tolower);

            if (pluginName == "keytrace.esp") {
                std::string formID = "";
                if ((*me)["formID"].IsString()) {
                    formID = (*me)["formID"].GetString();
                }
                else if ((*me)["formID"].IsInt()) {
                    char hexStr[20];
                    sprintf_s(hexStr, "%X", (*me)["formID"].GetInt());
                    formID = hexStr;
                }

                // Tudo em minusculo para garantir que acha o Hex corretamente
                std::transform(formID.begin(), formID.end(), formID.begin(), ::tolower);
                info.isValid = true;

                // Direcionais Antigos (Int)
                if (formID.find("801") != std::string::npos) { info.isDirectional = true; info.dirValue = 1; }
                else if (formID.find("803") != std::string::npos) { info.isDirectional = true; info.dirValue = 5; }
                else if (formID.find("802") != std::string::npos) { info.isDirectional = true; info.dirValue = 7; }
                else if (formID.find("804") != std::string::npos) { info.isDirectional = true; info.dirValue = 3; }
                // Novas Teclas (Bools)
                else if (formID.find("81b") != std::string::npos) { info.boolVarName = "DMKLeftShift"; }
                else if (formID.find("81c") != std::string::npos) { info.boolVarName = "DMKQ"; }
                else if (formID.find("81d") != std::string::npos) { info.boolVarName = "DMKE"; }
                else if (formID.find("81e") != std::string::npos) { info.boolVarName = "DMKLeftAlt"; }
                else if (formID.find("821") != std::string::npos) { info.boolVarName = "DMKZ"; }
                else if (formID.find("822") != std::string::npos) { info.boolVarName = "DMKX"; }
                else { info.isValid = false; }
            }
        }
        return info;
    }

    // --- RENDERIZAÇÃO DA UI NO SKSE MENU FRAMEWORK ---
    void RenderMenu();

    void MSettings();
    // Registro do menu
    void Register();
}