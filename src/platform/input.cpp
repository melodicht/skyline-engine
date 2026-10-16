#include "input.h"

#include <algorithm>
#include <limits>

#include "skl_debug.h"

static inline u64 GetIdIdx(InputId id, const InputId* array, u8 count) {
    auto it = std::find(array, array + count, id);
    return std::distance(array, it);
}

InputSpecification::InputSpecification(
    u8 buttonTypeCount,
    u8 input2dTypeCount,
    std::array<InputId, MaxButtonTypeCount> buttonIds,
    std::array<InputId, MaxButtonTypeCount> input2dIds) :
    m_buttonIds(buttonIds),
    m_input2dIds(input2dIds),
    m_input2Specs{ },
    m_playerIdCounter(1),
    m_buttonTypeByteCount((std::min(buttonTypeCount, MaxButtonTypeCount) + 7) / 8),
    m_buttonTypeCount(std::min(buttonTypeCount, MaxButtonTypeCount)),
    m_input2dTypeByteCount(std::min(input2dTypeCount, MaxInput2dTypeCount) * sizeof(glm::vec2)),
    m_input2dTypeCount(std::min(input2dTypeCount, MaxInput2dTypeCount)) {
    ASSERT_PRINT(buttonTypeCount <= MaxButtonTypeCount, "Button type count exceeds bounds");
    ASSERT_PRINT(input2dTypeCount <= MaxInput2dTypeCount, "Input 2d type count exceeds bounds");
}

u8 InputSpecification::GetButtonSpecCount() const {
    return m_buttonTypeCount;
}
u8 InputSpecification::GetInput2dSpecCount() const {
    return m_input2dTypeCount;
}

void InputSpecification::AddButtonSpec(InputId id) {
    TRY_LOG_RETURN(
        GetIdIdx(id, m_buttonIds.data(), m_buttonTypeCount) >= m_buttonTypeCount, 
        "Button type already exists");
    TRY_LOG_RETURN(
        m_buttonTypeCount < MaxButtonTypeCount, 
        "Added more button types then allowed");
    m_buttonIds[m_buttonTypeCount] = id;
    m_buttonTypeCount += 1;
    m_buttonTypeByteCount = (m_buttonTypeCount + 7) / 8;
}

void InputSpecification::AddInput2dSpec(InputId id) {
    TRY_LOG_RETURN(
        GetIdIdx(id, m_input2dIds.data(), m_input2dTypeCount) >= m_input2dTypeCount, 
        "Input 2d type already exists");
    TRY_LOG_RETURN(
        m_input2dTypeCount < MaxInput2dTypeCount, 
        "Added more input 2d types then allowed");
    m_input2dIds[m_input2dTypeCount] = id;
    m_input2Specs[m_input2dTypeCount] = { };
    m_input2dTypeCount += 1;
    m_input2dTypeByteCount = m_input2dTypeCount * sizeof(glm::vec2);
}

void InputSpecification::RemoveButtonSpec(InputId id) {
    u64 idx = GetIdIdx(id, m_buttonIds.data(), m_buttonTypeCount);
    TRY_LOG_RETURN(idx < m_buttonTypeCount, "Button doesn't exist");
    for (u64 i = idx; i + 1 < m_buttonTypeCount; i++) {
        m_buttonIds[i] = m_buttonIds[i + 1];
    }
    m_buttonTypeCount--;
    m_buttonIds[m_buttonTypeCount] = 0;
    m_buttonTypeByteCount = (m_buttonTypeCount + 7) / 8;
    std::erase_if(m_buttonGatherTypes, [idx](const ButtonGatherSpecification& behavior) {
        return behavior.m_buttonIdx == idx;
    });
    for (auto& behavior : m_buttonGatherTypes) {
        if (behavior.m_buttonIdx > idx) behavior.m_buttonIdx--;
    }
    UpdateGatherRanges();
}

