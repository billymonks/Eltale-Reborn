// The field and battle camera modes, for both controls styles (the Controls page).
//
// Classic controls run the original camera, unchanged: hold B to swing behind Brian, N64 L turns the battle camera 45
// degrees, the battle aim moves in 22.5-degree steps.
//
// Modern controls, with the controls page's camera actions:
// - Free camera (the camera stick, the right stick by default), in towns, fields and dungeons: push it and the camera
//   leaves the original leash and orbits Brian at the angle you set, X around him and Y up and down around the point it
//   looks at, keeping that angle as he moves. In battle the stick turns the battle camera and tilts it.
// - Focus camera (LT by default): a tap swings the camera behind Brian in 8 frames and stops, and ends the free
//   camera; holding it keeps the camera behind him, letting go while the stick points back toward the camera so Brian
//   can walk toward it. In battle, holding it keeps the battle camera behind Brian; it stays there when released.
//   B doesn't move the camera (the original's follow button).
// - Zoom out (LB by default): toggles the field camera between the room's distance and twice it (the eye raised by half
//   the added distance, the game's own ratio for rooms with a second preset), not indoors and not in battle. The zoom
//   backs off where its eye would sit on a roof, and tries again at full on each new area.
// - The battle camera's aim turns smoothly toward its true direction, at most about 3 degrees a frame, instead of
//   snapping to 22.5-degree steps (which the tighter battle follow would show as a jitter as Brian walks).
// - Brian's staff swings and spells: their camera starts along the battle camera's direction, so with the camera turned
//   aside it showed the action from the side or the front. It swings round behind Brian and levels, and the battle
//   camera comes back behind him afterwards.
// - After an escape Brian turns back to his pre-battle facing, as he does after a win.
//
// CameraMode_Field (func_80013008) and CameraMode_Battle (func_80013A20) are identical, instruction for instruction, to
// the Japanese release's (Eltale Monsters), whose matched decompilation this follows; every game address below was
// paired instruction by instruction between the two ROMs. Mods can still hook both functions.
#include "patches.h"
#include "input.h"

typedef struct {
    f32 x, y, z;
} QVec3;

typedef struct {
    QVec3 pos;          // 0x00
    f32 xrot;           // 0x0C
    f32 yrot;           // 0x10
} QActorTransform;      // the head of the game's actor transform (0x68 bytes)

typedef struct {
    f32 x, y, z;        // in: x = 4.0 (the probe radius); out: the point moved out of walls, y = the floor
    f32 sn_x, sn_y, sn_z;
    u16 flags;          // 0x18: 0x8000 = no floor
} QFloorPoll;

typedef struct {
    s32 mode;           // 0x00: 0 / 2 field, 1 fixed, 3 fixed aim, 4 battle, 5 after battle
    s32 timer;          // 0x04
    u16 flags;          // 0x08: 0x04 turn toward orbit_yaw, 0x20 orbit_yaw = behind the target, 0x40 spell camera
    s16 unkA;
    f32 cam_x, cam_y, cam_z;            // 0x0C
    f32 focus_x, focus_y, focus_z;      // 0x18
    f32 pitch;          // 0x24
    f32 target_yaw;     // 0x28: Brian's walking is turned by this (BrianFieldMovement)
    f32 yaw;            // 0x2C
    f32 unk30;
    f32 focus_speed_y;  // 0x34
    f32 unk38;
    f32 orbit_speed;    // 0x3C: yaw velocity; CameraTurnToAngle: v = (v + orbit_pull * error) * orbit_drag
    f32 cam_speed_x, cam_speed_y, cam_speed_z;  // 0x40
    f32 ideal_planar_distance;          // 0x4C
    f32 unk50;
    f32 orbit_yaw;      // 0x54
    f32 local_camera_height;            // 0x58: the eye's height over the target
    f32 local_focus_height;             // 0x5C: the look-at point's
    f32 min_angle_x, max_angle_x, min_angle_y, max_angle_y;
    f32 pull;           // 0x70
    f32 drag;           // 0x74
    f32 orbit_pull;     // 0x78
    f32 orbit_drag;     // 0x7C
    QActorTransform* target_transform;  // 0x80
    f32 unk84;
} QCamera;              // 0x88

