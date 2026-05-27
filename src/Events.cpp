#include "Events.h"
#include "DelayedDispatcher.h"
#include "Settings.h"
#include <cmath>

void UpdateNPCDirectionalState(RE::Actor* a_npc) {
    if (!a_npc || a_npc->IsDead()) return;

    int directionalState = 0; // 0 = Parado por padrão

    if (a_npc->IsMoving()) {
        float velocityX = 0.0f;
        float velocityY = 0.0f;

        auto* charController = a_npc->GetCharController();
        if (charController) {
            velocityX = charController->outVelocity.quad.m128_f32[0];
            velocityY = charController->outVelocity.quad.m128_f32[1];
        }

        // 1. Calcula o ângulo da velocidade no espaço do mundo usando atan2
        // Em Skyrim, Y é o eixo frontal-traseiro e X é o lateral.
        float moveAngleWorld = std::atan2(velocityX, velocityY) * (180.0f / 3.14159265358979323846f);

        // 2. Pega a rotação atual do NPC (para onde o modelo está olhando)
        float npcRotationZ = a_npc->GetAngleZ() * (180.0f / 3.14159265358979323846f);

        // 3. Subtrai a rotação do NPC para obter a direção Local (para onde ele anda em relação à própria frente)
        float angle = moveAngleWorld - npcRotationZ;

        // 4. Normaliza o ângulo para garantir que fique sempre entre 0 e 360 graus
        while (angle < 0.0f) angle += 360.0f;
        while (angle >= 360.0f) angle -= 360.0f;

        SKSE::log::info("[NPC Direcional] NPC: {:08X} | VelX: {:.2f} | VelY: {:.2f} | Angulo Local Calculado: {:.2f}",
            a_npc->GetFormID(), velocityX, velocityY, angle);

        // Mapeia os ângulos convertidos para o seu sistema de Movesets (1 a 8)
        if (angle > 337.5f || angle <= 22.5f) {
            directionalState = 1;  // Norte (Frente)
        }
        else if (angle > 22.5f && angle <= 67.5f) {
            directionalState = 2;  // Nordeste
        }
        else if (angle > 67.5f && angle <= 112.5f) {
            directionalState = 3;  // Leste (Direita)
        }
        else if (angle > 112.5f && angle <= 157.5f) {
            directionalState = 4;  // Sudeste
        }
        else if (angle > 157.5f && angle <= 202.5f) {
            directionalState = 5;  // Sul (Trás)
        }
        else if (angle > 202.5f && angle <= 247.5f) {
            directionalState = 6;  // Sudoeste
        }
        else if (angle > 247.5f && angle <= 292.5f) {
            directionalState = 7;  // Oeste (Esquerda)
        }
        else if (angle > 292.5f && angle <= 337.5f) {
            directionalState = 8;  // Noroeste
        }
    }
    else {
        // Se a velocidade ou a intenção de movimento for zero
        SKSE::log::info("[NPC Direcional] NPC: {:08X} | Estado: PARADO (IsMoving == false)", a_npc->GetFormID());
    }

    // Aplica na Animation Graph do NPC
    a_npc->SetGraphVariableInt("DirecionalCycleMoveset", directionalState);
}

void Sink::UpdateRegisteredHotkeys() {
    auto* controlMap = RE::ControlMap::GetSingleton();
    const auto* userEvents = RE::UserEvents::GetSingleton();

    if (controlMap && userEvents) {
        // Pega as teclas de movimento mapeadas no teclado pelo jogador
        keyForward = controlMap->GetMappedKey(userEvents->forward, RE::INPUT_DEVICE::kKeyboard);
        keyBack = controlMap->GetMappedKey(userEvents->back, RE::INPUT_DEVICE::kKeyboard);
        keyLeft = controlMap->GetMappedKey(userEvents->strafeLeft, RE::INPUT_DEVICE::kKeyboard);
        keyRight = controlMap->GetMappedKey(userEvents->strafeRight, RE::INPUT_DEVICE::kKeyboard);

        // Opcional: Logar as teclas para confirmar se pegou os Scan Codes corretos
        SKSE::log::info("Teclas de Movimento - Frente: {}, Tras: {}, Esquerda: {}, Direita: {}", Sink::keyForward, Sink::keyBack, Sink::keyLeft, Sink::keyRight);
    }

}

