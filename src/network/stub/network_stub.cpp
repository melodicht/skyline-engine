// Used when SKL_ENABLE_WEBRTC is off. Nothing is ever connected, so no input
// can be sent or received, and each scheme simply applies to a player in order.
#include <network_game.h>

#include <algorithm>

#include <skl_debug.h>

void SyncConnection(InputSpecification* specification, std::vector<GatheringScheme> schemes, std::vector<PlayerId> playerIds) {
    TRY_LOG_RETURN(specification, "SyncConnection needs an input specification");
    u64 count = std::min(schemes.size(), playerIds.size());
    for (u64 i = 0; i < count; i++) {
        for (const auto& [inputId, info] : schemes[i].buttons) {
            specification->AddButtonGatheringSpec(playerIds[i], inputId, info);
        }
        for (const auto& [inputId, info] : schemes[i].inputs2d) {
            specification->AddInput2dGatheringSpec(playerIds[i], inputId, info);
        }
    }
}

bool IsConnected() {
    return false;
}

bool SendInput(std::span<const PlayerData> inputs) {
    return false;
}

std::vector<PlayerData> GetInputs() {
    return { };
}
