#ifndef __QUEST64_LIGHTING_H__
#define __QUEST64_LIGHTING_H__

#include <cstdint>

#include "hle/rt64_scene_light.h"

namespace quest64 {
    // Picks the advanced lighting sun, moon or indoor light for the room and time the game is in.
    RT64::SceneLight scene_light_from_game(const uint8_t* rdram);
}

#endif
