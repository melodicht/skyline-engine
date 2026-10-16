#include "sdl_input.h"

void GatherInputsFromEvent(const InputSpecification& specification, const SDL_Event& e, std::span<PlayerData> inputs) {
    switch (e.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            for (const auto& behavior : specification.GetButtonGatherSpecification(ButtonGatherType::KeyBoardStroke)) {
                if (e.key.key == behavior.m_type.m_keycode) {
                    auto playerIdx = behavior.m_playerIdx;
                    ASSERT_LOG( playerIdx < inputs.size() 
                        && behavior.m_buttonIdx < specification.GetButtonSpecCount(), 
                        "PlayerData has OOB idx");
                    inputs[playerIdx].m_inputButtonDown[behavior.m_buttonIdx] = e.type == SDL_EVENT_KEY_DOWN;
                }
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            for (const auto& behavior : specification.GetButtonGatherSpecification(ButtonGatherType::MouseClick)) {
                if (e.button.button == behavior.m_type.m_mouseClickIdx) {
                    auto playerIdx = behavior.m_playerIdx;
                    ASSERT_LOG( playerIdx < inputs.size() 
                        && behavior.m_buttonIdx < specification.GetButtonSpecCount(), 
                        "PlayerData has OOB idx");
                    inputs[playerIdx].m_inputButtonDown[behavior.m_buttonIdx] = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                }
            }
            break;
    }
}

void GatherInputs(const InputSpecification& specification, std::span<PlayerData> inputs) {
    auto locations = specification.GetInput2dGatherSpecification(Input2dGatherType::MouseLocation);
    if (!locations.empty()) {
        glm::vec2 loc{ };
        SDL_GetMouseState(&loc.x, &loc.y);
        for (const auto& behavior : locations) {
            auto playerIdx = behavior.m_playerIdx;
            ASSERT_PRINT( playerIdx < inputs.size() 
                && behavior.m_buttonIdx < specification.GetInput2dSpecCount(), 
                "Invalid behavior locations");
            inputs[playerIdx].m_input2ds[behavior.m_buttonIdx] = loc;
        }
    }

    auto deltas = specification.GetInput2dGatherSpecification(Input2dGatherType::MouseDelta);
    if (!deltas.empty()) {
        glm::vec2 delta{ };
        SDL_GetRelativeMouseState(&delta.x, &delta.y);
        for (const auto& behavior : deltas) {
            auto playerIdx = behavior.m_playerIdx;
            ASSERT_PRINT( playerIdx < inputs.size() 
                && behavior.m_buttonIdx < specification.GetInput2dSpecCount(), 
                "Invalid behavior locations");
            inputs[playerIdx].m_input2ds[behavior.m_buttonIdx] = delta;
        }
    }
}