RE::BSEventNotifyControl Sink::InputListener::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource)
{
    if (!a_event || !*a_event) {
        return RE::BSEventNotifyControl::kContinue;
    }

    bool umaTeclaMudou = false;

    for (auto* event = *a_event; event; event = event->next) {
        RE::INPUT_DEVICE device = event->GetDevice();

        // --- LÓGICA DE MOVIMENTO (TECLADO E CONTROLE) ---
        if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick) {
            auto* thumbstick = event->AsThumbstickEvent();
            if (thumbstick && thumbstick->IsLeft()) {
                // Normalizamos os valores para evitar pequenas flutuações do analógico
                bool new_c_up = thumbstick->yValue > 0.5f;
                bool new_c_down = thumbstick->yValue < -0.5f;
                bool new_c_left = thumbstick->xValue < -0.5f;
                bool new_c_right = thumbstick->xValue > 0.5f;

                if (c_up != new_c_up || c_down != new_c_down || c_left != new_c_left || c_right != new_c_right) {
                    c_up = new_c_up;
                    c_down = new_c_down;
                    c_left = new_c_left;
                    c_right = new_c_right;
                    umaTeclaMudou = true;
                }
            }
            else if (thumbstick && thumbstick->IsRight()) {
                bool new_rs_up = thumbstick->yValue > 0.5f;
                bool new_rs_down = thumbstick->yValue < -0.5f;
                bool new_rs_left = thumbstick->xValue < -0.5f;
                bool new_rs_right = thumbstick->xValue > 0.5f;

                if (rs_up != new_rs_up || rs_down != new_rs_down || rs_left != new_rs_left || rs_right != new_rs_right) {
                    rs_up = new_rs_up; rs_down = new_rs_down; rs_left = new_rs_left; rs_right = new_rs_right;
                    umaTeclaMudou = true;
                }

                if (std::abs(thumbstick->xValue) > 0.1f || std::abs(thumbstick->yValue) > 0.1f) {
                    mouseCamX = 0.0f;
                    mouseCamY = 0.0f;
                    if (m_up || m_down || m_left || m_right) {
                        m_up = false; m_down = false; m_left = false; m_right = false;
                        umaTeclaMudou = true;
                    }
                }
            }
        }
        else if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kMouseMove) {
            auto* mouseEvent = static_cast<RE::MouseMoveEvent*>(event);

            // 1. Sensibilidade reduzida: Exige mais arrasto físico do mouse.
            // Mude de 0.4f para 0.2f se ainda estiver muito rápido, ou 0.6f se ficar muito pesado.
            float mouseSensitivity = 0.4f;

            mouseCamX += mouseEvent->mouseInputX * mouseSensitivity;
            mouseCamY -= mouseEvent->mouseInputY * mouseSensitivity;

            // 2. Clamping aumentado (Limite de retenção).
            // Agora o valor vai até 300. Isso cria um "peso" maior para reverter a direção.
            float clampLimit = 300.0f;

            if (mouseCamX > clampLimit) mouseCamX = clampLimit;
            if (mouseCamX < -clampLimit) mouseCamX = -clampLimit;
            if (mouseCamY > clampLimit) mouseCamY = clampLimit;
            if (mouseCamY < -clampLimit) mouseCamY = -clampLimit;

            // 3. Deadzone aumentada: O movimento só é validado após passar de 150.
            // Elimina esbarrões e movimentos curtos acidentais.
            float deadzone = 150.0f;

            bool new_m_up = mouseCamY > deadzone;
            bool new_m_down = mouseCamY < -deadzone;
            bool new_m_left = mouseCamX < -deadzone;
            bool new_m_right = mouseCamX > deadzone;

            if (m_up != new_m_up || m_down != new_m_down || m_left != new_m_left || m_right != new_m_right) {
                m_up = new_m_up;
                m_down = new_m_down;
                m_left = new_m_left;
                m_right = new_m_right;
                umaTeclaMudou = true;
            }
        }
        else if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton) {
            auto* button = event->AsButtonEvent();
            if ((button->IsDown() || button->IsUp())) {
                auto CheckOSKey = [](std::uint32_t dik_code) -> bool {
                    UINT vk = MapVirtualKeyA(dik_code, 3 /* MAPVK_VSC_TO_VK_EX */);
                    if (vk == 0) return false;
                    return (GetAsyncKeyState(vk) & 0x8000) != 0;
                    };
                const auto* userEvents = RE::UserEvents::GetSingleton();
                const auto& userEventStr = button->GetUserEvent();
                bool isPressed = button->IsPressed();

                bool new_w = w_pressed;
                bool new_s = s_pressed;
                bool new_a = a_pressed;
                bool new_d = d_pressed;

                if (userEvents) {
                    if (userEventStr == userEvents->forward) {
                            new_w = isPressed;
                    }
                    else if (userEventStr == userEvents->back) {
                            new_s = isPressed;
                    }
                    else if (userEventStr == userEvents->strafeLeft) {
                            new_a = isPressed;
                    }
                    else if (userEventStr == userEvents->strafeRight) {
                            new_d = isPressed;
                    }
                }

                bool new_ls = CheckOSKey(0x2A); // Left Shift
                bool new_q = CheckOSKey(0x10); // Q
                bool new_e = CheckOSKey(0x12); // E
                bool new_la = CheckOSKey(0x38); // Left Alt
                bool new_z = CheckOSKey(0x2C); // Z
                bool new_x = CheckOSKey(0x2D); // X

                if (w_pressed != new_w || s_pressed != new_s || a_pressed != new_a || d_pressed != new_d ||
                    ls_pressed != new_ls || q_pressed != new_q || e_pressed != new_e ||
                    la_pressed != new_la || z_pressed != new_z || x_pressed != new_x)
                {
                    w_pressed = new_w;
                    s_pressed = new_s;
                    a_pressed = new_a;
                    d_pressed = new_d;

                    ls_pressed = new_ls;
                    q_pressed = new_q;
                    e_pressed = new_e;
                    la_pressed = new_la;
                    z_pressed = new_z;
                    x_pressed = new_x;

                    umaTeclaMudou = true;
                }
            }
        }
    } 

    if (umaTeclaMudou) {
        UpdateDirectionalState();
    }

    return RE::BSEventNotifyControl::kContinue;
}

