#include "input.h"

#include <iterator>

#include "skl_debug.h"


// Controls how input data is interpreted.
// Assumed to be the same for all clients.
// class InputSpecification {
// private:
//     std::array<ButtonSpecification,MaxButtonTypeCount> buttonSpecification{ };
//     std::array<Input2dSpecification, MaxInput2dTypeCount> Input2dSpecification{ };

InputSpecification::InputSpecification(
    u8 buttonTypeCount,
    InputTypeID buttonBaseId,
    std::array<ButtonSpecification,MaxButtonTypeCount> buttonSpecification,
    u8 input2dTypeCount,
    InputTypeID input2DBaseId,
    std::array<MergeBehavior2D, MaxInput2dTypeCount> input2dMergeBehaviors,
    std::array<Input2DType, MaxInput2dTypeCount> input2dTypes) :
    m_buttonTypeCount(buttonTypeCount),
    m_buttonTypeByteCount((buttonTypeCount + 7) / 8 ),
    m_buttonBaseId(buttonBaseId),
    m_buttonSpecification(buttonSpecification),
    m_input2dTypeCount(input2dTypeCount),
    m_input2dTypeByteCount(input2dTypeCount * sizeof(glm::vec2)),
    m_input2DBaseId(input2DBaseId),
    m_input2dMergeBehaviors(input2dMergeBehaviors),
    m_input2dTypes(input2dTypes) {
    ASSERT_PRINT(buttonTypeCount <= MaxButtonTypeCount, "Button type count exceeds bounds");
    ASSERT_PRINT(input2dTypeCount <= MaxInput2dTypeCount, "Input 2d type count exceeds bounds");
}

bool InputSpecification::GetButtonDown(const PlayerInput& input, InputTypeID buttonId) const { 
    return input.inputButtonDown[buttonId + m_buttonBaseId]; 
}
bool InputSpecification::GetButtonDownPrevFrame(const PlayerInput& input, InputTypeID buttonId) const {
    return input.inputButtonPrevDown[buttonId + m_buttonBaseId]; 
}
glm::vec2 InputSpecification::Get2DInput(const PlayerInput& input, InputTypeID inputId) const {
    return input.input2ds[inputId + m_input2DBaseId];
}

void InputSpecification::SetButtonDown(PlayerInput& input, InputTypeID buttonId, bool set) const { 
    input.inputButtonDown[buttonId + m_buttonBaseId] = set; 
}
void InputSpecification::SetButtonDownPrevFrame(PlayerInput& input, const PlayerInput& lastInput) const {
    input.inputButtonPrevDown = lastInput.inputButtonPrevDown; 
}
void InputSpecification::Set2dInput(PlayerInput& input, InputTypeID inputId, glm::vec2 set) const {
    input.input2ds[inputId + m_input2DBaseId] = set;
}

void InputSpecification::MergeInputs(PlayerInput& oldInput, const PlayerInput& newInput) const {
    // Make input buttons
    oldInput.inputButtonDown |= newInput.inputButtonDown;

    // Merge together 2d inputs
    for (int i = 0 ; i < m_input2dTypeCount ; i++) {
        MergeBehavior2D mergeBehavior = m_input2dMergeBehaviors[i];
        switch(mergeBehavior) {
            case MergeBehavior2D::Add:
                oldInput.input2ds[i] += newInput.input2ds[i];
                break;
            case MergeBehavior2D::Copy:
                oldInput.input2ds[i] = newInput.input2ds[i];
                break;
            default:
                // TODO: MAKE FAULT
                break;
        }
    }
}

u32 InputSpecification::WriteBuffer(std::byte* buffer, const PlayerInput& input) const {
    BitsetToBuffer(input.inputButtonDown, buffer, m_buttonTypeByteCount);
    std::memcpy(buffer + m_buttonTypeByteCount, &input.input2ds, m_input2dTypeByteCount);
    return m_buttonTypeByteCount + m_input2dTypeByteCount;
}
u32 InputSpecification::ReadBuffer(const std::byte* buffer, PlayerInput& input) const {
    BufferToBitset(input.inputButtonDown, buffer, m_buttonTypeByteCount);
    std::memcpy(&input.input2ds, buffer + m_buttonTypeByteCount, m_input2dTypeByteCount);
    return m_buttonTypeByteCount + m_input2dTypeByteCount;
}
