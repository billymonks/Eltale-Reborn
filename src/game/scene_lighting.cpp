#include <algorithm>
#include <cmath>

#include "quest64_lighting.h"

namespace {
    // Game variables. Despite the decomp names, gNextMap/gNextSubmap hold the loaded room.
    constexpr uint32_t gNextMap = 0x80084EE4;
    constexpr uint32_t gNextSubmap = 0x80084EE8;
    constexpr uint32_t gCurrentTime = 0x800859D0;
    constexpr uint32_t gDayLength = 0x800859D4;
    constexpr uint32_t gLightTime = 0x800859D8;

    // Clock marks on the game's 27000 tick day.
    constexpr float DefaultDayLength = 27000.0f;
    constexpr float SunriseTime = 5625.0f;
    constexpr float SunsetTime = 21375.0f;

    constexpr float Pi = 3.14159265f;
    constexpr float DegToRad = Pi / 180.0f;

    int32_t read_s32(const uint8_t* rdram, uint32_t address) {
        return *reinterpret_cast<const int32_t*>(rdram + (address - 0x80000000u));
    }

    enum class LightKind {
        Outdoor,  // Sun by day, moon by night. Yaw turns the sun's path, pitch is its highest elevation.
        Interior, // Houses, inns, ships. Fixed light from yaw/pitch.
        Castle,   // Castle halls. Fixed light from yaw/pitch.
        Cave,     // Caves and shrines. Fixed light from yaw/pitch.
        Mammon,   // World of Mammon and its ghost rooms. Fixed light from yaw/pitch.
    };

    // A room of -1 covers every room of the map that has no entry of its own.
    // Yaw is in degrees around the up axis, 0 towards +Z and 90 towards +X. Pitch is in degrees above the horizon.
    struct RoomLight {
        int map;
        int room;
        LightKind kind;
        float yaw;
        float pitch;
    };

