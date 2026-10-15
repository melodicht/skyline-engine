#include "sdl_input.h"

void GatherPollingSDL(InputSpecification& specification, const SDL_Event& e, std::span<PlayerInput> inputs) {
    switch (e.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            for (const auto& behavior : specification.GetButtonGatherBehavior(ButtonGatherType::KeyBoardStroke)) {
                if (e.key.key == behavior.m_type.m_keycode) {
                    auto playerIdx = specification.GetPlayerIdx(behavior.m_playerId);
                    ASSERT_PRINT(playerIdx < inputs.size(), "OOB");
                    if (playerIdx < inputs.size()) {
                        inputs[playerIdx].SetButtonDown(inputs[playerIdx], behavior.m_buttonId, e.type == SDL_EVENT_KEY_DOWN);
                    }
                }
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            for (const auto& behavior : specification.GetButtonGatherBehavior(ButtonGatherType::MouseClick)) {
                if (e.button.button == behavior.m_type.m_mouseClickIdx) {
                    auto playerIdx = specification.GetPlayerIdx(behavior.m_playerId);
                    ASSERT_PRINT(playerIdx < inputs.size(), "OOB");
                    if (playerIdx < inputs.size()) {
                        inputs[playerIdx].SetButtonDown(inputs[playerIdx], behavior.m_buttonId, e.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                    }
                }
            }
            break;
    }

    auto locations = specification.GetInput2dGatherBehavior(Input2dGatherType::MouseLocation);
    if (!locations.empty()) {
        glm::vec2 loc{ };
        SDL_GetMouseState(&loc.x, &loc.y);
        for (const auto& behavior : locations) {
            auto playerIdx = behavior.m_playerIdx;
            ASSERT_PRINT(playerIdx < inputs.size(), "OOB");
            if (playerIdx < inputs.size()) {
                inputs[playerIdx].Set2dInput(inputs[playerIdx], behavior.m_buttonId, loc);
            }
        }
    }

    auto deltas = specification.GetInput2dGatherBehavior(Input2dGatherType::MouseDelta);
    if (!deltas.empty()) {
        glm::vec2 delta{ };
        SDL_GetRelativeMouseState(&delta.x, &delta.y);
        for (const auto& behavior : deltas) {
            auto playerIdx = behavior.m_playerIdx;
            ASSERT_PRINT(playerIdx < inputs.size(), "OOB");
            if (playerIdx < inputs.size()) {
                inputs[playerIdx].Set2dInput(inputs[playerIdx], behavior.m_buttonId, delta);
            }
        }
    }
}