void InputSpecification::RemoveInput2dSpec(InputId id) {
    u64 idx = GetIdIdx(id, m_input2dIds.data(), m_input2dTypeCount);
    TRY_LOG_RETURN(idx < m_input2dTypeCount, "Input 2d doesn't exist");
    for (u64 i = idx; i + 1 < m_input2dTypeCount; i++) {
        m_input2dIds[i] = m_input2dIds[i + 1];
        m_input2Specs[i] = m_input2Specs[i + 1];
    }
    m_input2dTypeCount--;
    m_input2dIds[m_input2dTypeCount] = 0;
    m_input2Specs[m_input2dTypeCount] = { };
    m_input2dTypeByteCount = m_input2dTypeCount * sizeof(glm::vec2);
    std::erase_if(m_input2dGatherTypes, [idx](const Input2dGatherSpecification& behavior) {
        return behavior.m_buttonIdx == idx;
    });
    for (auto& behavior : m_input2dGatherTypes) {
        if (behavior.m_buttonIdx > idx) behavior.m_buttonIdx--;
    }
    UpdateGatherRanges();
}

void InputSpecification::SetInput2dSpec(InputId id, Input2dSpecification input) {
    for (int i = 0 ; i < m_input2dTypeCount ; i++) {
        if (m_input2dIds[i] == id) {
            m_input2Specs[i] = input;
            return;
        }
    }
    u8 idx = m_input2dTypeCount;
    AddInput2dSpec(id);
    TRY_LOG_RETURN(idx < m_input2dTypeCount, "");
    if (idx < m_input2dTypeCount) m_input2Specs[idx] = input;
}

PlayerId InputSpecification::AddPlayer() {
    TRY_LOG_RETURN(m_playerIds.size() <= std::numeric_limits<u8>::max(), "Player count exceeds gather index capacity", 0);
    TRY_LOG_RETURN(m_playerIdCounter < std::numeric_limits<PlayerId>::max(), "Player IDs exhausted", 0);
    PlayerId id = m_playerIdCounter++;
    m_playerIds.push_back(id);
    return id;
}

void InputSpecification::RemovePlayer(PlayerId playerId) {
    u64 idx = GetPlayerIdx(playerId);
    TRY_LOG_RETURN(idx < m_playerIds.size(), "Player doesn't exist to be removed");
    m_playerIds.erase(m_playerIds.begin() + idx);
    std::erase_if(m_buttonGatherTypes, [idx](const ButtonGatherSpecification& behavior) {
        return behavior.m_playerIdx == idx;
    });
    for (auto& behavior : m_buttonGatherTypes) {
        if (behavior.m_playerIdx > idx) behavior.m_playerIdx--;
    }
    std::erase_if(m_input2dGatherTypes, [idx](const Input2dGatherSpecification& behavior) {
        return behavior.m_playerIdx == idx;
    });
    for (auto& behavior : m_input2dGatherTypes) {
        if (behavior.m_playerIdx > idx) behavior.m_playerIdx--;
    }
    UpdateGatherRanges();
}

void InputSpecification::AddButtonGatheringSpec(PlayerId playerId, InputId inputId, ButtonGatherTypeInfo info) {
    u64 playerIdx = GetPlayerIdx(playerId);
    u64 buttonIdx = GetIdIdx(inputId, m_buttonIds.data(), m_buttonTypeCount);
    TRY_LOG_RETURN(playerIdx < m_playerIds.size(), "Unknown player ID");
    TRY_LOG_RETURN(buttonIdx < m_buttonTypeCount, "Unknown button ID");
    TRY_LOG_RETURN(info.m_type < ButtonGatherType::Count, "Invalid button gather type");
    if (playerIdx >= m_playerIds.size() || buttonIdx >= m_buttonTypeCount || info.m_type >= ButtonGatherType::Count) return;
    m_buttonGatherTypes.push_back({(u8)playerIdx, (u8)buttonIdx, info});
    UpdateGatherRanges();
}

