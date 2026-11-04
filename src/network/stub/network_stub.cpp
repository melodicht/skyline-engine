// Used when SKL_ENABLE_WEBRTC is off. Nothing is ever connected, so no input
// can be sent or received.
#include <optional>
#include <queue>

#include <input.h>
#include <network_mode.h>

void SetNetworkMode(NetworkMode mode) {}

u64 GetLocalPlayerIdx() {
    return 0;
}

void SyncConnection() {}

void SetInputSpecification(InputSpecification specification) {}

bool SendInput(PlayerData input) {
    return false;
}

const std::queue<std::optional<PlayerData>>& GetInputs(PlayerId playerId) {
    static const std::queue<std::optional<PlayerData>> empty;
    return empty;
}

bool PopInputs(int count) {
    return false;
}
