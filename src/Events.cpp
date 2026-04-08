#include "Events.h"

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
        SKSE::log::info("Teclas de Movimento - Frente: {}, Tras: {}, Esquerda: {}, Direita: {}",
            Sink::keyForward, Sink::keyBack, Sink::keyLeft, Sink::keyRight);
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
            }
        }
        else if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton) {
            auto* button = event->AsButtonEvent();
            if (button->IsDown() || button->IsUp()) {
                auto CheckOSKey = [](std::uint32_t dik_code) -> bool {
                    UINT vk = MapVirtualKeyA(dik_code, 3 /* MAPVK_VSC_TO_VK_EX */);
                    if (vk == 0) return false;

                    // Se o bit mais alto (0x8000) for 1, a tecla está fisicamente pressionada agora
                    return (GetAsyncKeyState(vk) & 0x8000) != 0;
                    };

                // Puxamos o estado real, validado direto do hardware
                bool new_w = CheckOSKey(keyForward);
                bool new_s = CheckOSKey(keyBack);
                bool new_a = CheckOSKey(keyLeft);
                bool new_d = CheckOSKey(keyRight);

                bool new_ls = CheckOSKey(0x2A); // Left Shift
                bool new_q = CheckOSKey(0x10); // Q
                bool new_e = CheckOSKey(0x12); // E
                bool new_la = CheckOSKey(0x38); // Left Alt
                bool new_z = CheckOSKey(0x2C); // Z
                bool new_x = CheckOSKey(0x2D); // X

                // Verifica se houve alguma divergência entre as variáveis locais da nossa 
                // state machine e a realidade física atual do teclado.
                if (w_pressed != new_w || s_pressed != new_s || a_pressed != new_a || d_pressed != new_d ||
                    ls_pressed != new_ls || q_pressed != new_q || e_pressed != new_e ||
                    la_pressed != new_la || z_pressed != new_z || x_pressed != new_x)
                {
                    // Sincroniza o estado local com o estado real
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

                    // Como algo mudou, sinalizamos para recalcular o DirectionalState no fim do loop
                    umaTeclaMudou = true;
                }

				
            }
        }
        int previousDirectionalState = directionalState;

        if (umaTeclaMudou) {
            UpdateDirectionalState();
        }

        return RE::BSEventNotifyControl::kContinue;
    }
}

void Sink::InputListener::UpdateDirectionalState()
{
    //static int DirecionalCycleMoveset = 0;
    int VariavelAnterior = directionalState;

    // Prioriza o input do teclado. Se qualquer tecla WASD estiver pressionada, ignore o controle.
    // Caso contrário, use o estado do controle.
    bool FRENTE = w_pressed || (!w_pressed && !a_pressed && !s_pressed && !d_pressed && c_up);
    bool TRAS = s_pressed || (!w_pressed && !a_pressed && !s_pressed && !d_pressed && c_down);
    bool ESQUERDA = a_pressed || (!w_pressed && !a_pressed && !s_pressed && !d_pressed && c_left);
    bool DIREITA = d_pressed || (!w_pressed && !a_pressed && !s_pressed && !d_pressed && c_right);

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

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (player) {
        player->SetGraphVariableInt("DirecionalCycleMoveset", directionalState);
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