void InputSpecification::AddInput2dGatheringSpec(PlayerId playerId, InputId inputId, Input2dGatherType info) {
    u64 playerIdx = GetPlayerIdx(playerId);
    u64 inputIdx = GetIdIdx(inputId, m_input2dIds.data(), m_input2dTypeCount);
    TRY_LOG_RETURN(playerIdx < m_playerIds.size(), "Unknown player ID");
    TRY_LOG_RETURN(inputIdx < m_input2dTypeCount, "Unknown input 2d ID");
    TRY_LOG_RETURN(info < Input2dGatherType::Count, "Invalid input 2d gather type");
    m_input2dGatherTypes.push_back({(u8)playerIdx, (u8)inputIdx, info});
    UpdateGatherRanges();
}

const u64 InputSpecification::GetPlayerIdx(PlayerId playerId) const {
    auto it = std::find(m_playerIds.begin(), m_playerIds.end(), playerId);
    return std::distance(m_playerIds.begin(), it);
}

const u64 InputSpecification::GetPlayerCount(PlayerId playerId) const {
    return m_playerIds.size();
}

const std::span<const ButtonGatherSpecification> InputSpecification::GetButtonGatherSpecification(ButtonGatherType type) const {
    TRY_LOG_RETURN(type < ButtonGatherType::Count, "Invalid button gather type", { });
    const auto& range = m_buttonGatherRanges[(size_t)type];
    return std::span<const ButtonGatherSpecification>(m_buttonGatherTypes).subspan(range.begin, range.end - range.begin);
}

const std::span<const Input2dGatherSpecification> InputSpecification::GetInput2dGatherSpecification(Input2dGatherType type) const {
    TRY_LOG_RETURN(type < Input2dGatherType::Count, "Invalid input 2d gather type", { });
    const auto& range = m_input2dGatherRanges[(size_t)type];
    return std::span<const Input2dGatherSpecification>(m_input2dGatherTypes).subspan(range.begin, range.end - range.begin);
}

void InputSpecification::UpdateGatherRanges() {
    std::sort(m_buttonGatherTypes.begin(), m_buttonGatherTypes.end(),
        [](const ButtonGatherSpecification& a, const ButtonGatherSpecification& b) { return a.m_type.m_type < b.m_type.m_type; });
    u32 cursor = 0;
    for (size_t i = 0; i < m_buttonGatherRanges.size(); i++) {
        m_buttonGatherRanges[i].begin = cursor;
        while (cursor < m_buttonGatherTypes.size() && (size_t)m_buttonGatherTypes[cursor].m_type.m_type == i) cursor++;
        m_buttonGatherRanges[i].end = cursor;
    }
    std::sort(m_input2dGatherTypes.begin(), m_input2dGatherTypes.end(),
        [](const Input2dGatherSpecification& a, const Input2dGatherSpecification& b) { return a.m_type < b.m_type; });
    cursor = 0;
    for (size_t i = 0; i < m_input2dGatherRanges.size(); i++) {
        m_input2dGatherRanges[i].begin = cursor;
        while (cursor < m_input2dGatherTypes.size() && (size_t)m_input2dGatherTypes[cursor].m_type == i) cursor++;
        m_input2dGatherRanges[i].end = cursor;
    }
}

PlayerInput::PlayerInput(const InputSpecification& spec, PlayerData& data) :
    m_inputSpec(spec),
    m_playerData(data) {}

bool PlayerInput::GetButtonDown(InputId buttonId) const {
    size_t dist = GetIdIdx(buttonId, m_inputSpec.m_buttonIds.begin(), m_inputSpec.m_buttonTypeCount);
    return dist < m_inputSpec.m_buttonTypeCount && m_playerData.m_inputButtonDown[dist];
}

bool PlayerInput::GetButtonDownPrevFrame(InputId buttonId) const {
    size_t dist = GetIdIdx(buttonId, m_inputSpec.m_buttonIds.begin(), m_inputSpec.m_buttonTypeCount);
    TRY_LOG_RETURN(dist < m_inputSpec.m_buttonTypeCount, "Button id doesn't exist", false);
    return dist < m_inputSpec.m_buttonTypeCount && m_playerData.m_inputButtonPrevDown[dist];
}