void Sink::InputListener::UpdateDirectionalState()
{
    //static int DirecionalCycleMoveset = 0;
    int directionalState = 0;

    // Prioriza o input do teclado. Se qualquer tecla WASD estiver pressionada, ignore o controle.
    // Caso contrário, use o estado do controle.
    bool is_w = w_pressed || vw_pressed;
    bool is_a = a_pressed || va_pressed;
    bool is_s = s_pressed || vs_pressed;
    bool is_d = d_pressed || vd_pressed;

    // Lógica ajustada para usar as novas booleanas mescladas
    bool FRENTE = is_w || (!is_w && !is_a && !is_s && !is_d && c_up);
    bool TRAS = is_s || (!is_w && !is_a && !is_s && !is_d && c_down);
    bool ESQUERDA = is_a || (!is_w && !is_a && !is_s && !is_d && c_left);
    bool DIREITA = is_d || (!is_w && !is_a && !is_s && !is_d && c_right);

    bool out_ls = ls_pressed || rs_left; 
    bool out_la = la_pressed || rs_right; 
    bool out_q = q_pressed || rs_down; 
    bool out_e = e_pressed || rs_up; 
    bool out_z = z_pressed;  
    bool out_x = x_pressed;  

    // A lógica de decisão permanece a mesma, mas agora usa as variáveis combinadas
    // 1º Prioridade: 3 Teclas pressionadas simultaneamente
    if (ESQUERDA && FRENTE && DIREITA) {
        directionalState = 11;
    }
    else if (TRAS && FRENTE && DIREITA) {
        directionalState = 12;
    }
    else if (ESQUERDA && TRAS && DIREITA) {
        directionalState = 13;
    }
    else if (TRAS && FRENTE && ESQUERDA) {
        directionalState = 14; 
    }
    // 2º Prioridade: 2 Teclas OPOSTAS pressionadas simultaneamente
    else if (FRENTE && TRAS) {
        directionalState = 9;
    }
    else if (ESQUERDA && DIREITA) {
        directionalState = 10;
    }

    else if (FRENTE && ESQUERDA) {
        directionalState = 8;  // Noroeste
    }
    else if (FRENTE && DIREITA) {
        directionalState = 2;  // Nordeste
    }
    else if (TRAS && ESQUERDA) {
        directionalState = 6;  // Sudoeste
    }
    else if (TRAS && DIREITA) {
        directionalState = 4;  // Sudeste
    }
    else if (FRENTE) {
        directionalState = 1;  // Norte (Frente)
    }
    else if (ESQUERDA) {
        directionalState = 7;  // Oeste (Esquerda)
    }
    else if (TRAS) {
        directionalState = 5;  // Sul (Trás)
    }
    else if (DIREITA) {
        directionalState = 3;  // Leste (Direita)
    }
    else {
        directionalState = 0;  // Parado
    }

    // --- MOVIMENTO DE CÂMERA (CameraMovementCMF) ---
    // Mesclamos o input do Analógico Direito com o nosso Mouse Virtual
    bool CAM_FRENTE = rs_up || m_up;
    bool CAM_TRAS = rs_down || m_down;
    bool CAM_ESQUERDA = rs_left || m_left;
    bool CAM_DIREITA = rs_right || m_right;

    int cameraMovementCMF = 0;

    if (CAM_FRENTE && CAM_ESQUERDA) cameraMovementCMF = 8;
    else if (CAM_FRENTE && CAM_DIREITA) cameraMovementCMF = 2;
    else if (CAM_TRAS && CAM_ESQUERDA) cameraMovementCMF = 6;
    else if (CAM_TRAS && CAM_DIREITA) cameraMovementCMF = 4;
    else if (CAM_FRENTE) cameraMovementCMF = 1;
    else if (CAM_ESQUERDA) cameraMovementCMF = 7;
    else if (CAM_TRAS) cameraMovementCMF = 5;
    else if (CAM_DIREITA) cameraMovementCMF = 3;
    else cameraMovementCMF = 0; 

    if(cameraMovementCMF == 0) {
        mouseCamX = 0.0f;
        mouseCamY = 0.0f;
	}

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player) {
        player->SetGraphVariableInt("DirecionalCycleMoveset", directionalState);
        player->SetGraphVariableInt("CameraMovementCMF", cameraMovementCMF);
        player->SetGraphVariableBool("DMKLeftShift", out_ls);
        player->SetGraphVariableBool("DMKLeftAlt", out_la);
        player->SetGraphVariableBool("DMKQ", out_q);
        player->SetGraphVariableBool("DMKE", out_e);
        player->SetGraphVariableBool("DMKZ", out_z);
        player->SetGraphVariableBool("DMKX", out_x);
    }

}

