#pragma once

#include <skl_types.h>
#include <meta_definitions.h>

enum class NetworkMode : u8 {
    // Hosts the connection. Gathers each player's input, including its own,
    // and sends out each layer once every player has contributed to it.
    Server,
    // Connects to the server. Sends its input to the server, and only
    // receives input as whole layers.
    Client
};

// Must be called before SyncConnection.
void SetNetworkMode(NetworkMode mode);

// The index of the player this device sends input for, as given by
// InputSpecification::GetPlayerIdx. The server is always 0, and clients are
// given theirs during SyncConnection.
u64 GetLocalPlayerIdx();