// Game state (Quest 64 US addresses).
#define gGameState (*(volatile s32*)0x8007B2E4)        // 0x01 item menu, 0x02 / 0x80 a box or a scene change
#define gBattleState (*(volatile u16*)0x8008C592)      // 0x01 in battle
#define gButtonsHeld (*(volatile u16*)0x80092874)      // the N64 buttons (Classic controls' camera reads them)
#define gButtonsPressed (*(volatile u16*)0x80092876)
// B_BUTTON (0x4000) and L_TRIG (0x0020) come from libultra (os_cont.h).
#define gStickX (*(volatile s8*)0x80092871)
#define gStickY (*(volatile s8*)0x80092872)
#define gSubmapHeader (*(volatile u32*)0x80084F1C)     // the current room's data: changes with every area
#define gBrianY (*(volatile f32*)0x8007BAD0)           // Brian's height (gPlayerState's transform)
#define BRIAN_TRANSFORM 0x8007BACCu                     // Brian's transform (its pos.y, verified, is at +4)
#define gCameraFloorPoll (*(QFloorPoll*)0x80086BA0)
#define gCameraFloorFlags (*(volatile u16*)0x80086BBC)
#define gDefaultCamera (*(QCamera*)0x80086B00)
#define gBattleCameraYawOffset (*(volatile f32*)0x80086EFC)
#define gBattleCenterX (*(volatile f32*)0x8008C59C)
#define gBattleCenterZ (*(volatile f32*)0x8008C5A0)
// Brian's state block (Quest's PlayerState, 0x8007BAB8: UpdateBrian passes it to the state handlers in the table at
// 0x8004C230, with his transform at +0x14). State 15 (0x80004BD0) stands him still and, in its last 0x26 frames, turns
// him 20% of the way a frame to the facing saved at battle start (+0x130, written at 0x80003B38), then state 0.
#define gBrianState (*(volatile u16*)0x8007BAB8)
#define gBrianTimer (*(volatile u16*)0x8007BABC)
#define BRIAN_STATE_FIELD 0
#define BRIAN_STATE_TURN_BACK 15

// Game functions.
void func_80013D30(QCamera* cam);                                       // (empty) the camera update's first call
void func_80013008(QCamera* cam);                                       // CameraMode_Field (this file's patch)
void func_80013DE0(QCamera* cam, QVec3* target);                        // CameraTurnToAngle
void func_8000EE60(f32 x, f32 y, f32 z, u16 flags, QFloorPoll* out);    // PollWorldCollision
f32 func_80014720(QCamera* cam, f32 height);                            // CameraAboveNpcs
void func_80013D3C(QCamera* cam, void* target);                         // CameraFollowHeight
f32 func_80023210(f32 y, f32 x);                                        // Atan2 (atan2(y, x))

#define PI 3.141592653589793
#define TWO_PI 6.283185307179586
#define DEG (PI / 180.0)
#define STYLE_CLASSIC 0         // recomp_get_controls_style()

// ---- tuning (Modern controls; tuned in play, most of it first in the Eltale English patch) ----
#define SNAP_FRAMES 8           // a tap's swing
#define SNAP_PULL 0.40f         // pull 0.40, brake 0: 40% of the turn on the first frame, a dead stop
#define FIELD_PULL 0.005f       // the field camera's own yaw gains (area entry)
#define FIELD_DRAG 0.8f
#define BATTLE_PULL 0.5f        // battle: a tight follow, so turning doesn't drift
#define BATTLE_DRAG 0.4f
#define BATTLE_TURN 0.10f       // rad/frame on the battle camera's yaw offset at full tilt
#define TRIGGER_ON 0.5f         // an analog trigger counts as pressed past half its travel
#define ZOOM_OUT 1.0f           // zoomed out: this times the preset's distance added
#define ZOOM_RISE 0.5f          // the eye rises this share of the added distance
#define ZOOM_EASE 0.15f         // the zoom moves this share of the way each frame
#define ROOF_H 15.0f            // a floor this much higher under the zoomed eye than under the preset eye: a roof
#define ROOF_STEP 0.05f         // the zoom's cap drops this much a frame on a roof
#define ZOOM_MIN 0.2f
#define NO_FLOOR -1.0e9f
#define FREE_ENTER 0.25f        // a push this far starts the free camera (pads drift; the stick's dead zone may be 0)
#define FREE_DEAD 0.10f         // inside it, smaller pushes are ignored
#define FREE_YAW_RATE 0.069f    // rad/frame at full tilt (about 4 degrees)
#define FREE_PITCH_RATE 0.044f  // rad/frame at full tilt
#define FREE_FOLLOW 0.5f        // the eye moves this share of the way to its place on the circle each frame
#define TILT_MAX (70.0 * DEG)   // the eye at most this far above the look-at point's level
#define TILT_MIN (-40.0 * DEG)  // and at most this far below it, looking up
#define EYE_MIN_H 5.0f          // the eye stays this high over Brian: looking up past that, it slides in along the
#define EYE_MIN_D 12.0f         // ground toward him instead (to this distance)
#define FAST_Y 0.35f            // the eye's height moves this share of the way each frame in the free camera and right
#define FAST_Y_FRAMES 30        // after a snap (the original spring moves it 0.02 x 0.2 going down: far too slow)
#define AIM_K 0.0016f           // the battle aim turns at most this x Brian's distance from the arena's centre (rad) a
#define AIM_DCAP 33.0f          // frame, the distance capped here: about 3 degrees a frame at most
#define SIDE_MIN 0.35f          // an action camera this far (20 degrees) or more off behind Brian swings behind him,
#define SWING_EASE 0.2f         // this share of the way each frame (90% there in about 10 frames)
#define LOCK_LEAD 0.157f        // holding the lock on in battle, the camera's aim leads its eye by at most this (9
                                // degrees: about 3 degrees a frame at the battle gains)
#define ESCAPE_TURN_FRAMES 0x1C // an escape's turn back to the pre-battle facing: 28 frames

static f32 wrap(f32 a) {
    if (a > PI) {
        a -= TWO_PI;
    } else if (a < -PI) {
        a += TWO_PI;
    }
    return a;
}

