// Field (func_80013008) and battle (func_80013A20) camera modes.
// Classic controls run the original camera. Modern controls add the free camera on the camera stick, focus camera,
// zoom out, a smooth battle aim and action cameras that swing behind Brian.
#include "patches.h"
#include "input.h"

typedef struct {
    f32 x, y, z;
} QVec3;

typedef struct {
    QVec3 pos;          // 0x00
    f32 xrot;           // 0x0C
    f32 yrot;           // 0x10
} QActorTransform;

typedef struct {
    f32 x, y, z;        // x is the probe radius going in, y is the floor coming out
    f32 sn_x, sn_y, sn_z;
    u16 flags;          // 0x8000: no floor
} QFloorPoll;

typedef struct {
    s32 mode;           // 0x00: 0/2 field, 1 fixed, 3 fixed aim, 4 battle, 5 after battle
    s32 timer;          // 0x04
    u16 flags;          // 0x08: 0x04 turn to orbit_yaw, 0x20 orbit behind target, 0x40 action camera
    s16 unkA;
    f32 cam_x, cam_y, cam_z;            // 0x0C
    f32 focus_x, focus_y, focus_z;      // 0x18
    f32 pitch;          // 0x24
    f32 target_yaw;     // 0x28: Brian walks relative to this
    f32 yaw;            // 0x2C
    f32 unk30;
    f32 focus_speed_y;  // 0x34
    f32 unk38;
    f32 orbit_speed;    // 0x3C
    f32 cam_speed_x, cam_speed_y, cam_speed_z;  // 0x40
    f32 ideal_planar_distance;          // 0x4C
    f32 unk50;
    f32 orbit_yaw;      // 0x54
    f32 local_camera_height;            // 0x58
    f32 local_focus_height;             // 0x5C
    f32 min_angle_x, max_angle_x, min_angle_y, max_angle_y;
    f32 pull;           // 0x70
    f32 drag;           // 0x74
    f32 orbit_pull;     // 0x78
    f32 orbit_drag;     // 0x7C
    QActorTransform* target_transform;  // 0x80
    f32 unk84;
} QCamera;              // 0x88

#define gGameState (*(volatile s32*)0x8007B2E4)        // 0x01 item menu, 0x02/0x80 text box or scene change
#define gBattleState (*(volatile u16*)0x8008C592)      // 0x01 in battle
#define gButtonsHeld (*(volatile u16*)0x80092874)
#define gButtonsPressed (*(volatile u16*)0x80092876)
#define gStickX (*(volatile s8*)0x80092871)
#define gStickY (*(volatile s8*)0x80092872)
#define gSubmapHeader (*(volatile u32*)0x80084F1C)     // changes with every area
#define gBrianY (*(volatile f32*)0x8007BAD0)
#define BRIAN_TRANSFORM 0x8007BACCu
#define gCameraFloorPoll (*(QFloorPoll*)0x80086BA0)
#define gCameraFloorFlags (*(volatile u16*)0x80086BBC)
#define gDefaultCamera (*(QCamera*)0x80086B00)          // the battle camera saved during an action camera
#define gBattleCameraYawOffset (*(volatile f32*)0x80086EFC)
#define gBattleCenterX (*(volatile f32*)0x8008C59C)
#define gBattleCenterZ (*(volatile f32*)0x8008C5A0)
void func_80013D30(QCamera* cam);
void func_80013008(QCamera* cam);                                       // CameraMode_Field
void func_80013DE0(QCamera* cam, QVec3* target);                        // CameraTurnToAngle
void func_8000EE60(f32 x, f32 y, f32 z, u16 flags, QFloorPoll* out);    // PollWorldCollision
f32 func_80014720(QCamera* cam, f32 height);                            // CameraAboveNpcs
void func_80013D3C(QCamera* cam, void* target);                         // CameraFollowHeight
f32 func_80023210(f32 y, f32 x);                                        // atan2

#define PI 3.141592653589793
#define TWO_PI 6.283185307179586
#define DEG (PI / 180.0)
#define STYLE_CLASSIC 0

#define SNAP_FRAMES 16
#define SNAP_PULL 0.2f
#define FIELD_PULL 0.005f
#define FIELD_DRAG 0.8f
#define BATTLE_PULL 0.5f
#define BATTLE_DRAG 0.4f
#define BATTLE_TURN 0.10f
#define TRIGGER_ON 0.5f
#define ZOOM_OUT 1.0f
#define ZOOM_RISE 0.5f
#define ZOOM_EASE 0.15f
#define ROOF_H 15.0f
#define ROOF_STEP 0.05f
#define ZOOM_MIN 0.2f
#define NO_FLOOR -1.0e9f
#define FREE_ENTER 0.25f
#define FREE_DEAD 0.10f
#define FREE_YAW_RATE 0.069f
#define FREE_PITCH_RATE 0.044f
#define FREE_FOLLOW 0.5f
#define TILT_MAX (70.0 * DEG)
#define TILT_MIN (-40.0 * DEG)
#define EYE_MIN_H 5.0f
#define EYE_MIN_D 12.0f
#define FAST_Y 0.35f
#define FAST_Y_FRAMES 30
#define AIM_K 0.0016f
#define AIM_DCAP 33.0f
#define SIDE_MIN 0.35f
#define SWING_EASE 0.2f
#define LOCK_LEAD 0.157f
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

