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
        u8 m_emptyPadding{ 0 };
        SDL_Keycode m_keycode;
        u8 m_mouseClickIdx;
    };
    ButtonGatherType m_type{ ButtonGatherType::Empty };
};
struct ButtonGatherSpecification  {
    u8 m_playerIdx{ 0 };
    u8 m_buttonIdx{ 0 };
    ButtonGatherTypeInfo m_type{ 0 };
};

// How should 2d inputs be merged together
enum class MergeBehavior2d : u8 {
    Copy,
    Add
};
// Specifies how the input 2d is gathered
enum class Input2dGatherType : u8 {
    Empty,
    MouseLocation,
    MouseDelta,
    Count
};
struct Input2dGatherSpecification {
    u8 m_playerIdx{ 0 };
    u8 m_buttonIdx{ 0 };
    Input2dGatherType m_type{ Input2dGatherType::Empty };
};
struct Input2dSpecification {
    MergeBehavior2d m_mergeBehavior{ 0 };
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
union SDL_Event;

struct PlayerData {
    std::bitset<MaxButtonTypeCount> m_inputButtonPrevDown{ 0 };
    std::bitset<MaxButtonTypeCount> m_inputButtonDown{ 0 };
    std::array<glm::vec2, MaxInput2dTypeCount> m_input2ds{ };
};

consteval InputId GetInputId(const char* str) {
    CONSTEVAL_CHECK(
        std::char_traits<char>::length(str) <= sizeof(InputId),
        "String is too big to be converted into input id");

    std::array<std::byte, sizeof(InputId)> inputIdBuffer{ };
    for (std::size_t i = 0; i < std::char_traits<char>::length(str); ++i) {
        inputIdBuffer[i] = static_cast<std::byte>(str[i]);
    }

    return std::bit_cast<InputId>(inputIdBuffer);
}

// Controls how input data is interpreted.
// Assumed to be the same for all clients.
class InputSpecification {
private:
    friend class PlayerInput;

    struct GatherRange {
        u32 begin{ 0 };
        u32 end{ 0 };
    };

    // Specifies how a single player input should be 
    std::array<InputId, MaxButtonTypeCount> m_buttonIds{ 0 };
    std::array<InputId, MaxButtonTypeCount> m_input2dIds{ 0 };
    std::array<Input2dSpecification, MaxInput2dTypeCount> m_input2Specs{ };

    // Specifies how to move input to players
    // and how inputs are locally gathered
    std::array<GatherRange, (size_t)ButtonGatherType::Count> m_buttonGatherRanges;
    std::array<GatherRange, (size_t)Input2dGatherType::Count> m_input2dGatherRanges;

    std::vector<PlayerId> m_playerIds{ };
    std::vector<ButtonGatherSpecification> m_buttonGatherTypes{ };
    std::vector<Input2dGatherSpecification> m_input2dGatherTypes{ };

    // A player id of 0 indicates an error
    PlayerId m_playerIdCounter{ 1 };

    // Specifies the amount of types of player inputs
    u8 m_buttonTypeByteCount{ 0 };
    u8 m_buttonTypeCount{ 0 };
    u8 m_input2dTypeByteCount{ 0 };
    u8 m_input2dTypeCount{ 0 };

    void UpdateGatherRanges();
    void GetPlayerIdxInner(PlayerId playerId) const;
public:
    InputSpecification(
        u8 buttonTypeCount,
        u8 input2dTypeCount,
        std::array<InputId, MaxButtonTypeCount> buttonIds,
        std::array<InputId, MaxButtonTypeCount> input2dIds);

    // Handles administration of input process spec]
    u8 GetButtonSpecCount() const;
    u8 GetInput2dSpecCount() const;
    void AddButtonSpec(InputId count);
    void AddInput2dSpec(InputId count);
    void RemoveButtonSpec(InputId count);
    void RemoveInput2dSpec(InputId count);
    void SetInput2dSpec(InputId inputId, Input2dSpecification input);

    // Handles administration of input gather spec
    PlayerId AddPlayer();
    void RemovePlayer(PlayerId playerId);
    void AddButtonGatheringSpec(PlayerId playerId, InputId inputId, ButtonGatherTypeInfo info);
    void AddInput2dGatheringSpec(PlayerId playerId, InputId inputId, Input2dGatherType info);

    // Uses gather spec
    const u64 GetPlayerIdx(PlayerId playerId) const;
    const u64 GetPlayerCount(PlayerId playerId) const;
    const std::span<const ButtonGatherSpecification> GetButtonGatherSpecification(ButtonGatherType type) const;
    const std::span<const Input2dGatherSpecification> GetInput2dGatherSpecification(Input2dGatherType type) const;

    // Returns amount read/wrote bytes
    u32 WriteBuffer(std::byte* buffer, const PlayerData& input) const;
    u32 ReadBuffer(const std::byte* buffer, PlayerData& input) const;
};

// Object exposed to gameplay code.
// Provides interfaces of gameplay
class PlayerInput {
private:
    const InputSpecification& m_inputSpec;
    PlayerData& m_playerData;
public:
    // The specification and data must outlive this view.
    PlayerInput(const InputSpecification& spec, PlayerData& data);

    bool GetButtonDown(InputId buttonId) const;
    bool GetButtonDownPrevFrame(InputId buttonId) const;
    glm::vec2 Get2dInput(InputId inputId) const;

    void SetButtonDown(InputId buttonId, bool set);
    void SetButtonDownPrevFrame(const PlayerInput& lastInput);
    void Set2dInput(InputId inputId, glm::vec2 set);

    void MergeInputs(const PlayerInput& newInput);
};
