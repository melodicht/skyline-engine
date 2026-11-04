#pragma once

#include <input.h>

#define NETWORK_FUNCS(method) \
    /* Partitions the local user from vector */ \
    method(PlayerId,SyncConnection,(std::vector<PlayerId>))\
    /* Assumed to happen exactly the same across every device */ \
    /* Should be done before any data flows through the network */ \
    method(void,SetInputSpecification,(InputSpecification))\
    /* Sends the data of a frame out to others at next frame */ \
    method(bool,SendInput,(PlayerData))\
    /* Attempts to get gather inputs on future frames */ \
    method(const std::queue<std::optional<PlayerData>>&,GetInputs,(PlayerId)) \
    /* Pops the front n inputs from the queue*/ \
    method(bool, PopInputs,(int)) \
DEFINE_GAME_MODULE_API(PlatformNetwork, NETWORK_FUNCS)