    constexpr RoomLight room_lights[] = {
        { 0x00, -1, LightKind::Outdoor, 0.0f, 62.0f },   // Melrode
        { 0x00, 2, LightKind::Mammon, -130.0f, 55.0f },  // Floating monastery / Mammon approach
        { 0x00, 3, LightKind::Mammon, -130.0f, 55.0f },  // Ghost Melrode village
        { 0x00, 4, LightKind::Mammon, -130.0f, 55.0f },  // Ghost monastery path
        { 0x01, -1, LightKind::Outdoor, 0.0f, 62.0f },   // Dondoran
        { 0x02, -1, LightKind::Outdoor, 0.0f, 62.0f },   // Holy Plain
        { 0x03, -1, LightKind::Outdoor, 0.0f, 62.0f },   // Dondoran Flats
        { 0x04, -1, LightKind::Outdoor, 0.0f, 62.0f },   // Larapool
        { 0x05, -1, LightKind::Outdoor, 0.0f, 62.0f },   // West Carmagh
        { 0x06, -1, LightKind::Outdoor, 0.0f, 62.0f },   // Normoon
        { 0x07, -1, LightKind::Outdoor, 0.0f, 62.0f },   // East Limelin
        { 0x08, -1, LightKind::Outdoor, 0.0f, 62.0f },   // Limelin
        { 0x09, -1, LightKind::Outdoor, 0.0f, 70.0f },   // Dindom Dries
        { 0x0A, -1, LightKind::Outdoor, 0.0f, 62.0f },   // Shamwood
        { 0x0B, -1, LightKind::Outdoor, 0.0f, 55.0f },   // Brannoch / Baragoon Moor
        { 0x0B, 3, LightKind::Cave, 20.0f, 72.0f },      // Post-Beigis passage
        { 0x0B, 4, LightKind::Cave, 20.0f, 72.0f },
        { 0x0B, 5, LightKind::Cave, 20.0f, 72.0f },
        { 0x0B, 6, LightKind::Cave, 20.0f, 72.0f },
        { 0x0B, 7, LightKind::Cave, 20.0f, 72.0f },
        { 0x0C, -1, LightKind::Outdoor, 0.0f, 58.0f },   // Isle of Skye
        { 0x0D, -1, LightKind::Interior, 35.0f, 58.0f }, // Melrode Monastery
        { 0x0E, -1, LightKind::Castle, -30.0f, 62.0f },  // Dondoran Castle
        { 0x0E, 13, LightKind::Mammon, -130.0f, 55.0f }, // Ghost room
        { 0x0F, -1, LightKind::Interior, 35.0f, 58.0f }, // Melrode houses
        { 0x0F, 13, LightKind::Mammon, -130.0f, 55.0f }, // Ghost house
        { 0x10, -1, LightKind::Interior, 35.0f, 58.0f }, // Dondoran houses
        { 0x11, -1, LightKind::Interior, 35.0f, 58.0f }, // Larapool houses
        { 0x12, -1, LightKind::Interior, 35.0f, 58.0f }, // Larapool & West Carmagh houses
        { 0x13, -1, LightKind::Interior, 35.0f, 66.0f }, // Normoon windmills
        { 0x14, -1, LightKind::Interior, 35.0f, 58.0f }, // Normoon houses
        { 0x15, -1, LightKind::Castle, -30.0f, 62.0f },  // Limelin Castle
        { 0x16, -1, LightKind::Interior, 35.0f, 58.0f }, // Limelin houses
        { 0x16, 24, LightKind::Mammon, -130.0f, 55.0f }, // Ghost house
        { 0x16, 25, LightKind::Mammon, -130.0f, 55.0f }, // Ghost house cellar
        { 0x17, -1, LightKind::Interior, 35.0f, 58.0f }, // Greenoch/Dindom/Brannoch houses
        { 0x18, -1, LightKind::Castle, -30.0f, 62.0f },  // Shamwood palace
        { 0x19, -1, LightKind::Interior, 35.0f, 58.0f }, // Skye houses, ships
        { 0x19, 6, LightKind::Mammon, -130.0f, 55.0f },  // Ghost ship corridor
        { 0x19, 7, LightKind::Mammon, -130.0f, 55.0f },  // Ghost ship hold
        { 0x1A, -1, LightKind::Cave, 20.0f, 72.0f },     // Blue Cave
        { 0x1A, 2, LightKind::Outdoor, 0.0f, 62.0f },    // Crystal Valley
        { 0x1A, 3, LightKind::Interior, 35.0f, 58.0f },  // Epona's cottage
        { 0x1A, 4, LightKind::Interior, 35.0f, 58.0f },  // Epona's cottage back room
        { 0x1B, -1, LightKind::Cave, 20.0f, 72.0f },     // Cull Hazard
        { 0x1C, -1, LightKind::Cave, 20.0f, 72.0f },     // Baragoon Tunnel
        { 0x1D, -1, LightKind::Cave, 20.0f, 72.0f },     // Boil Hole
        { 0x1E, -1, LightKind::Castle, -30.0f, 62.0f },  // Brannoch Castle
        { 0x1F, -1, LightKind::Outdoor, 0.0f, 62.0f },   // Connor Forest
        { 0x1F, 2, LightKind::Interior, 35.0f, 58.0f },  // Forest hut
        { 0x20, -1, LightKind::Outdoor, 0.0f, 62.0f },   // Glencoe Forest
        { 0x20, 1, LightKind::Interior, 35.0f, 58.0f },  // Glencoe cottage
        { 0x20, 2, LightKind::Interior, 35.0f, 58.0f },  // Glencoe cottage back room
        { 0x21, -1, LightKind::Outdoor, 0.0f, 62.0f },   // Windward Forest
        { 0x21, 1, LightKind::Interior, 35.0f, 58.0f },  // Windward forest house
        { 0x21, 2, LightKind::Interior, 35.0f, 58.0f },  // Windward forest house back room
        { 0x22, -1, LightKind::Mammon, -130.0f, 55.0f }, // World of Mammon
        { 0x23, -1, LightKind::Cave, 20.0f, 72.0f },     // Nepty's shrine
    };

    const RoomLight default_light = { -1, -1, LightKind::Interior, 35.0f, 58.0f };

    const RoomLight& find_room_light(int map, int room) {
        const RoomLight* map_light = nullptr;
        for (const RoomLight& light : room_lights) {
            if (light.map != map) {
                continue;
            }
            if (light.room == room) {
                return light;
            }
            if (light.room < 0) {
                map_light = &light;
            }
        }
        return (map_light != nullptr) ? *map_light : default_light;
    }

