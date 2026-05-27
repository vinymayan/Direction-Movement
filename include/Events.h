#pragma once
#include <shared_mutex>

static bool TweenPause = false;

namespace Sink {

    // Variaveis de estado globais de input (para nao precisar mexer no seu Events.h)
    static bool ls_pressed = false; // Left Shift (0x2A)
    static bool q_pressed = false;  // Q (0x10)
    static bool e_pressed = false;  // E (0x12)
    static bool la_pressed = false; // Left Alt (0x38)
    static bool z_pressed = false;  // Z (0x2C)
    static bool x_pressed = false;  // X (0x2D)

    // Variaveis do Right Thumbstick
    static bool rs_up = false;
    static bool rs_down = false;
    static bool rs_left = false;
    static bool rs_right = false;

    inline uint32_t keyForward = 0x11;
    inline uint32_t keyBack = 0x1F;
    inline uint32_t keyLeft = 0x1E;
    inline uint32_t keyRight = 0x20;

    static bool vw_pressed = false;
    static bool vs_pressed = false;
    static bool va_pressed = false;
    static bool vd_pressed = false;

    static float mouseCamX = 0.0f;
    static float mouseCamY = 0.0f;
    static bool m_up = false;
    static bool m_down = false;
    static bool m_left = false;
    static bool m_right = false;

    class TweenInputListener : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
    public:
        static TweenInputListener* GetSingleton() {
            static TweenInputListener singleton;
            return &singleton;
        }

        static void Register() {
            auto eventSource = SKSE::GetModCallbackEventSource();
            if (eventSource) {
                eventSource->AddEventSink(GetSingleton());
                SKSE::log::info("[Prisma] Listener do Input Manager Registrado e aguardando comandos!");
            }
        }

        RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override;
    };

    class InputListener : public RE::BSTEventSink<RE::InputEvent*> {
    public:
        // Singleton para garantir que exista apenas uma instância
        static InputListener* GetSingleton() {
            static InputListener singleton;
            return &singleton;
        }

        static inline RE::INPUT_DEVICE lastUsedDevice = RE::INPUT_DEVICE::kKeyboard;
        // A função que processa os eventos de input do jogo
        virtual RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,
            RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;
        void ForceDirectionalUpdate() { UpdateDirectionalState(); }
    protected:

    private:
        // Função para calcular a direção com base nas teclas pressionadas
        void UpdateDirectionalState();
        // Variáveis para rastrear o estado de cada tecla de movimento
        bool w_pressed = false;
        bool a_pressed = false;
        bool s_pressed = false;
        bool d_pressed = false;

        // Controle
        bool c_up = false;
        bool c_left = false;
        bool c_down = false;
        bool c_right = false;
    };
    class MenuWatcher : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        static MenuWatcher* GetSingleton()
        {
            static MenuWatcher singleton;
            return &singleton;
        }

        void Register()
        {
            auto ui = RE::UI::GetSingleton();
            if (ui) {
                ui->AddEventSink(this);
                SKSE::log::info("MenuWatcher registrado para monitorar o JournalMenu.");
            }
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;
    };

    void UpdateRegisteredHotkeys();

    static void ScheduleSinkRegistration(RE::Actor* actor, int attempts);


    class NpcCycleSink : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
    public:
        static NpcCycleSink* GetSingleton() {
            static NpcCycleSink singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event,
            RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override;
    };



    class NpcCombatTracker : public RE::BSTEventSink<RE::TESCombatEvent> {
    public:
        static NpcCombatTracker* GetSingleton() {
            static NpcCombatTracker singleton;
            return &singleton;
        }

        // Função chamada quando um evento de combate ocorre
        RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* a_event,
            RE::BSTEventSource<RE::TESCombatEvent>*) override;

        static void RegisterSink(RE::Actor* a_actor);
        static void UnregisterSink(RE::Actor* a_actor);

        static void RegisterSinksForExistingCombatants();

    private:
        // Instância compartilhada do nosso processador de lógica
        inline static NpcCycleSink g_npcSink;

        // Guarda os FormIDs dos NPCs que já estamos ouvindo
        inline static std::set<RE::FormID> g_trackedNPCs;
        inline static std::shared_mutex g_mutex;
    };

    class PC3DLoadEventHandler : public RE::BSTEventSink<RE::TESObjectLoadedEvent> {
    public:
        static PC3DLoadEventHandler* GetSingleton() {
            static PC3DLoadEventHandler singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override;
    };
}