static f32 clampf(f32 v, f32 lo, f32 hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// *dst = *src for a camera, a word at a time. A plain struct copy becomes a memcpy call, which patches can't make.
static void copy_camera(QCamera* dst, const QCamera* src) {
    volatile u32* d = (volatile u32*)dst;
    const volatile u32* s = (const volatile u32*)src;
    u32 i;
    for (i = 0; i < sizeof(QCamera) / sizeof(u32); i++) {
        d[i] = s[i];
    }
}

// ---- the original CameraMode_Field's steps, shared by both styles ----

// The leash: the eye pulled toward the ideal distance from the target; then, inside 25 units, the eye swung aside so
// the target is seen from the side, or with flag 0x20 round behind it; then CameraTurnToAngle. Returns whether the eye
// was swung aside (the walking direction is then kept).
static u8 field_follow(QCamera* cam, QActorTransform* target) {
    u8 behind = 0;
    f32 dx = target->pos.x - cam->cam_x;
    f32 dz = target->pos.z - cam->cam_z;
    f32 dist = sqrtf(dx * dx + dz * dz);
    f32 ratio, diff;
    f32 pushX = 0.0f;
    f32 pushZ = 0.0f;

    if (dist != 0.0f) {
        if (cam->ideal_planar_distance < dist) {
            ratio = cam->ideal_planar_distance / dist;
            pushX = (dx - dx * ratio) * cam->pull;
            pushZ = (dz - dz * ratio) * cam->pull;
        } else if (dist <= cam->ideal_planar_distance) {
            ratio = cam->ideal_planar_distance / dist;
            pushX = (dx - dx * ratio) * cam->pull * 0.5;
            pushZ = (dz - dz * ratio) * cam->pull * 0.5;
        }
    }
    cam->cam_speed_x += pushX;
    cam->cam_speed_z += pushZ;
    cam->cam_x += cam->cam_speed_x;
    cam->cam_z += cam->cam_speed_z;
    cam->cam_speed_x *= cam->drag;
    cam->cam_speed_z *= cam->drag;
    if (dist < 25.0f && !(gGameState & 0x80)) {
        diff = wrap(target->yrot - cam->target_yaw);
        if (diff < 1.5707963267948966 && diff > -1.5707963267948966) {
            cam->orbit_yaw = ((diff < 0.0f) ? 1.5707963267948966 : -1.5707963267948966) + target->yrot;
        }
        cam->flags |= 4;
        behind = 1;
    } else if (cam->flags & 0x20) {
        cam->orbit_yaw = wrap(target->yrot + PI);
    }
    func_80013DE0(cam, &target->pos);
    return behind;
}

// The eye's height: the floor under the eye (the poll also moves the eye out of walls), at least the target's height
// plus the eye's own, above any NPC, not below Brian in battle; then the original spring toward it (faster up than
// down), or with `fast` an ease straight there.
static void field_height(QCamera* cam, QActorTransform* target, s32 fast) {
    f32 floor, want, dy;

    gCameraFloorPoll.x = 4.0f;
    func_8000EE60(cam->cam_x, cam->cam_y, cam->cam_z, 0x602, &gCameraFloorPoll);
    cam->cam_x = gCameraFloorPoll.x;
    cam->cam_z = gCameraFloorPoll.z;
    if (gCameraFloorPoll.flags & 0x8000) {
        floor = target->pos.y;
    } else {
        floor = gCameraFloorPoll.y;
    }
    gCameraFloorFlags = gCameraFloorPoll.flags;
    if (floor < target->pos.y + cam->local_camera_height) {
        floor = target->pos.y + cam->local_camera_height;
    }
    want = func_80014720(cam, floor);
    if ((gBattleState & 1) && want < gBrianY) {
        want = gBrianY;
    }
    dy = want - cam->cam_y;
    if (fast) {
        cam->cam_y += dy * FAST_Y;
        cam->cam_speed_y = 0.0f;
    } else {
        dy *= cam->pull * ((dy < 0.0f) ? 0.2 : 0.8);
        cam->cam_speed_y += dy;
        cam->cam_y += cam->cam_speed_y;
        cam->cam_speed_y *= cam->drag;
    }
}

// The look-at point follows the target (CameraFollowHeight); the camera's pitch and yaw come from the eye and that
// point, and Brian's walking direction from the yaw unless the eye was swung aside.
static void field_look(QCamera* cam, QActorTransform* target, u8 behind) {
    f32 dx, dy, dz, dist;

    func_80013D3C(cam, target);
    dx = cam->cam_x - cam->focus_x;
    dy = cam->cam_y - cam->focus_y;
    dz = cam->cam_z - cam->focus_z;
    if (dx > -0.5 && dx < 0.5 && dz > -0.5 && dz < 0.5) {
        cam->cam_x += 0.5;
        cam->cam_z += 0.5;
    }
    dist = sqrtf(dx * dx + dz * dz);
    if (dy != 0.0f || dist != 0.0f) {
        cam->pitch = func_80023210(dy, dist);
    }
    if (dx != 0.0f || dz != 0.0f) {
        cam->yaw = func_80023210(dx, dz);
        if (!behind) {
            cam->target_yaw = cam->yaw;
        }
    }
}

// ---- Classic controls: the original camera ----

// The original CameraMode_Field: hold B (or camera mode 2) to swing behind the target.
static void classic_field(QCamera* cam) {
    QActorTransform* target = cam->target_transform;
    u8 behind;

    func_80013D30(cam);
    if (!(gGameState & 0x82) && !(gBattleState & 0x200) && !(cam->flags & 0x40)) {
        if ((gButtonsHeld & B_BUTTON) || cam->mode == 2) {
            cam->flags |= 0x24;
        } else {
            cam->flags &= ~0x24;
        }
    } else {
        cam->flags &= ~0x24;
    }
    behind = field_follow(cam, target);
    field_height(cam, target, 0);
    field_look(cam, target, behind);
}

// The original battle camera's aim: from the arena's centre past Brian, in 22.5-degree steps (before its offset).
static f32 classic_battle_base(QActorTransform* subject) {
    f32 angle = wrap(func_80023210(gBattleCenterX - subject->pos.x, gBattleCenterZ - subject->pos.z) + PI);
    angle = (s32)(angle / 0.39269908169872414) * 0.39269908169872414 - 0.19634954084936207;
    return wrap(angle);
}

// The original CameraMode_Battle: N64 L turns the camera 45 degrees; an action camera (flag 0x40) counts down, then
// the saved battle camera comes back.
static void classic_battle(QCamera* cam) {
    f32 angle;
    QActorTransform* subject;

    if (cam->flags & 0x40) {
        cam->timer--;
        if (cam->timer == 0) {
            cam->flags &= ~0x40;
            copy_camera(cam, &gDefaultCamera);
        }
    } else {
        subject = cam->target_transform;
        angle = wrap(classic_battle_base(subject) + gBattleCameraYawOffset);
        cam->orbit_yaw = angle;
        if (gButtonsPressed & L_TRIG) {
            gBattleCameraYawOffset = wrap(gBattleCameraYawOffset + 0.7853981633974483);
        }
    }
    func_80013008(cam);     // CameraMode_Field (classic_field, through the patch), as the original does
}

// ---- Modern controls: the camera stick and the focus camera action ----

// The camera stick, each axis -1..1 (y positive DOWN), with `dead` taken out and the rest rescaled.
static void camera_stick(f32 dead, f32* x, f32* y) {
    f32 sx, sy, mag, scale;
    recomp_get_camera_inputs(&sx, &sy);
    mag = sqrtf(sx * sx + sy * sy);
    if (mag <= dead) {
        *x = 0.0f;
        *y = 0.0f;
        return;
    }
    scale = (mag > 1.0f ? 1.0f : mag);
    scale = (scale - dead) / (1.0f - dead) / mag;
    *x = sx * scale;
    *y = sy * scale;
}

static u8 sTargetHeld;          // this frame's state, refreshed once per CameraMode_Field call
static u8 sTargetPrev;
static s32 sSnapFrames;

static void read_target(void) {
    sTargetPrev = sTargetHeld;
    sTargetHeld = recomp_get_action_input(RECOMP_ACTION_CAMERA_TARGET) > TRIGGER_ON;
}

// The stick pulled back toward the camera: more than about 117 degrees from forward (-2y > |x| + 16, which keeps the
// game's dead zone of 8). Following then would turn "back" with the camera and the two would chase each other round.
static int stick_pulled_back(void) {
    s32 x = gStickX, y = gStickY;
    if (x < 0) {
        x = -x;
    }
    return 2 * y < -(x + 16);
}

// ---- Modern controls: the field zoom ----
static s32 sZoomOn;
static u8 sZoomPrev;
static f32 sZoom;               // 0 = the preset, 1 = fully zoomed out (eased)
static f32 sZoomCap = 1.0f;     // how far this place lets the zoom go
static u32 sZoomArea;
static QFloorPoll sProbe;

static f32 floor_at(f32 x, f32 y, f32 z) {
    sProbe.x = 4.0f;
    func_8000EE60(x, y, z, 0x602, &sProbe);
    if (sProbe.flags & 0x8000) {
        return NO_FLOOR;
    }
    return sProbe.y;
}

// The zoom's distance and eye height for this update, on top of whatever camera preset the room has active. `who` is
// the camera's target (Brian in the field). view_pre saves and view_post restores the preset's values.
static void zoom_apply(QCamera* cam, QActorTransform* who) {
    f32 want, dx, dz, len, d0, h0, d1, h1, f0, f1;
    f32 bx = who->pos.x, by = who->pos.y, bz = who->pos.z;
    u8 held = recomp_get_action_input(RECOMP_ACTION_CAMERA_ZOOM) > TRIGGER_ON;

    if (gSubmapHeader != sZoomArea) {
        sZoomArea = gSubmapHeader;
        sZoomCap = 1.0f;
    }
    if (held && !sZoomPrev && !(gGameState & 0x87) && !(gBattleState & 0x201)) {
        sZoomOn = !sZoomOn;
        sZoomCap = 1.0f;
    }
    sZoomPrev = held;
    d0 = cam->ideal_planar_distance;
    h0 = cam->local_camera_height;
    if (sZoomOn) {
        dx = cam->cam_x - bx;
        dz = cam->cam_z - bz;
        len = sqrtf(dx * dx + dz * dz);
        if (len > 1.0f) {
            dx /= len;
            dz /= len;
            d1 = d0 * (1.0f + ZOOM_OUT * sZoomCap);
            h1 = h0 + d0 * ZOOM_OUT * ZOOM_RISE * sZoomCap;
            f1 = floor_at(bx + dx * d1, by + h1, bz + dz * d1);
            f0 = floor_at(bx + dx * d0, by + h0, bz + dz * d0);
            if (f0 == NO_FLOOR) {
                f0 = by;    // nothing under the preset eye: Brian's own floor is the reference
            }
            if (f1 != NO_FLOOR && f1 - f0 > ROOF_H) {
                sZoomCap -= ROOF_STEP;
                if (sZoomCap < ZOOM_MIN) {
                    sZoomCap = ZOOM_MIN;
                }
            }
        }
    }
    want = sZoomOn ? sZoomCap : 0.0f;
    sZoom += (want - sZoom) * ZOOM_EASE;
    if (sZoom < 0.001f) {
        sZoom = 0.0f;
        return;
    }
    cam->ideal_planar_distance = d0 * (1.0f + ZOOM_OUT * sZoom);
    cam->local_camera_height = h0 + d0 * ZOOM_OUT * ZOOM_RISE * sZoom;
}

// ---- Modern controls: the free camera and the tilt ----
static s32 sFree;               // the free camera is on
static f32 sFreeYaw;            // the eye's angle around Brian, atan2 of (eye - Brian) in (x, z), like the game's yaw
static f32 sTilt;               // the stick's tilt, added to the preset's angle of the eye over the look-at point
static u32 sFreeArea;
static s32 sWasBattle;
static s32 sFastYFrames;        // frames of fast eye height left after a snap
static s32 sEscapeTurn;         // Brian escaped a battle and still has to turn back to his pre-battle facing

static void free_leave(QCamera* cam) {
    if (sFree) {
        sFree = 0;
        cam->orbit_speed = 0.0f;
        cam->cam_speed_x = 0.0f;
        cam->cam_speed_z = 0.0f;
    }
}

// The tilt: the eye turned up or down around the look-at point at the same distance from it, starting from the
// (zoomed) preset, between TILT_MIN and TILT_MAX; sTilt is kept within that, so pushing past a limit doesn't wind up.
// Looking up, the eye would sink into the ground: below EYE_MIN_H over Brian it stays at that height and slides in
// toward him along the line of sight instead (down to EYE_MIN_D).
static void tilt_apply(QCamera* cam) {
    f32 fh = cam->local_focus_height;
    f32 d = cam->ideal_planar_distance;
    f32 v = cam->local_camera_height - fh;
    f32 r = sqrtf(d * d + v * v);
    f32 a0, lo, hi, a, h;
    if (r < 1.0f) {
        return;
    }
    a0 = func_80023210(v, d);
    lo = TILT_MIN;
    hi = TILT_MAX;
    if (lo > a0) {
        lo = a0;    // a preset already beyond a limit: never force the eye past it
    }
    if (hi < a0) {
        hi = a0;
    }
    sTilt = clampf(sTilt, lo - a0, hi - a0);
    if (sTilt == 0.0f) {
        return;
    }
    a = a0 + sTilt;
    d = r * cosf(a);
    h = fh + r * sinf(a);
    if (h < EYE_MIN_H) {
        f32 vv = EYE_MIN_H - fh;                // the eye's height over the look-at point at the ground limit
        f32 s = sinf(a);
        h = EYE_MIN_H;
        if (vv < 0.0f && s < 0.0f) {
            d = vv * cosf(a) / s;               // the same line of sight, met at that height: closer in
        }
        d = clampf(d, EYE_MIN_D, r);
    }
    cam->ideal_planar_distance = d;
    cam->local_camera_height = h;
}

// The preset's distance and eye height, saved before the zoom and the tilt change them for this update and put back
// before the look-at spring, so they are never left changed.
static f32 sSaveD, sSaveH;
static s32 sViewSet;

static void view_pre(QCamera* cam, QActorTransform* target, s32 field, s32 battle) {
    sSaveD = cam->ideal_planar_distance;
    sSaveH = cam->local_camera_height;
    sViewSet = 1;
    if (field) {
        zoom_apply(cam, target);
    }
    if (field || battle) {
        tilt_apply(cam);
    }
}

static void view_post(QCamera* cam) {
    if (sViewSet) {
        cam->ideal_planar_distance = sSaveD;
        cam->local_camera_height = sSaveH;
        sViewSet = 0;
    }
}

// ---- Modern controls: the battle camera's aim, action cameras and lock on ----
static s32 sAimSet;             // sAimBase holds this battle's aim
static f32 sAimBase;            // the aim's direction from the arena's centre past Brian, turned toward it smoothly
static s32 sActCam;             // the action camera (camera flag 0x40) running now: 0 none, 1 Brian's, swinging behind
                                // him, 2 Brian's, already behind, 3 another's (an enemy's attack or spell)
static s32 sActTimer;           // its timer at the last frame: a larger one means it began again (a combo)
static f32 sActBehind;          // the direction behind Brian when it began
static u8 sLockPrev;            // the lock-on action was held at the last battle frame
static s32 sLockSnapFrames;     // frames left of a lock-on press's full-speed swing behind Brian

// SnapSpellCamera (Brian's staff swing, his spells, and the enemies' actions) saves the battle camera to
// gDefaultCamera, sets flag 0x40, and places the eye along the camera's orbit_yaw from the actor; CameraMode_Battle
// puts the saved one back when its timer runs out. For Brian's own (the spell camera's target is a copy of his
// position), the eye swings round behind him, and the saved battle camera and its offset turn behind him too, so it
// stays there after.
static void act_begin(QCamera* cam) {
    QActorTransform* t = cam->target_transform;
    QActorTransform* b = (QActorTransform*)BRIAN_TRANSFORM;
    QCamera* s = &gDefaultCamera;
    f32 dx = t->pos.x - b->pos.x;
    f32 dz = t->pos.z - b->pos.z;
    f32 dev, ex, ez, r, turn;

    sActTimer = cam->timer;
    if (dx < -2.0f || dx > 2.0f || dz < -2.0f || dz > 2.0f) {
        sActCam = 3;
        return;
    }
    sTilt = 0.0f;                   // level, for the action and after it
    sActBehind = wrap(b->yrot + PI);
    dev = wrap(sActBehind - cam->orbit_yaw);
    if (sActCam != 1 && dev > -SIDE_MIN && dev < SIDE_MIN) {    // (a combo's next hit mid-swing keeps swinging)
        sActCam = 2;
        return;
    }
    sActCam = 1;
    ex = s->cam_x - b->pos.x;
    ez = s->cam_z - b->pos.z;
    r = sqrtf(ex * ex + ez * ez);
    turn = wrap(sActBehind - func_80023210(ex, ez));
    s->cam_x = b->pos.x + sinf(sActBehind) * r;
    s->cam_z = b->pos.z + cosf(sActBehind) * r;
    s->yaw = wrap(s->yaw + turn);
    s->target_yaw = wrap(s->target_yaw + turn);
    gBattleCameraYawOffset = wrap(gBattleCameraYawOffset + wrap(sActBehind - s->orbit_yaw));
    s->orbit_yaw = sActBehind;
    s->orbit_speed = 0.0f;
}

// One frame of the swing: the eye turns round the action's target SWING_EASE of the way to behind Brian, at the same
// distance. orbit_yaw follows, so a combo's next SnapSpellCamera places the eye where the swing has got to.
static void act_swing(QCamera* cam) {
    QVec3* p = &cam->target_transform->pos;
    f32 ex = cam->cam_x - p->x;
    f32 ez = cam->cam_z - p->z;
    f32 r = sqrtf(ex * ex + ez * ez);
    f32 a = func_80023210(ex, ez);
    a = wrap(a + wrap(sActBehind - a) * SWING_EASE);
    cam->cam_x = p->x + sinf(a) * r;
    cam->cam_z = p->z + cosf(a) * r;
    cam->orbit_yaw = a;
    cam->orbit_speed = 0.0f;
}

// ---- switching styles ----
// Checked at the top of both camera modes. Switching to Modern clears what the modern camera keeps between frames;
// switching to Classic puts back the area's own yaw gains (Modern writes the battle pair every frame) and stops the
// camera's motion. Mid-battle, Classic also starts from the original's own framing: Modern's yaw offset is relative to
// its smooth aim and would point the original camera anywhere, and the original's slow follow would take seconds to
// come round, so the eye is placed there at once.
static s32 sStyleSeen = -1;

static s32 style_sync(QCamera* cam) {
    s32 style = recomp_get_controls_style();
    if (style != sStyleSeen) {
        if (style == STYLE_CLASSIC) {
            if (sStyleSeen >= 0 && (cam->mode == 0 || cam->mode == 2 || cam->mode == 4)) {
                cam->orbit_pull = FIELD_PULL;
                cam->orbit_drag = FIELD_DRAG;
                cam->orbit_speed = 0.0f;
                cam->cam_speed_x = 0.0f;
                cam->cam_speed_y = 0.0f;
                cam->cam_speed_z = 0.0f;
            }
            if (sStyleSeen >= 0 && cam->mode == 4 && !(cam->flags & 0x40)) {
                QActorTransform* subject = cam->target_transform;
                f32 angle = classic_battle_base(subject);
                f32 dx = cam->cam_x - subject->pos.x;
                f32 dz = cam->cam_z - subject->pos.z;
                f32 r = sqrtf(dx * dx + dz * dz);
                gBattleCameraYawOffset = 0.0f;
                cam->orbit_yaw = angle;
                cam->cam_x = subject->pos.x + sinf(angle) * r;
                cam->cam_z = subject->pos.z + cosf(angle) * r;
            }
        } else {
            sFree = 0;
            sTilt = 0.0f;
            sZoomOn = 0;
            sZoom = 0.0f;
            sSnapFrames = 0;
            sFastYFrames = 0;
            sAimSet = 0;
            sActCam = 0;
            sEscapeTurn = 0;
            sLockPrev = 0;
            sLockSnapFrames = 0;
            sWasBattle = cam->mode == 4;
        }
        sStyleSeen = style;
    }
    return style;
}

// ---- CameraMode_Field (camera modes 0 and 2; battle's mode 4 and the after-battle mode 5 end here too) ----
RECOMP_PATCH void func_80013008(QCamera* cam) {
    u8 behind = 0;
    f32 sx, sy;
    QActorTransform* target;
    s32 field = cam->mode == 0 || cam->mode == 2;
    s32 battle = cam->mode == 4;
    s32 action = cam->flags & 0x40;     // an action camera (spells, attacks): its own framing, no tilt
    s32 gated = (gGameState & 0x82) || (gBattleState & 0x200) || action;
    s32 is_brian;
    s32 snap_press = 0;
    s32 follow;
    f32 turn_speed, tilt_speed;         // the General page's camera speeds, 1 = the tuned rate

    if (style_sync(cam) == STYLE_CLASSIC) {
        classic_field(cam);
        return;
    }
    target = cam->target_transform;
    is_brian = (u32)target == BRIAN_TRANSFORM;
    func_80013D30(cam);
    read_target();
    recomp_get_camera_speed(&turn_speed, &tilt_speed);

    // Battle starts and ends reset the tilt, so a field tilt never carries into the battle framing or back.
    if (battle != sWasBattle) {
        // A battle's end: the after-battle camera (mode 5) swings back to where the camera looked before the fight.
        // A win turns Brian back to the facing he had then (state 15, with his victory animation); an escape (battle
        // flag 0x10) left him facing wherever he ran, so he turns back too, standing.
        if (sWasBattle && cam->mode == 5) {
            sEscapeTurn = (gBattleState & 0x10) != 0;
        }
        sTilt = 0.0f;
        sWasBattle = battle;
    }
    // The escape's turn starts once Brian is back on his feet in the field state (an escape by spell ends mid-cast),
    // while the after-battle camera runs.
    if (cam->mode != 5) {
        sEscapeTurn = 0;
    } else if (sEscapeTurn && gBrianState == BRIAN_STATE_FIELD) {
        gBrianState = BRIAN_STATE_TURN_BACK;
        gBrianTimer = ESCAPE_TURN_FRAMES;
        sEscapeTurn = 0;
    }
    // Every frame out of battle (the after-battle camera, mode 5, always runs here) forgets the battle's aim, so the
    // next battle starts from its true direction.
    if (!battle) {
        sAimSet = 0;
        sActCam = 0;
        sLockPrev = 0;
        sLockSnapFrames = 0;
    }

    // The free camera: on with a deliberate push, off outside the field, behind a gate, in another area, on another
    // target (scripted scenes), or with the snap (which then runs its press frame below).
    camera_stick(FREE_DEAD, &sx, &sy);
    if (!field || gated || !is_brian || gSubmapHeader != sFreeArea) {
        free_leave(cam);
    }
    if (field && !gated && is_brian && !(gGameState & 0x01) && !sFree) {
        f32 rx, ry;
        recomp_get_camera_inputs(&rx, &ry);
        if (rx * rx + ry * ry > FREE_ENTER * FREE_ENTER) {
            sFree = 1;
            sFreeArea = gSubmapHeader;
            sFreeYaw = func_80023210(cam->cam_x - target->pos.x, cam->cam_z - target->pos.z);
            sSnapFrames = 0;
        }
    }
    if (field && sTargetHeld && !sTargetPrev && !(gGameState & 0x01)) {
        free_leave(cam);
        sTilt = 0.0f;
    }
    if (sFree) {
        sFreeYaw = wrap(sFreeYaw - FREE_YAW_RATE * turn_speed * sx);    // stick right: the view turns right
    }
    if ((sFree || battle) && !gated) {
        sTilt += FREE_PITCH_RATE * tilt_speed * sy;      // stick up (y < 0) lowers the eye: the view tilts up
    }

    view_pre(cam, target, field, battle && !action);

    if (sFree) {
        cam->flags &= ~0x24;
    } else if (!gated) {
        // The snap, a tap of focus camera (field only, not while the item menu is open: its list scrolls with the same
        // triggers in some layouts). The press frame follows (0x24: orbit_yaw = Brian's heading + pi); the other snap
        // frames pull without 0x20, so the press frame's aim stays.
        if (field && !(gGameState & 0x01) && sTargetHeld && !sTargetPrev) {
            sSnapFrames = SNAP_FRAMES;
            sFastYFrames = FAST_Y_FRAMES;
            cam->orbit_speed = 0.0f;
            snap_press = 1;
        }
        if (!field) {
            sSnapFrames = 0;
        }
        if (sSnapFrames > 0) {
            follow = snap_press;
        } else if (field && sTargetHeld && !(gGameState & 0x01)) {
            follow = !stick_pulled_back();
        } else {
            follow = cam->mode == 2;    // camera mode 2's own follow; B doesn't move the camera (the snap does)
        }
        if (follow) {
            cam->flags |= 0x24;
        } else {
            cam->flags &= ~0x24;
        }
    } else {
        cam->flags &= ~0x24;
        sSnapFrames = 0;
    }

    if (sFree) {
        // On the circle at the chosen angle (the game's yaw: the eye at (sin, cos) x distance from the target), eased
        // there; the floor poll still keeps it out of walls. No leash, no swing aside up close, no turn physics.
        f32 want_x = target->pos.x + sinf(sFreeYaw) * cam->ideal_planar_distance;
        f32 want_z = target->pos.z + cosf(sFreeYaw) * cam->ideal_planar_distance;
        cam->cam_x += (want_x - cam->cam_x) * FREE_FOLLOW;
        cam->cam_z += (want_z - cam->cam_z) * FREE_FOLLOW;
        cam->cam_speed_x = 0.0f;
        cam->cam_speed_z = 0.0f;
        cam->orbit_speed = 0.0f;
    } else {
        // The yaw gains: the snap's, battle's, or the field's own every frame (the field's also for the after-battle
        // swing back, mode 5, at the original's slow pace).
        if (sSnapFrames > 0) {
            sSnapFrames--;
            cam->flags |= 0x04;
            cam->orbit_pull = SNAP_PULL;
            cam->orbit_drag = 0.0f;
        } else if (battle) {
            cam->orbit_pull = BATTLE_PULL;
            cam->orbit_drag = BATTLE_DRAG;
        } else {
            cam->orbit_pull = FIELD_PULL;
            cam->orbit_drag = FIELD_DRAG;
        }
        behind = field_follow(cam, target);
    }
    // The free camera, a tilted battle camera and the second after a snap: the eye's height eases straight to its
    // place, the same either way, so a tilt follows the stick and a snap's reset settles at once.
    field_height(cam, target, sFree || sFastYFrames > 0 || (battle && !action && sTilt != 0.0f));
    if (sFastYFrames > 0) {
        sFastYFrames--;
    }
    view_post(cam);
    field_look(cam, target, behind);    // Brian walks relative to the yaw, in the free camera too
}

// ---- CameraMode_Battle (camera mode 4) ----
// The battle camera aims from the arena's centre past Brian plus gBattleCameraYawOffset. The original snapped that
// direction to 22.5-degree steps; here it turns toward the true direction by at most AIM_K x min(distance, AIM_DCAP) a
// frame, so walking round a fight never jumps it (a trade-off: walking past the middle, the aim lags). The camera stick
// turns the offset (and tilts the camera, in CameraMode_Field); holding the focus camera action sets it so the camera
// sits behind Brian, and it stays there when released. While an action camera runs (flag 0x40) the aim holds still.
RECOMP_PATCH void func_80013A20(QCamera* cam) {
    f32 angle, sx, sy, dx, dz, d, lim, turn_speed, tilt_speed;
    u8 held;
    QActorTransform* subject;

    if (style_sync(cam) == STYLE_CLASSIC) {
        classic_battle(cam);
        return;
    }

    if (cam->flags & 0x40) {
        if (sActCam == 0 || cam->timer > sActTimer) {
            act_begin(cam);
        }
        if (sActCam == 1) {
            act_swing(cam);
        }
        cam->timer--;
        sActTimer = cam->timer;
        if (cam->timer == 0) {
            cam->flags &= ~0x40;
            copy_camera(cam, &gDefaultCamera);
            if (sActCam == 1 || sActCam == 2) {
                sFastYFrames = FAST_Y_FRAMES;   // the level eye height settles at once after Brian's action
            }
            sActCam = 0;
        }
    } else {
        sActCam = 0;
        subject = cam->target_transform;
        dx = gBattleCenterX - subject->pos.x;
        dz = gBattleCenterZ - subject->pos.z;
        angle = wrap(func_80023210(dx, dz) + PI);
        if (!sAimSet) {
            sAimBase = angle;
            sAimSet = 1;
        } else {
            d = sqrtf(dx * dx + dz * dz);
            lim = AIM_K * (d < AIM_DCAP ? d : AIM_DCAP);
            sAimBase = wrap(sAimBase + clampf(wrap(angle - sAimBase), -lim, lim));
        }
        angle = sAimBase;
        held = recomp_get_action_input(RECOMP_ACTION_CAMERA_TARGET) > TRIGGER_ON;
        if (held) {
            // Lock: behind Brian. A press swings there at full speed (SNAP_FRAMES); while it's held after that, the aim
            // may lead the eye by at most LOCK_LEAD. Brian walks relative to the camera, so a camera that followed his
            // facing at once would turn him further each frame, and the two would spin round.
            f32 behind = wrap(subject->yrot + PI);
            f32 want = behind;
            if (!sLockPrev) {
                sLockSnapFrames = SNAP_FRAMES;
            }
            if (sLockSnapFrames > 0) {
                sLockSnapFrames--;
            } else {
                f32 eye = func_80023210(cam->cam_x - subject->pos.x, cam->cam_z - subject->pos.z);
                want = wrap(eye + clampf(wrap(behind - eye), -LOCK_LEAD, LOCK_LEAD));
            }
            gBattleCameraYawOffset = wrap(want - angle);     // kept in step, so letting go leaves the view there
        } else {
            camera_stick(FREE_DEAD, &sx, &sy);
            recomp_get_camera_speed(&turn_speed, &tilt_speed);
            gBattleCameraYawOffset = wrap(gBattleCameraYawOffset - BATTLE_TURN * turn_speed * sx);
        }
        sLockPrev = held;
        cam->orbit_yaw = wrap(angle + gBattleCameraYawOffset);
    }
    func_80013008(cam);
}
