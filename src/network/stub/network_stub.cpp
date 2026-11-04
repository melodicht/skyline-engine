// Used when SKL_ENABLE_WEBRTC is off. Nothing is ever connected, so no input
// can be sent or received.
#include <network_game.h>
#include <network_mode.h>

void SetNetworkMode(NetworkMode mode) {}

u64 GetLocalPlayerIdx() {
    return 0;
}

PlayerId SyncConnection(std::vector<PlayerId> playerIds) {
    return 0;
}

void SetInputSpecification(InputSpecification specification) {}

bool SendInput(PlayerData input) {
    return false;
}

std::vector<PlayerData> GetInputs() {
    return { };
}
