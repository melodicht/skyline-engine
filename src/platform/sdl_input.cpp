#include "sdl_input.h"

InputSpecification::PlayerInput GatherPollingSDL(const InputSpecification& specification, const SDL_Event& e, const InputSpecification::PlayerInput& prevInput) {
    InputSpecification::PlayerInput ret = prevInput;
    switch (e.type)
    {
        case SDL_EVENT_KEY_DOWN:
            for (int i = 0 ; i < specification.m_buttonTypeCount ; i++) {
                if (auto* p = std::get_if<KeyBoardStroke>(&specification.m_buttonSpecification[i].inputType)) {
                    if (e.key.key == p->m_keycode) {
                        specification.SetButtonDown(ret, specification.m_buttonBaseId + i, true);
                    }
                }
            }
            break;
        case SDL_EVENT_KEY_UP:
            for (int i = 0 ; i < specification.m_buttonTypeCount ; i++) {
                if (auto* p = std::get_if<KeyBoardStroke>(&specification.m_buttonSpecification[i].inputType)) {
                    if (e.key.key == p->m_keycode) {
                        specification.SetButtonDown(ret, specification.m_buttonBaseId + i, false);
                    }
                }
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            for (int i = 0 ; i < specification.m_buttonTypeCount ; i++) {
                if (auto* p = std::get_if<MouseClick>(&specification.m_buttonSpecification[i].inputType)) {
                    if (p->m_clickIndex == e.button.button) {
                        specification.SetButtonDown(ret, specification.m_buttonBaseId + i, true);
                    }
                }
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            for (int i = 0 ; i < specification.m_buttonTypeCount ; i++) {
                if (auto* p = std::get_if<MouseClick>(&specification.m_buttonSpecification[i].inputType)) {
                    if (p->m_clickIndex == e.button.button) {
                        specification.SetButtonDown(ret, specification.m_buttonBaseId + i, false);
                    }
                }
            }
            break;
    }

    for (int i = 0; i < specification.m_input2dTypeCount; i++) {
        switch (specification.m_input2dTypes[i]) {
            case Input2DType::MouseLocation: {
                glm::vec2 loc{ };
                SDL_GetMouseState(&loc.x, &loc.y);
                specification.Set2dInput(ret, specification.m_input2DBaseId + i, loc);
                break;
            }
            case Input2DType::MouseDelta: {
                glm::vec2 delta{ };
                SDL_GetRelativeMouseState(&delta.x, &delta.y);
                specification.Set2dInput(ret, specification.m_input2DBaseId + i, delta);
                break;
            }
            default:
                //ADD FAULT
                break;
        }
    }
    return ret;
}
