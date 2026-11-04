#pragma once

#include <input.h>

#define NETWORK_FUNCS(method) \
    /* Partitions the local user from a vector of potential player ids */ \
    method(PlayerId,SyncConnection,(std::vector<PlayerId>))\
    /* Assumed to happen exactly the same across every device */ \
    /* Should be done before any data flows through the network */ \
    method(void,SetInputSpecification,(InputSpecification))\
    /* Sends the data of a frame out to others at next frame */ \
    method(bool,SendInput,(PlayerData))\
    /* Attempts to get gather inputs on future frames. */ \
    /* For now only shows when all inputs are available in a single frame */ \
    /* Empty if information simply is not here yet */ \
    method(std::vector<PlayerData>,GetInputs,())

DEFINE_GAME_MODULE_API(PlatformNetwork, NETWORK_FUNCS)