// A struct copy would compile to a memcpy call, which patches can't make.
static void copy_camera(QCamera* dst, const QCamera* src) {
    volatile u32* d = (volatile u32*)dst;
    const volatile u32* s = (const volatile u32*)src;
    u32 i;
    for (i = 0; i < sizeof(QCamera) / sizeof(u32); i++) {
        d[i] = s[i];
    }
}

// Returns whether the eye was swung aside up close, which keeps Brian's walking direction.
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

// With `fast` the eye eases straight to its height instead of using the original spring.
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

// From the arena's centre past Brian, in 22.5 degree steps.
static f32 classic_battle_base(QActorTransform* subject) {
    f32 angle = wrap(func_80023210(gBattleCenterX - subject->pos.x, gBattleCenterZ - subject->pos.z) + PI);
    angle = (s32)(angle / 0.39269908169872414) * 0.39269908169872414 - 0.19634954084936207;
    return wrap(angle);
}

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
    func_80013008(cam);
}

// Camera stick with the dead zone taken out, y positive down.
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

static u8 sTargetHeld;
static u8 sTargetPrev;
static s32 sSnapFrames;

static void read_target(void) {
    sTargetPrev = sTargetHeld;
    sTargetHeld = recomp_get_action_input(RECOMP_ACTION_CAMERA_TARGET) > TRIGGER_ON;
}

// Following the camera while walking toward it would spin Brian and the camera around each other.
static int stick_pulled_back(void) {
    s32 x = gStickX, y = gStickY;
    if (x < 0) {
        x = -x;
    }
    return 2 * y < -(x + 16);
}

static s32 sZoomOn;
static u8 sZoomPrev;
static f32 sZoom;
static f32 sZoomCap = 1.0f;
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

// Backs the zoom off while the zoomed eye would be over a roof.
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
                f0 = by;
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

static s32 sFree;
static f32 sFreeYaw;
static f32 sTilt;
static u32 sFreeArea;
static s32 sWasBattle;
static s32 sFastYFrames;
static void free_leave(QCamera* cam) {
    if (sFree) {
        sFree = 0;
        cam->orbit_speed = 0.0f;
        cam->cam_speed_x = 0.0f;
        cam->cam_speed_z = 0.0f;
    }
}

// Below EYE_MIN_H the eye slides in toward Brian along the line of sight instead of going into the ground.
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
        lo = a0;
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
        f32 vv = EYE_MIN_H - fh;
        f32 s = sinf(a);
        h = EYE_MIN_H;
        if (vv < 0.0f && s < 0.0f) {
            d = vv * cosf(a) / s;
        }
        d = clampf(d, EYE_MIN_D, r);
    }
    cam->ideal_planar_distance = d;
    cam->local_camera_height = h;
}

// The zoom and tilt only change the room's camera preset for one update.
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

static s32 sAimSet;
static f32 sAimBase;
static s32 sActCam;             // 0 none, 1 Brian's swinging behind him, 2 Brian's already behind, 3 someone else's
static s32 sActTimer;
static f32 sActBehind;
static u8 sLockPrev;
static s32 sLockSnapFrames;
static u8 sLockEasing;

// Brian's action cameras swing behind him, and the saved battle camera is turned to match so it stays there after.
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
    sTilt = 0.0f;
    sActBehind = wrap(b->yrot + PI);
    dev = wrap(sActBehind - cam->orbit_yaw);
    if (sActCam != 1 && dev > -SIDE_MIN && dev < SIDE_MIN) {
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

static s32 sStyleSeen = -1;

// Mods that draw their own battle camera still have Brian walking relative to the game's camera, so they can hold it to
// the original code.
static s32 sOriginalCamera;

RECOMP_EXPORT void recomp_set_original_camera(s32 on) {
    sOriginalCamera = on != 0;
}

// Switching to Classic mid-battle puts the camera straight at the original framing, since Modern's yaw offset is
// relative to its own aim.
static s32 style_sync(QCamera* cam) {
    s32 style = sOriginalCamera ? STYLE_CLASSIC : recomp_get_controls_style();
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
            sLockPrev = 0;
            sLockSnapFrames = 0;
            sLockEasing = 0;
            sWasBattle = cam->mode == 4;
        }
        sStyleSeen = style;
    }
    return style;
}

