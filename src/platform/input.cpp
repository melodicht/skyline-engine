#include "input.h"

#include <algorithm>

#include "skl_debug.h"


InputSpecification::InputSpecification(
    u8 buttonTypeCount,
    u8 input2dTypeCount,
    std::array<InputId, MaxButtonTypeCount> buttonIds,
    std::array<InputId, MaxButtonTypeCount> input2dIds) :
    m_buttonIds(buttonIds),
    m_input2dIds(input2dIds),
    m_input2Specs{ },
    m_playerIdCounter(0),
    m_buttonTypeByteCount((buttonTypeCount + 7) / 8),
    m_buttonTypeCount(buttonTypeCount),
    m_input2dTypeByteCount(input2dTypeCount * sizeof(glm::vec2)),
    m_input2dTypeCount(input2dTypeCount) {
    ASSERT_PRINT(buttonTypeCount <= MaxButtonTypeCount, "Button type count exceeds bounds");
    ASSERT_PRINT(input2dTypeCount <= MaxInput2dTypeCount, "Input 2d type count exceeds bounds");
}

void InputSpecification::AddButton(InputId count) {
    // TODO: Implement.
    (void)count;
}

void InputSpecification::AddInput2d(InputId count) {
    // TODO: Implement.
    (void)count;
}

void InputSpecification::RemoveButton(InputId count) {
    // TODO: Implement.
    (void)count;
}

void InputSpecification::RemoveInput2d(InputId count) {
    // TODO: Implement.
    (void)count;
}

void InputSpecification::SetInput2dSpec(InputId id, Input2dSpecification input) {
    for (int i = 0 ; i < m_input2dTypeCount ; i++) {
        if (m_input2dIds[i] == id) {
            m_input2Specs[i] = input;
            return;
        }
    }
    ASSERT_PRINT(m_input2dTypeCount < MaxInput2dTypeCount, "Input 2d type count exceeds bounds");
    if (m_input2dTypeCount >= MaxInput2dTypeCount) return;
    m_input2dIds[m_input2dTypeCount] = id;
    m_input2Specs[m_input2dTypeCount] = input;
    m_input2dTypeCount++;
    m_input2dTypeByteCount = m_input2dTypeCount * sizeof(glm::vec2);
}

PlayerId InputSpecification::AddPlayer() {
    // TODO: Implement.
    return 0;
}

void InputSpecification::RemovePlayer() {
    // TODO: Implement.
}

void InputSpecification::AddButtonGatheringSpec(PlayerId playerId, InputId inputId, ButtonGatherTypeInfo info) {
    // TODO: Implement.
    (void)playerId;
    (void)inputId;
    (void)info;
}

void InputSpecification::AddInput2dGatheringSpec(PlayerId playerId, InputId inputId, Input2dGatherType info) {
    // TODO: Implement.
    (void)playerId;
    (void)inputId;
    (void)info;
}

const u64 InputSpecification::GetPlayerIdx(PlayerId playerId) const {
    // TODO: Implement.
    (void)playerId;
    return 0;
}

const u64 InputSpecification::GetPlayerCount(PlayerId playerId) const {
    // TODO: Implement.
    (void)playerId;
    return 0;
}

const std::span<ButtonGatherBehavior> InputSpecification::GetButtonGatherBehavior(ButtonGatherType type) {
    // TODO: Implement.
    (void)type;
    return { };
}

const std::span<Input2dGatherBehavior> InputSpecification::GetInput2dGatherBehavior(Input2dGatherType type) {
    // TODO: Implement.
    (void)type;
    return { };
}

u64 PlayerSpecification::GetPlayerIdIdx(PlayerId id) {
    // TODO: Implement.
    (void)id;
    return 0;
}

PlayerInput::PlayerInput(const InputSpecification& spec, PlayerId playerId) :
    spec(&spec) {
    // TODO: Use playerId.
    (void)playerId;
    m_input2ds.fill(glm::vec2(0.0f));
}

