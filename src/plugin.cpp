#include "logger.h"
#include "Events.h"
#include "Settings.h"
#include "InputManagerAPI.h"

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        //Sink::MenuWatcher::GetSingleton()->Register();
        //Sink::UpdateRegisteredHotkeys();
        OARConverterUI::Register();
        if (GetModuleHandleW(L"TweenPause.dll")) {
            TweenPause = true;
            logger::info("TweenPause.dll founded");
        }
        else {
            TweenPause = false;
            logger::info("TweenPause.dll not found.");
        }
        if (TweenPause) {
            InputManagerAPI::RequestAPIDirect();
            if (InputManagerAPI::_API) {
                logger::info("API do Input Manager conectada");
                Sink::TweenInputListener::Register();
            }
        }
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(Sink::PC3DLoadEventHandler::GetSingleton());
    }
    if (message->type == SKSE::MessagingInterface::kNewGame || message->type == SKSE::MessagingInterface::kPostLoadGame) {
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(Sink::NpcCombatTracker::GetSingleton());
        Sink::NpcCombatTracker::RegisterSinksForExistingCombatants();
        if (auto* inputDeviceManager = RE::BSInputDeviceManager::GetSingleton()) {
            inputDeviceManager->AddEventSink(Sink::InputListener::GetSingleton());
            SKSE::log::info("Listener de input registrado com sucesso!");
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    logger::info("Plugin loaded");
    SKSE::Init(skse);
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