glm::vec2 PlayerInput::Get2dInput(InputId inputId) const {
    auto end = m_inputSpec.m_input2dIds.begin() + m_inputSpec.m_input2dTypeCount;
    auto it = std::find(m_inputSpec.m_input2dIds.begin(), end, inputId);
    size_t dist = std::distance(m_inputSpec.m_input2dIds.begin(), it);
    TRY_LOG_RETURN(dist < m_inputSpec.m_input2dTypeCount, "Input 2d id doesn't exist", glm::vec2(0.0f));
    return m_playerData.m_input2ds[dist];
}

void PlayerInput::SetButtonDown(InputId buttonId, bool set) {
    auto end = m_inputSpec.m_buttonIds.begin() + m_inputSpec.m_buttonTypeCount;
    auto it = std::find(m_inputSpec.m_buttonIds.begin(), end, buttonId);
    size_t dist = std::distance(m_inputSpec.m_buttonIds.begin(), it);
    TRY_LOG_RETURN(dist < m_inputSpec.m_buttonTypeCount, "Button id doesn't exist");
    m_playerData.m_inputButtonDown[dist] = set;
}

void PlayerInput::SetButtonDownPrevFrame(const PlayerInput& lastInput) {
    ASSERT_LOG(&m_inputSpec == &lastInput.m_inputSpec, "Input specification mismatch");
    m_playerData.m_inputButtonPrevDown = lastInput.m_playerData.m_inputButtonDown;
}

void PlayerInput::Set2dInput(InputId inputId, glm::vec2 set) {
    auto end = m_inputSpec.m_input2dIds.begin() + m_inputSpec.m_input2dTypeCount;
    auto it = std::find(m_inputSpec.m_input2dIds.begin(), end, inputId);
    size_t dist = std::distance(m_inputSpec.m_input2dIds.begin(), it);
    ASSERT_PRINT(dist < m_inputSpec.m_input2dTypeCount, "OOB");
    if (dist < m_inputSpec.m_input2dTypeCount) m_playerData.m_input2ds[dist] = set;
}

void PlayerInput::MergeInputs(const PlayerInput& newInput) {
    ASSERT_LOG(&m_inputSpec == &newInput.m_inputSpec, "Input specification mismatch");
    m_playerData.m_inputButtonDown |= newInput.m_playerData.m_inputButtonDown;

    for (int i = 0 ; i < m_inputSpec.m_input2dTypeCount ; i++) {
        MergeBehavior2d mergeBehavior = m_inputSpec.m_input2Specs[i].m_mergeBehavior;
        switch(mergeBehavior) {
            case MergeBehavior2d::Add:
                m_playerData.m_input2ds[i] += newInput.m_playerData.m_input2ds[i];
                break;
            case MergeBehavior2d::Copy:
                m_playerData.m_input2ds[i] = newInput.m_playerData.m_input2ds[i];
                break;
            default:
                FAULT_LOG("Invalid MergeBehavior2d used");
                break;
        }
    }
}

u32 InputSpecification::WriteBuffer(std::byte* buffer, const PlayerData& input) const {
    BitsetToBuffer(input.m_inputButtonDown, buffer, m_buttonTypeByteCount);
    std::memcpy(buffer + m_buttonTypeByteCount, &input.m_input2ds, m_input2dTypeByteCount);
    return m_buttonTypeByteCount + m_input2dTypeByteCount;
}

u32 InputSpecification::ReadBuffer(const std::byte* buffer, PlayerData& input) const {
    BufferToBitset(input.m_inputButtonDown, buffer, m_buttonTypeByteCount);
    std::memcpy(&input.m_input2ds, buffer + m_buttonTypeByteCount, m_input2dTypeByteCount);
    return m_buttonTypeByteCount + m_input2dTypeByteCount;
}