bool PlayerInput::GetButtonDown(const PlayerInput& input, InputId buttonId) const {
    auto end = spec->m_buttonIds.begin() + spec->m_buttonTypeCount;
    auto it = std::find(spec->m_buttonIds.begin(), end, buttonId);
    size_t dist = std::distance(spec->m_buttonIds.begin(), it);
    return dist < spec->m_buttonTypeCount && input.m_inputButtonDown[dist];
}

bool PlayerInput::GetButtonDownPrevFrame(const PlayerInput& input, InputId buttonId) const {
    auto end = spec->m_buttonIds.begin() + spec->m_buttonTypeCount;
    auto it = std::find(spec->m_buttonIds.begin(), end, buttonId);
    size_t dist = std::distance(spec->m_buttonIds.begin(), it);
    return dist < spec->m_buttonTypeCount && input.m_inputButtonPrevDown[dist];
}

glm::vec2 PlayerInput::Get2DInput(const PlayerInput& input, InputId inputId) const {
    auto end = spec->m_input2dIds.begin() + spec->m_input2dTypeCount;
    auto it = std::find(spec->m_input2dIds.begin(), end, inputId);
    size_t dist = std::distance(spec->m_input2dIds.begin(), it);
    return dist < spec->m_input2dTypeCount ? input.m_input2ds[dist] : glm::vec2(0.0f);
}

void PlayerInput::SetButtonDown(PlayerInput& input, InputId buttonId, bool set) {
    auto end = spec->m_buttonIds.begin() + spec->m_buttonTypeCount;
    auto it = std::find(spec->m_buttonIds.begin(), end, buttonId);
    size_t dist = std::distance(spec->m_buttonIds.begin(), it);
    ASSERT_PRINT(dist < spec->m_buttonTypeCount, "OOB");
    if (dist < spec->m_buttonTypeCount) input.m_inputButtonDown[dist] = set;
}

void PlayerInput::SetButtonDownPrevFrame(PlayerInput& input, const PlayerInput& lastInput) {
    input.m_inputButtonPrevDown = lastInput.m_inputButtonDown;
}

void PlayerInput::Set2dInput(PlayerInput& input, InputId inputId, glm::vec2 set) {
    auto end = spec->m_input2dIds.begin() + spec->m_input2dTypeCount;
    auto it = std::find(spec->m_input2dIds.begin(), end, inputId);
    size_t dist = std::distance(spec->m_input2dIds.begin(), it);
    ASSERT_PRINT(dist < spec->m_input2dTypeCount, "OOB");
    if (dist < spec->m_input2dTypeCount) input.m_input2ds[dist] = set;
}

void PlayerInput::MergeInputs(const PlayerInput& newInput) {
    ASSERT_PRINT(spec == newInput.spec, "Input specification mismatch");
    m_inputButtonDown |= newInput.m_inputButtonDown;

    for (int i = 0 ; i < spec->m_input2dTypeCount ; i++) {
        MergeBehavior2D mergeBehavior = spec->m_input2Specs[i].m_mergeBehavior;
        switch(mergeBehavior) {
            case MergeBehavior2D::Add:
                m_input2ds[i] += newInput.m_input2ds[i];
                break;
            case MergeBehavior2D::Copy:
                m_input2ds[i] = newInput.m_input2ds[i];
                break;
            default:
                // TODO: MAKE FAULT
                break;
        }
    }
}

u32 InputSpecification::WriteBuffer(std::byte* buffer, const PlayerInput& input) const {
    BitsetToBuffer(input.m_inputButtonDown, buffer, m_buttonTypeByteCount);
    std::memcpy(buffer + m_buttonTypeByteCount, &input.m_input2ds, m_input2dTypeByteCount);
    return m_buttonTypeByteCount + m_input2dTypeByteCount;
}

u32 InputSpecification::ReadBuffer(const std::byte* buffer, PlayerInput& input) const {
    BufferToBitset(input.m_inputButtonDown, buffer, m_buttonTypeByteCount);
    std::memcpy(&input.m_input2ds, buffer + m_buttonTypeByteCount, m_input2dTypeByteCount);
    return m_buttonTypeByteCount + m_input2dTypeByteCount;
}