    void set_direction(RT64::SceneLight& light, float yaw_deg, float pitch_deg) {
        const float yaw = yaw_deg * DegToRad;
        const float pitch = pitch_deg * DegToRad;
        light.direction[0] = std::cos(pitch) * std::sin(yaw);
        light.direction[1] = std::sin(pitch);
        light.direction[2] = std::cos(pitch) * std::cos(yaw);
    }

    void set_style(RT64::SceneLight& light, float r, float g, float b, float facing_floor, float strength) {
        light.shadowTint[0] = 1.0f + (r - 1.0f) * strength;
        light.shadowTint[1] = 1.0f + (g - 1.0f) * strength;
        light.shadowTint[2] = 1.0f + (b - 1.0f) * strength;
        light.facingFloor = 1.0f + (facing_floor - 1.0f) * strength;
    }

    float smoothstep(float edge0, float edge1, float x) {
        const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    void set_sky_light(RT64::SceneLight& light, const RoomLight& room, float time, float day_length) {
        const float scale = day_length / DefaultDayLength;
        const float sunrise = SunriseTime * scale;
        const float sunset = SunsetTime * scale;
        const bool sun_up = (time >= sunrise) && (time < sunset);

        // Phase runs 0 to 1 from rising to setting, for the sun by day and the moon by night.
        float phase;
        if (sun_up) {
            phase = (time - sunrise) / (sunset - sunrise);
        }
        else {
            const float night_length = day_length - (sunset - sunrise);
            phase = std::fmod(time - sunset + day_length, day_length) / night_length;
        }
        phase = std::clamp(phase, 0.0f, 1.0f);

        // With yaw 0: rises on the -X side, crosses the sky leaning towards +Z and sets on the +X side.
        const float arc = std::sin(phase * Pi);
        const float min_elevation = 18.0f;
        const float max_elevation = sun_up ? room.pitch : std::min(room.pitch, 50.0f);
        const float elevation = min_elevation + (max_elevation - min_elevation) * arc;
        const float azimuth = room.yaw - 90.0f + 180.0f * phase;
        set_direction(light, azimuth, elevation);

        // Fade the shadows out near the horizon so the sun and moon hand over without a jump.
        const float strength = smoothstep(0.0f, 0.08f, arc);
        if (sun_up) {
            set_style(light, 0.45f, 0.49f, 0.62f, 0.74f, strength);
        }
        else {
            set_style(light, 0.50f, 0.53f, 0.66f, 0.80f, strength);
        }
    }
}

RT64::SceneLight quest64::scene_light_from_game(const uint8_t* rdram) {
    RT64::SceneLight light;
    if (rdram == nullptr) {
        return light;
    }

    const int map = read_s32(rdram, gNextMap);
    const int room = read_s32(rdram, gNextSubmap);
    const RoomLight& room_light = find_room_light(map, room);

    switch (room_light.kind) {
        case LightKind::Outdoor: {
            float day_length = float(read_s32(rdram, gDayLength));
            if (!(day_length > 0.0f)) {
                day_length = DefaultDayLength;
            }

            // The lighting time is what the room's own lights and fog are blended from.
            int32_t time = read_s32(rdram, gLightTime);
            if ((time < 0) || (float(time) >= day_length)) {
                time = read_s32(rdram, gCurrentTime);
            }
            const float wrapped = std::fmod(std::fmod(float(time), day_length) + day_length, day_length);
            set_sky_light(light, room_light, wrapped, day_length);
            break;
        }
        case LightKind::Interior:
            set_direction(light, room_light.yaw, room_light.pitch);
            set_style(light, 0.60f, 0.56f, 0.52f, 0.80f, 1.0f);
            break;
        case LightKind::Castle:
            set_direction(light, room_light.yaw, room_light.pitch);
            set_style(light, 0.56f, 0.56f, 0.60f, 0.78f, 1.0f);
            break;
        case LightKind::Cave:
            set_direction(light, room_light.yaw, room_light.pitch);
            set_style(light, 0.52f, 0.56f, 0.66f, 0.74f, 1.0f);
            break;
        case LightKind::Mammon:
            set_direction(light, room_light.yaw, room_light.pitch);
            set_style(light, 0.58f, 0.50f, 0.66f, 0.78f, 1.0f);
            break;
    }

    return light;
}