RE::BSEventNotifyControl Sink::MenuWatcher::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{
    if (a_event && !a_event->opening && a_event->menuName == RE::JournalMenu::MENU_NAME) {
        Sink::UpdateRegisteredHotkeys();
    }

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Sink::TweenInputListener::ProcessEvent(const SKSE::ModCallbackEvent* a_event, RE::BSTEventSource<SKSE::ModCallbackEvent>*)
{
    if (!a_event) return RE::BSEventNotifyControl::kContinue;

    std::string_view eventName = a_event->eventName.c_str();

    // strArg contém a string enviada pela sua função SendActionTriggeredEvent (ex: "Forward")
    std::string_view actionName = a_event->strArg.c_str();

    bool mudouAlgo = false;

    if (eventName == "InputManager_ActionTriggered") {
        //if (actionName == "Forward") { vw_pressed = true; mudouAlgo = true; }
        //else if (actionName == "Back") { vs_pressed = true; mudouAlgo = true; }
        //else if (actionName == "Left") { va_pressed = true; mudouAlgo = true; }
        //else if (actionName == "Right") { vd_pressed = true; mudouAlgo = true; }
    }
    else if (eventName == "InputManager_ActionReleased") {
        //if (actionName == "Forward") { vw_pressed = false; mudouAlgo = true; }
        //else if (actionName == "Back") { vs_pressed = false; mudouAlgo = true; }
        //else if (actionName == "Left") { va_pressed = false; mudouAlgo = true; }
        //else if (actionName == "Right") { vd_pressed = false; mudouAlgo = true; }
    }

    if (mudouAlgo) {
        // Se uma tecla virtual foi apertada, pedimos para o InputListener principal recalcular o direcional
        InputListener::GetSingleton()->ForceDirectionalUpdate();
    }

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Sink::NpcCycleSink::ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*)
{
    if (!a_event || !a_event->holder) return RE::BSEventNotifyControl::kContinue;

    auto* actor = a_event->holder->As<RE::Actor>();
    if (!actor || actor->IsDead()) return RE::BSEventNotifyControl::kContinue;

    const std::string_view eventName = a_event->tag;
    auto npc = const_cast<RE::Actor*>(actor);
    bool isPlayer = actor->IsPlayerRef();

    if(!isPlayer) {
		UpdateNPCDirectionalState(npc);
    }
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Sink::NpcCombatTracker::ProcessEvent(const RE::TESCombatEvent* a_event, RE::BSTEventSource<RE::TESCombatEvent>*)
{
    if (!a_event || !a_event->actor) {
        return RE::BSEventNotifyControl::kContinue;
    }

    auto actor = a_event->actor.get();
    auto* npc = actor->As<RE::Actor>();
    if (npc && npc != RE::PlayerCharacter::GetSingleton()) {
        switch (a_event->newState.get()) {
        case RE::ACTOR_COMBAT_STATE::kCombat:
            NpcCombatTracker::RegisterSink(npc);
            break;
        case RE::ACTOR_COMBAT_STATE::kNone:
            if (OARConverterUI::NPCOnlyCombat) {
                NpcCombatTracker::UnregisterSink(npc);
            }
            break;
        }
    }
    return RE::BSEventNotifyControl::kContinue;
}

void Sink::NpcCombatTracker::RegisterSink(RE::Actor* a_actor)
{
    std::unique_lock lock(g_mutex);
    if (g_trackedNPCs.find(a_actor->GetFormID()) == g_trackedNPCs.end()) {
        a_actor->AddAnimationGraphEventSink(&g_npcSink);
        g_trackedNPCs.insert(a_actor->GetFormID());
    }
}

void Sink::NpcCombatTracker::UnregisterSink(RE::Actor* a_actor)
{
    if (!a_actor || a_actor->IsPlayerRef()) return;

    std::unique_lock lock(g_mutex);
    if (g_trackedNPCs.find(a_actor->GetFormID()) != g_trackedNPCs.end()) {
        a_actor->RemoveAnimationGraphEventSink(&g_npcSink);
        g_trackedNPCs.erase(a_actor->GetFormID());
    }
}

void Sink::NpcCombatTracker::RegisterSinksForExistingCombatants()
{
    auto* processLists = RE::ProcessLists::GetSingleton();
    if (!processLists) {
        SKSE::log::warn("[NpcCombatTracker] Não foi possível obter ProcessLists.");
        return;
    }

    // Itera sobre todos os atores que estão "ativos" no jogo
    for (auto& actorHandle : processLists->highActorHandles) {
        if (auto actor = actorHandle.get().get()) {
            // A função IsInCombat() nos diz se o ator já está em um estado de combate
            if (!actor->IsPlayerRef()) {
                if (actor->IsInCombat()) {
                    RegisterSink(actor);
                }
            }

        }
    }
    SKSE::log::info("[NpcCombatTracker] Verificação concluída.");
}

void Sink::ScheduleSinkRegistration(RE::Actor* actor, int attempts)
{
    if (attempts > 20) {
        SKSE::log::critical("[Actor3DLoadEventHandler] Desistindo após {} tentativas para o ator {:08X}.", attempts, actor->GetFormID());
        return;
    }

    std::thread([actor, attempts]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        SKSE::GetTaskInterface()->AddTask([actor, attempts]() {
            if (!actor) return;
            auto ui = RE::UI::GetSingleton();
            if (ui && (ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME))) {
                return;
            }
            RE::BSTSmartPointer<RE::BSAnimationGraphManager> graphManager;
            actor->GetAnimationGraphManager(graphManager);

            if (graphManager) {
                if (!actor->IsPlayerRef()) {
                    Sink::NpcCombatTracker::UnregisterSink(actor);
                    if (!OARConverterUI::NPCOnlyCombat || actor->IsInCombat()) {
                        Sink::NpcCombatTracker::RegisterSink(actor);
                    }
                }
            }
            else {
                ScheduleSinkRegistration(actor, attempts + 1);
            }
            });
        }).detach();
}

RE::BSEventNotifyControl Sink::PC3DLoadEventHandler::ProcessEvent(const RE::TESObjectLoadedEvent* a_event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*)
{
    if (!a_event || !a_event->loaded) {
        return RE::BSEventNotifyControl::kContinue;
    }

    // Em vez de pegar o Player Singleton, buscamos o formulário pelo ID do evento
    auto* form = RE::TESForm::LookupByID(a_event->formID);
    if (!form) return RE::BSEventNotifyControl::kContinue;

    // Tentamos converter para Ator. Se não for ator (ex: uma parede), ignoramos.
    auto* actor = form->As<RE::Actor>();

    if (actor) {
        ScheduleSinkRegistration(actor, 0);
    }

    return RE::BSEventNotifyControl::kContinue;
}
