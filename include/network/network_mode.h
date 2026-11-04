#pragma once

#include <skl_types.h>
#include <meta_definitions.h>

// Decided by SyncConnection. The first device to claim the signaling port is
// the server, and every other device is a client.
enum class NetworkMode : u8 {
    // Hosts the connection. Gathers each player's input, including its own,
    // and sends out each layer once every player has contributed to it.
    Server,
    // Connects to the server. Sends its input to the server, and only
    // receives input as whole layers.
    Client
};
