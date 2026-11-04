#pragma once

#include <cstdint>

#include <input.h>

// How a single player on this device gathers input.
struct GatheringScheme {
    std::vector<ButtonGatherPair> buttons;
    std::vector<Input2dGatherPair> inputs2d;
};

#define NETWORK_FUNCS(method) \
    /* Blocks until every player id is claimed by a device. This device claims one per */ \
    /* given scheme, which gets applied to it. All ids are held on to, as a frame needs all of them. */ \
    /* The specification and player ids must be equal across devices, or this device isn't connected. */ \
    /* The specification must outlive the network. */ \
    method(void,SyncConnection,(InputSpecification*, std::vector<GatheringScheme>, std::vector<PlayerId>))\
    /* Whether input is flowing through the network */ \
    method(bool,IsConnected,())\
    /* Sends the data of a frame out to others at next frame. */ \
    /* Given every player's data, in the input specification's order */ \
    method(bool,SendInput,(std::span<const PlayerData>))\
    /* Attempts to get gather inputs on future frames. */ \
    /* For now only shows when all inputs are available in a single frame */ \
    /* Empty if information simply is not here yet */ \
    method(std::vector<PlayerData>,GetInputs,())\
    /* The id of the last frame given out by GetInputs, starting from 0. Is 0 until then too */ \
    method(int32_t,GetCurrentFrame,())

DEFINE_GAME_MODULE_API(PlatformNetwork, NETWORK_FUNCS)