// Camera modes 0 and 2. The battle (4) and after battle (5) modes also end here.
RECOMP_PATCH void func_80013008(QCamera* cam) {
    u8 behind = 0;
    f32 sx, sy;
    QActorTransform* target;
    // Mode 5 holds for over a second after a battle before handing back to the field mode.
    s32 field = cam->mode == 0 || cam->mode == 2 || cam->mode == 5;
    s32 battle = cam->mode == 4;
    s32 action = cam->flags & 0x40;
    s32 gated = (gGameState & 0x82) || (gBattleState & 0x200) || action;
    s32 is_brian;
    s32 snap_press = 0;
    s32 follow;
    f32 turn_speed, tilt_speed;

    if (style_sync(cam) == STYLE_CLASSIC) {
        classic_field(cam);
        return;
    }
    target = cam->target_transform;
    is_brian = (u32)target == BRIAN_TRANSFORM;
    func_80013D30(cam);
    read_target();
    recomp_get_camera_speed(&turn_speed, &tilt_speed);

    if (battle != sWasBattle) {
        sTilt = 0.0f;
        sWasBattle = battle;
    }
    if (!battle) {
        sAimSet = 0;
        sActCam = 0;
        sLockPrev = 0;
        sLockSnapFrames = 0;
        sLockEasing = 0;
    }

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
        sFreeYaw = wrap(sFreeYaw - FREE_YAW_RATE * turn_speed * sx);
    }
    if ((sFree || battle) && !gated) {
        sTilt += FREE_PITCH_RATE * tilt_speed * sy;
    }

    view_pre(cam, target, field, battle && !action);

    if (sFree) {
        cam->flags &= ~0x24;
    } else if (!gated) {
        // Not while the item menu is open: some layouts scroll its list with the same trigger.
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
            follow = cam->mode == 2;
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
        f32 want_x = target->pos.x + sinf(sFreeYaw) * cam->ideal_planar_distance;
        f32 want_z = target->pos.z + cosf(sFreeYaw) * cam->ideal_planar_distance;
        cam->cam_x += (want_x - cam->cam_x) * FREE_FOLLOW;
        cam->cam_z += (want_z - cam->cam_z) * FREE_FOLLOW;
        cam->cam_speed_x = 0.0f;
        cam->cam_speed_z = 0.0f;
        cam->orbit_speed = 0.0f;
    } else {
        if (sSnapFrames > 0) {
            sSnapFrames--;
            cam->flags |= 0x04;
            cam->orbit_pull = SNAP_PULL;
            cam->orbit_drag = 0.0f;
        } else if (battle && sLockEasing) {
            // The battle spring overshoots a big swing.
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
    field_height(cam, target, sFree || sFastYFrames > 0 || (battle && !action && sTilt != 0.0f));
    if (sFastYFrames > 0) {
        sFastYFrames--;
    }
    view_post(cam);
    field_look(cam, target, behind);
}

// Camera mode 4.
RECOMP_PATCH void func_80013A20(QCamera* cam) {
    f32 angle, sx, sy, dx, dz, d, lim, turn_speed, tilt_speed;
    u8 held;
    QActorTransform* subject;

    if (style_sync(cam) == STYLE_CLASSIC) {
        classic_battle(cam);
        return;
    }

    if (cam->flags & 0x40) {
        sLockEasing = 0;
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
                sFastYFrames = FAST_Y_FRAMES;
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
            // Brian walks relative to the camera, so the lock trails his facing by at most LOCK_LEAD.
            f32 behind = wrap(subject->yrot + PI);
            f32 want = behind;
            if (!sLockPrev) {
                sLockSnapFrames = SNAP_FRAMES;
                cam->orbit_speed = 0.0f;
            }
            sLockEasing = sLockSnapFrames > 0;
            if (sLockSnapFrames > 0) {
                sLockSnapFrames--;
            } else {
                f32 eye = func_80023210(cam->cam_x - subject->pos.x, cam->cam_z - subject->pos.z);
                want = wrap(eye + clampf(wrap(behind - eye), -LOCK_LEAD, LOCK_LEAD));
            }
            gBattleCameraYawOffset = wrap(want - angle);
        } else {
            sLockEasing = 0;
            camera_stick(FREE_DEAD, &sx, &sy);
            recomp_get_camera_speed(&turn_speed, &tilt_speed);
            gBattleCameraYawOffset = wrap(gBattleCameraYawOffset - BATTLE_TURN * turn_speed * sx);
        }
        sLockPrev = held;
        cam->orbit_yaw = wrap(angle + gBattleCameraYawOffset);
    }
    func_80013008(cam);
}
