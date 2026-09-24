#include "SKSEMCP/SKSEMenuFramework.hpp"
#include <array>
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
    inline bool NPCAttackDirectionAtWeaponSwing = false;
    inline int NPCAttackDirectionFallback = 0;
    inline bool DirectionalMode = false;
    inline bool UseInputManagerExtendedKeys = false;
    inline float CameraSensitivity = 0.4f;
    inline float CameraMinimumDistance = 150.0f;
    inline bool EnableCameraAutoReset = false;
    inline float CameraResetDelaySeconds = 0.5f;

    enum class ExtendedKeySlot : std::size_t {
        kLeftShift,
        kLeftCtrl,
        kLeftAlt,
        kQ,
        kE,
        kZ,
        kX,
        kTotal
    };

    inline constexpr std::size_t ExtendedKeyCount = static_cast<std::size_t>(ExtendedKeySlot::kTotal);
    inline std::array<std::vector<int>, ExtendedKeyCount> ExtendedKeyActionIDs;
    inline constexpr std::array<const char*, ExtendedKeyCount> ExtendedKeyIds{
        "LeftShift", "LeftCtrl", "LeftAlt", "Q", "E", "Z", "X"
    };
    inline constexpr std::array<const char*, ExtendedKeyCount> ExtendedKeyLabels{
        "Left Shift", "Left Ctrl", "Left Alt", "Q", "E", "Z", "X"
    };

    inline constexpr const char* actionStateNames[] = { "Ignore", "Tap", "Hold", "Gesture", "Press" };
    inline constexpr std::uint32_t MOUSE_OFFSET = 256;
    inline constexpr std::uint32_t GAMEPAD_OFFSET = 266;

    inline constexpr const char* pcKeyNames[] = {
        "None",
        "Mouse 1 (Left)", "Mouse 2 (Right)", "Mouse 3 (Middle)", "Mouse 4", "Mouse 5", "Mouse 6", "Mouse 7", "Mouse 8",
        "Mouse Wheel Up", "Mouse Wheel Down",
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
        "Minus ( - )", "Equals ( = )", "Bracket Left ( [ )", "Bracket Right ( ] )", "Semicolon ( ; )", "Apostrophe ( ' )", "Tilde ( ~ )", "Backslash ( \\ )", "Comma ( , )", "Period ( . )", "Slash ( / )",
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
        "Esc", "Tab", "Caps Lock", "Shift (Left)", "Shift (Right)", "Ctrl (Left)", "Ctrl (Right)", "Alt (Left)", "Alt (Right)",
        "Space", "Enter", "Backspace", "Print Screen", "Scroll Lock", "Pause", "Num Lock",
        "Up Arrow", "Down Arrow", "Left Arrow", "Right Arrow", "Insert", "Delete", "Home", "End", "Page Up", "Page Down",
        "Num 0", "Num 1", "Num 2", "Num 3", "Num 4", "Num 5", "Num 6", "Num 7", "Num 8", "Num 9",
        "Num +", "Num -", "Num *", "Num /", "Num Enter", "Num Dot"
    };

    inline constexpr int pcKeyIDs[] = {
        0,
        RE::BSWin32MouseDevice::Keys::kLeftButton + MOUSE_OFFSET, RE::BSWin32MouseDevice::Keys::kRightButton + MOUSE_OFFSET,
        RE::BSWin32MouseDevice::Keys::kMiddleButton + MOUSE_OFFSET, RE::BSWin32MouseDevice::Keys::kButton3 + MOUSE_OFFSET,
        RE::BSWin32MouseDevice::Keys::kButton4 + MOUSE_OFFSET, RE::BSWin32MouseDevice::Keys::kButton5 + MOUSE_OFFSET,
        RE::BSWin32MouseDevice::Keys::kButton6 + MOUSE_OFFSET, RE::BSWin32MouseDevice::Keys::kButton7 + MOUSE_OFFSET,
        RE::BSWin32MouseDevice::Keys::kWheelUp + MOUSE_OFFSET, RE::BSWin32MouseDevice::Keys::kWheelDown + MOUSE_OFFSET,
        RE::BSKeyboardDevice::Keys::kA, RE::BSKeyboardDevice::Keys::kB, RE::BSKeyboardDevice::Keys::kC, RE::BSKeyboardDevice::Keys::kD,
        RE::BSKeyboardDevice::Keys::kE, RE::BSKeyboardDevice::Keys::kF, RE::BSKeyboardDevice::Keys::kG, RE::BSKeyboardDevice::Keys::kH,
        RE::BSKeyboardDevice::Keys::kI, RE::BSKeyboardDevice::Keys::kJ, RE::BSKeyboardDevice::Keys::kK, RE::BSKeyboardDevice::Keys::kL,
        RE::BSKeyboardDevice::Keys::kM, RE::BSKeyboardDevice::Keys::kN, RE::BSKeyboardDevice::Keys::kO, RE::BSKeyboardDevice::Keys::kP,
        RE::BSKeyboardDevice::Keys::kQ, RE::BSKeyboardDevice::Keys::kR, RE::BSKeyboardDevice::Keys::kS, RE::BSKeyboardDevice::Keys::kT,
        RE::BSKeyboardDevice::Keys::kU, RE::BSKeyboardDevice::Keys::kV, RE::BSKeyboardDevice::Keys::kW, RE::BSKeyboardDevice::Keys::kX,
        RE::BSKeyboardDevice::Keys::kY, RE::BSKeyboardDevice::Keys::kZ,
        RE::BSKeyboardDevice::Keys::kNum1, RE::BSKeyboardDevice::Keys::kNum2, RE::BSKeyboardDevice::Keys::kNum3, RE::BSKeyboardDevice::Keys::kNum4,
        RE::BSKeyboardDevice::Keys::kNum5, RE::BSKeyboardDevice::Keys::kNum6, RE::BSKeyboardDevice::Keys::kNum7, RE::BSKeyboardDevice::Keys::kNum8,
        RE::BSKeyboardDevice::Keys::kNum9, RE::BSKeyboardDevice::Keys::kNum0,
        RE::BSKeyboardDevice::Keys::kMinus, RE::BSKeyboardDevice::Keys::kEquals, RE::BSKeyboardDevice::Keys::kBracketLeft,
        RE::BSKeyboardDevice::Keys::kBracketRight, RE::BSKeyboardDevice::Keys::kSemicolon, RE::BSKeyboardDevice::Keys::kApostrophe,
        RE::BSKeyboardDevice::Keys::kTilde, RE::BSKeyboardDevice::Keys::kBackslash, RE::BSKeyboardDevice::Keys::kComma,
        RE::BSKeyboardDevice::Keys::kPeriod, RE::BSKeyboardDevice::Keys::kSlash,
        RE::BSKeyboardDevice::Keys::kF1, RE::BSKeyboardDevice::Keys::kF2, RE::BSKeyboardDevice::Keys::kF3, RE::BSKeyboardDevice::Keys::kF4,
        RE::BSKeyboardDevice::Keys::kF5, RE::BSKeyboardDevice::Keys::kF6, RE::BSKeyboardDevice::Keys::kF7, RE::BSKeyboardDevice::Keys::kF8,
        RE::BSKeyboardDevice::Keys::kF9, RE::BSKeyboardDevice::Keys::kF10, RE::BSKeyboardDevice::Keys::kF11, RE::BSKeyboardDevice::Keys::kF12,
        RE::BSKeyboardDevice::Keys::kEscape, RE::BSKeyboardDevice::Keys::kTab, RE::BSKeyboardDevice::Keys::kCapsLock,
        RE::BSKeyboardDevice::Keys::kLeftShift, RE::BSKeyboardDevice::Keys::kRightShift,
        RE::BSKeyboardDevice::Keys::kLeftControl, RE::BSKeyboardDevice::Keys::kRightControl,
        RE::BSKeyboardDevice::Keys::kLeftAlt, RE::BSKeyboardDevice::Keys::kRightAlt,
        RE::BSKeyboardDevice::Keys::kSpacebar, RE::BSKeyboardDevice::Keys::kEnter, RE::BSKeyboardDevice::Keys::kBackspace,
        RE::BSKeyboardDevice::Keys::kPrintScreen, RE::BSKeyboardDevice::Keys::kScrollLock, RE::BSKeyboardDevice::Keys::kPause,
        RE::BSKeyboardDevice::Keys::kNumLock,
        RE::BSKeyboardDevice::Keys::kUp, RE::BSKeyboardDevice::Keys::kDown, RE::BSKeyboardDevice::Keys::kLeft, RE::BSKeyboardDevice::Keys::kRight,
        RE::BSKeyboardDevice::Keys::kInsert, RE::BSKeyboardDevice::Keys::kDelete, RE::BSKeyboardDevice::Keys::kHome, RE::BSKeyboardDevice::Keys::kEnd,
        RE::BSKeyboardDevice::Keys::kPageUp, RE::BSKeyboardDevice::Keys::kPageDown,
        RE::BSKeyboardDevice::Keys::kKP_0, RE::BSKeyboardDevice::Keys::kKP_1, RE::BSKeyboardDevice::Keys::kKP_2, RE::BSKeyboardDevice::Keys::kKP_3,
        RE::BSKeyboardDevice::Keys::kKP_4, RE::BSKeyboardDevice::Keys::kKP_5, RE::BSKeyboardDevice::Keys::kKP_6, RE::BSKeyboardDevice::Keys::kKP_7,
        RE::BSKeyboardDevice::Keys::kKP_8, RE::BSKeyboardDevice::Keys::kKP_9,
        RE::BSKeyboardDevice::Keys::kKP_Plus, RE::BSKeyboardDevice::Keys::kKP_Subtract, RE::BSKeyboardDevice::Keys::kKP_Multiply,
        RE::BSKeyboardDevice::Keys::kKP_Divide, RE::BSKeyboardDevice::Keys::kKP_Enter, RE::BSKeyboardDevice::Keys::kKP_Decimal
    };
    inline constexpr const char* gamepadKeyNames[] = {
        "None",
        "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
        "Start / Options", "Back / Share / Select", "LS / L3 (Left Stick)", "RS / R3 (Right Stick)",
        "LB / L1 (Left Bumper)", "RB / R1 (Right Bumper)",
        "LT / L2 (Left Trigger)", "RT / R2 (Right Trigger)",
        "A / Cross", "B / Circle", "X / Square", "Y / Triangle"
    };
    inline constexpr int gamepadKeyIDs[] = {
        0,
        RE::BSWin32GamepadDevice::Keys::kUp + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kDown + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kLeft + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kRight + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kStart + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kBack + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kLeftThumb + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kRightThumb + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kLeftShoulder + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kRightShoulder + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kLeftTrigger + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kRightTrigger + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kA + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kB + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kX + GAMEPAD_OFFSET,
        RE::BSWin32GamepadDevice::Keys::kY + GAMEPAD_OFFSET
    };
    namespace fs = std::filesystem;

    void LoadLanguage();
    const char* GetLoc(const std::string& key, const char* defaultVal);
    void LoadSettings();
    void SaveSettings();
    void RegisterAllInputs();
    void UnregisterAllInputs();
    void TweenPauseRegister();
    bool HandleTweenPauseControlUpdate(const char* a_payload);

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
