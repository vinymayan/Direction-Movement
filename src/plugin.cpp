#include "logger.h"
#include "Events.h"
#include "Hooks.h"
#include "Settings.h"
#include "InputManagerAPI.h"

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == InputManagerAPI::kMessage_ProvideAPI) {
        InputManagerAPI::ReceiveAPI(message);
        if (InputManagerAPI::_API) {
            logger::info("API do Input Manager recebida via messaging");
            OARConverterUI::RegisterAllInputs();
            OARConverterUI::TweenPauseRegister();
        }
    }

    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        OARConverterUI::Register();
        Hooks::InstallWindowFocusHook();
        InputManagerAPI::RequestAPIDirect();
        if (InputManagerAPI::_API) {
            logger::info("API do Input Manager conectada");
            OARConverterUI::RegisterAllInputs();
            OARConverterUI::TweenPauseRegister();
        }
        else {
            InputManagerAPI::RequestAPI();
        }
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(Sink::PC3DLoadEventHandler::GetSingleton());
    }
    if (message->type == SKSE::MessagingInterface::kNewGame || message->type == SKSE::MessagingInterface::kPostLoadGame) {
        Hooks::InstallWindowFocusHook();
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
    Sink::TweenInputListener::Register();
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
    return true;
}
