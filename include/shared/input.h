#pragma once

#include <array>
#include <bitset>
#include <cstring>
#include <type_traits>
#include <variant>

#include "skl_math_types.h"

#include <SDL_keycode.h>

constexpr u8 MaxButtonTypeCount = 64;
constexpr u8 MaxInput2dTypeCount = 2;

// Used to specify specific input types.
typedef u16 InputTypeID;

constexpr u64 MaxBufferSize = ((MaxButtonTypeCount + 7) / 8) + (MaxInput2dTypeCount * sizeof(MaxInput2dTypeCount));
// This stores only the data absolutely needed for another client to recreate behavior.
// Used for networking.
struct InputBuffer {
    std::array<std::byte, MaxBufferSize> m_buffer;
};

// Specifies how the button is gathered
struct KeyBoardStroke {SDL_Keycode m_keycode{ 0 }; };
struct MouseClick {u8 m_clickIndex{ 0 }; };
typedef std::variant<KeyBoardStroke, MouseClick> ButtonInputType;
// Specifies a single type of button input
struct ButtonSpecification {
    ButtonInputType inputType;
};

// How should 2d inputs be merged together
enum class MergeBehavior2D : u8 {
    Copy =  0,
    Add = 1
};
// Specifies how the input 2d is gathered
enum class Input2DType : u8 {
    MouseLocation,
    MouseDelta
};


template <std::size_t N>
static void BitsetToBuffer(const std::bitset<N>& bs, std::byte* out, u8 bytes) {
    static_assert(std::is_trivially_copyable_v<std::bitset<N>>);
    std::memcpy(out, &bs, bytes);
}

template <std::size_t N>
static void BufferToBitset(std::bitset<N>& out, const std::byte* in, u8 bytes) {
    static_assert(std::is_trivially_copyable_v<std::bitset<N>>);
    std::memcpy(&out, in, bytes);
}

// Controls how input data is interpreted.
// Assumed to be the same for all clients.
class InputSpecification {
public:
    std::array<ButtonSpecification,MaxButtonTypeCount> m_buttonSpecification{ };
    std::array<MergeBehavior2D, MaxInput2dTypeCount> m_input2dMergeBehaviors{ };
    std::array<Input2DType, MaxInput2dTypeCount> m_input2dTypes{ };

    InputTypeID m_buttonBaseId{ 0 };
    InputTypeID m_input2DBaseId{ 0 };

    u8 m_buttonTypeByteCount{ 0 };
    u8 m_buttonTypeCount{ 0 };
    u8 m_input2dTypeByteCount{ 0 };
    u8 m_input2dTypeCount{ 0 };

    // The raw data used for gameplay code.
    class PlayerInput {
    private:
        std::bitset<MaxButtonTypeCount> inputButtonPrevDown{ 0 };
        std::bitset<MaxButtonTypeCount> inputButtonDown{ 0 };
        std::array<glm::vec2, MaxInput2dTypeCount> input2ds{ };
        
        friend InputSpecification;
    };

    InputSpecification(
        u8 buttonTypeCount,
        InputTypeID buttonBaseId,
        std::array<ButtonSpecification,MaxButtonTypeCount> buttonSpecification,
        u8 input2dTypeCount,
        InputTypeID input2DBaseId,
        std::array<MergeBehavior2D, MaxInput2dTypeCount> input2dMergeBehaviors,
        std::array<Input2DType, MaxInput2dTypeCount> input2dTypes);

    bool GetButtonDown(const PlayerInput& input, InputTypeID buttonId) const;
    bool GetButtonDownPrevFrame(const PlayerInput& input, InputTypeID buttonId) const;
    glm::vec2 Get2DInput(const PlayerInput& input, InputTypeID inputId) const;

    void SetButtonDown(PlayerInput& input, InputTypeID buttonId, bool set) const;
    void SetButtonDownPrevFrame(PlayerInput& input, const PlayerInput& lastInput) const;
    void Set2dInput(PlayerInput& input, InputTypeID inputId, glm::vec2 set) const;

    void MergeInputs(PlayerInput& oldInput, const PlayerInput& newInput) const;

    // Returns amount read/wrote bytes
    u32 WriteBuffer(std::byte* buffer, const PlayerInput& input) const;
    u32 ReadBuffer(const std::byte* buffer, PlayerInput& input) const;
};
