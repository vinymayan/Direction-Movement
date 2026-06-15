#include "Hooks.h"

#include "Events.h"
#include "logger.h"

#include <RE/M/Main.h>
#include <RE/R/Renderer.h>

#include <Windows.h>
#include <CommCtrl.h>

#pragma comment(lib, "Comctl32.lib")

namespace {
    constexpr UINT_PTR FOCUS_SUBCLASS_ID = 1337;

    bool g_focusHookInstalled = false;
    bool g_focusHookQueued = false;

    HWND GetGameWindow()
    {
        if (auto* main = RE::Main::GetSingleton(); main && main->wnd) {
            auto* hwnd = reinterpret_cast<HWND>(main->wnd);
            if (::IsWindow(hwnd)) {
                return hwnd;
            }
        }

        if (auto* renderWindow = RE::BSGraphics::Renderer::GetCurrentRenderWindow(); renderWindow && renderWindow->hWnd) {
            auto* hwnd = reinterpret_cast<HWND>(renderWindow->hWnd);
            if (::IsWindow(hwnd)) {
                return hwnd;
            }
        }

        if (auto* hwnd = ::FindWindowA(nullptr, "Skyrim Special Edition"); hwnd && ::IsWindow(hwnd)) {
            return hwnd;
        }

        return nullptr;
    }

    LRESULT CALLBACK FocusSubclassProc(HWND a_hwnd, UINT a_msg, WPARAM a_wParam, LPARAM a_lParam, UINT_PTR, DWORD_PTR)
    {
        if (a_msg == WM_ACTIVATEAPP && a_wParam == FALSE) {
            logger::info("[DMK] WM_ACTIVATEAPP: Game lost focus. Clearing inputs.");
            Sink::InputListener::GetSingleton()->ResetInputState();
        }

        return ::DefSubclassProc(a_hwnd, a_msg, a_wParam, a_lParam);
    }
}

void Hooks::InstallWindowFocusHook()
{
    if (g_focusHookInstalled || g_focusHookQueued) {
        return;
    }

    auto* taskInterface = SKSE::GetTaskInterface();
    if (!taskInterface) {
        logger::warn("[DMK] Failed to install focus hook: task interface unavailable.");
        return;
    }

    g_focusHookQueued = true;
    taskInterface->AddTask([]() {
        g_focusHookQueued = false;

        if (g_focusHookInstalled) {
            return;
        }

        auto* hwnd = GetGameWindow();
        if (!hwnd) {
            logger::warn("[DMK] Nao foi possivel instalar hook de foco: janela do jogo nao encontrada.");
            return;
        }

        DWORD windowPid = 0;
        ::GetWindowThreadProcessId(hwnd, &windowPid);
        if (windowPid != ::GetCurrentProcessId()) {
            logger::warn("[DMK] Hook de foco ignorado: janela encontrada pertence a outro processo.");
            return;
        }

        if (!::SetWindowSubclass(hwnd, FocusSubclassProc, FOCUS_SUBCLASS_ID, 0)) {
            logger::warn("[DMK] Falha ao instalar hook de foco da janela via subclass.");
            return;
        }

        g_focusHookInstalled = true;
        logger::info("[DMK] Hook de foco da janela instalado via subclass.");
    });
}
