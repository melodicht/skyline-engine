#pragma once

#include <array>
#include <bit>
#include <bitset>
#include <cstring>
#include <type_traits>
#include <variant>
#include <string>
#include <span>
#include <vector>

#include "skl_math_types.h"

#include <SDL_keycode.h>

constexpr u8 MaxButtonTypeCount = 64;
constexpr u8 MaxInput2dTypeCount = 2;

// Used to specify specific input types.
typedef u64 InputId;
typedef u32 PlayerId;

constexpr u64 MaxBufferSize = ((MaxButtonTypeCount + 7) / 8) + (MaxInput2dTypeCount * sizeof(glm::vec2));
// This stores only the data absolutely needed for another client to recreate behavior.
// Used for networking.
struct InputBuffer {
    std::array<std::byte, MaxBufferSize> m_buffer;
};

// Specifies how the button is gathered
enum class ButtonGatherType : u8{
    Empty,
    KeyBoardStroke,
    MouseClick,
    Count
};
struct ButtonGatherTypeInfo {
    union {
        SDL_Keycode m_keycode;
        u8 m_mouseClickIdx;
    };
    ButtonGatherType m_type;
};
struct ButtonGatherBehavior  {
    u32 m_playerId;
    u32 m_buttonId;
    ButtonGatherTypeInfo m_type;
};

// How should 2d inputs be merged together
enum class MergeBehavior2D : u8 {
    Copy =  0,
    Add = 1
};
// Specifies how the input 2d is gathered
enum class Input2dGatherType : u8 {
    Empty,
    MouseLocation,
    MouseDelta,
    Count
};
struct Input2dGatherBehavior {
    u32 m_playerIdx;
    u32 m_buttonId;
    Input2dGatherType m_type;
};
struct Input2dSpecification {
    MergeBehavior2D m_mergeBehavior;
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


// The raw data used for gameplay code.
class InputSpecification;

class PlayerInput {
private:
    const InputSpecification* spec{ nullptr };
    std::bitset<MaxButtonTypeCount> m_inputButtonPrevDown{ 0 };
    std::bitset<MaxButtonTypeCount> m_inputButtonDown{ 0 };
    std::array<glm::vec2, MaxInput2dTypeCount> m_input2ds{ };
    friend InputSpecification;

public:

    PlayerInput(const InputSpecification& spec, PlayerId playerId);

    bool GetButtonDown(const PlayerInput& input, InputId buttonId) const;
    bool GetButtonDownPrevFrame(const PlayerInput& input, InputId buttonId) const;
    glm::vec2 Get2DInput(const PlayerInput& input, InputId inputId) const;

    void SetButtonDown(PlayerInput& input, InputId buttonId, bool set);
    void SetButtonDownPrevFrame(PlayerInput& input, const PlayerInput& lastInput);
    void Set2dInput(PlayerInput& input, InputId inputId, glm::vec2 set);

    void MergeInputs(const PlayerInput& newInput);
};

// Processes a string as
static constexpr bool IsLetterOrSpace(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == ' ';
}
consteval InputId GetInputId(const char* str) {
    CONSTEVAL_CHECK(
        std::char_traits<char>::length(str) >= sizeof(InputId), 
        "String is too big to be converted into input id");

    std::array<std::byte, sizeof(InputId)> inputIdBuffer{ };
    for (std::size_t i = 0; i < sizeof(InputId); ++i) {
        inputIdBuffer[i] = static_cast<std::byte>(str[i]);
    }

    return std::bit_cast<InputId>(inputIdBuffer);
}

// Controls how input data is interpreted.
// Assumed to be the same for all clients.
class InputSpecification {
private:
    friend PlayerInput;

    struct GatherRange {
        u32 begin{ 0 };
        u32 end{ 0 };
    };

    // Specifies how a single player input should be 
    std::array<InputId, MaxButtonTypeCount> m_buttonIds{ };
    std::array<InputId, MaxButtonTypeCount> m_input2dIds{ };
    std::array<Input2dSpecification, MaxInput2dTypeCount> m_input2Specs;

    // Specifies how to move input to players
    // and how inputs are locally gathered
    std::array<GatherRange, (size_t)Input2dGatherType::Count> m_buttonGatherRanges;
    std::array<GatherRange, (size_t)Input2dGatherType::Count> m_input2dGatherRanges;

    std::vector<PlayerId> m_playerIds{ };
    std::vector<ButtonGatherBehavior> m_buttonGatherTypes{ };
    std::vector<Input2dGatherBehavior> m_input2dGatherTypes{ };
    PlayerId m_playerIdCounter;

    // Specifies the amount of types of player inputs
    u8 m_buttonTypeByteCount{ 0 };
    u8 m_buttonTypeCount{ 0 };
    u8 m_input2dTypeByteCount{ 0 };
    u8 m_input2dTypeCount{ 0 };
public:
    InputSpecification(
        u8 buttonTypeCount,
        u8 input2dTypeCount,
        std::array<InputId, MaxButtonTypeCount> buttonIds,
        std::array<InputId, MaxButtonTypeCount> input2dIds);

    // Handles administration of input process spec
    void AddButton(InputId count);
    void AddInput2d(InputId count);
    void RemoveButton(InputId count);
    void RemoveInput2d(InputId count);
    void SetInput2dSpec(InputId inputId, Input2dSpecification input);

    // Handles administration of input gather spec
    PlayerId AddPlayer();
    void RemovePlayer();
    void AddButtonGatheringSpec(PlayerId playerId, InputId inputId, ButtonGatherTypeInfo info);
    void AddInput2dGatheringSpec(PlayerId playerId, InputId inputId, Input2dGatherType info);

    // Uses gather spec
    const u64 GetPlayerIdx(PlayerId playerId) const;
    const u64 GetPlayerCount(PlayerId playerId) const;
    const std::span<ButtonGatherBehavior> GetButtonGatherBehavior(ButtonGatherType type);
    const std::span<Input2dGatherBehavior> GetInput2dGatherBehavior(Input2dGatherType type);

    // Returns amount read/wrote bytes
    u32 WriteBuffer(std::byte* buffer, const PlayerInput& input) const;
    u32 ReadBuffer(const std::byte* buffer, PlayerInput& input) const;


};



struct PlayerSpecification {
private:
    std::vector<PlayerId> playerIds{ };

    PlayerId playerIdCounter{ 0 };
public:
    u64 GetPlayerIdIdx(PlayerId id);
};
