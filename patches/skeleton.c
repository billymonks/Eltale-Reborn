#include "patches.h"
#include "misc_funcs.h"

typedef float MtxF_t[4][4];
typedef union {
    MtxF_t mf;
    struct {
        float xx, yx, zx, wx,
              xy, yy, zy, wy,
              xz, yz, zz, wz,
              xw, yw, zw, ww;
    };
} MtxF;

typedef struct Unk5 {
    char unk_00[0x0C];
    f32 unk_0C;
    f32 unk_10;
    f32 unk_14;
} Unk5; //sizeof 0x18

typedef struct Unk6 {
    f32 unk_00;
    f32 unk_04;
    f32 unk_08;
    f32 unk_0C;
    f32 unk_10;
    f32 unk_14;
    f32 unk_18;
    f32 unk_1C;
    f32 unk_20;
} Unk6; //sizeof 0x24

typedef struct Unk8 {
    /* 0x00 */ char unk_00[6];
    /* 0x06 */ u16 unk_06;
    /* 0x08 */ char unk_08[12];
    /* 0x14 */ s8 unk_14;                            /* parent index + 1, 0 for roots */
    /* 0x15 */ s8 unk_15;
    /* 0x16 */ u8 unk_16;                            /* inferred */
    /* 0x17 */ char unk_17[1];
    /* 0x18 */ s32* unk_18;
    /* 0x1C */ u16 unk_1C;
    /* 0x1E */ char unk_1E[2];
} Unk8; //sizeof 0x20

typedef struct Unk9 {
    /* 0x00 */ s16 unk_00;
    /* 0x02 */ char unk_02[2];
    /* 0x04 */ Unk8* unk_04;
    /* 0x08 */ s32* unk_08;
} Unk9;

typedef struct Vec3f {
    f32 x;
    f32 y;
    f32  z;
} Vec3f;

// Position and orientation of an object drawn by func_8001DB38.
typedef struct SkelObject {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ f32 unk_0C;
    /* 0x10 */ f32 unk_10;
    /* 0x14 */ f32 unk_14;
    /* 0x18 */ char unk_18[0xC];
    /* 0x24 */ f32 unk_24;
    /* 0x28 */ char unk_28[0x2C];
    /* 0x54 */ u16 unk_54;
    /* 0x56 */ u16 unk_56;
} SkelObject;

typedef struct ActorShadow {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ f32 rotX;
    /* 0x10 */ f32 rotY;
    /* 0x14 */ f32 rotZ;
    /* 0x18 */ f32 scale;
} ActorShadow; // sizeof 0x1C

#define ACTOR_SHADOW_MAX 0x40

extern Unk5 D_80086DC0;
extern Vec3f D_8008C5B0[];
extern s32 D_8008C5E0;
extern MtxF D_8008C5E8;
extern Gfx* gMasterGfxPos;
extern Mtx D_2000000[];
extern s32 D_8007B2F8;
extern s16 D_8007B318;
extern s16 D_8007B31A;
extern ActorShadow gActorShadows[ACTOR_SHADOW_MAX];
extern u32 gNumShadowsToDrawThisFrame;

void func_8001E138(MtxF*, void*);
void func_80022B40(void* arg0, Unk6* arg1, s32 arg2);
void func_80023360(MtxF*, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7);
void func_8002371C(MtxF* arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6);
void func_80023A08(MtxF* arg0, f32 arg1, f32 arg2, f32 arg3);
void func_80023C1C(MtxF*, f32, f32, f32, f32, f32, f32, f32, f32, f32);
void func_80023DF4(MtxF* arg0, f32 arg1, f32 arg2, f32 arg3);
void func_80023E80(MtxF*, MtxF*, MtxF*);
void func_8001DC78(MtxF* arg0, Vec3f arg1, s32 arg4, s32 arg5, s32 arg6, s32 arg7, Unk9* arg8, Unk8* arg9, Mtx* arg10);

// Per-part frame interpolation for every model drawn through func_8001DC78:
// characters, NPCs and enemies via func_8001DB38, and spell models via
// func_8001A620. Each part gets a matrix group whose ID comes from the object
// that owns it and the part's index in the skeleton, so RT64 never has to guess
// which transform from the previous frame a part corresponds to. Characters'
// shadows are keyed by the same object.
//
// The group commands live in side display lists outside the game's own buffer,
// which has a fixed size. The game's list gets one call per part in place of
// the commands it had before, so it never grows.

#define SKEL_INTERP_STATE_COUNT 128
#define SKEL_INTERP_PROBES 16
#define SKEL_INTERP_MAX_PARTS 128
#define SKEL_SIDE_BANK_COUNT 3
#define SKEL_SIDE_LIST_COUNT 640
#define SKEL_SIDE_LIST_LENGTH 8
// Squared world-space distance a model's root may move between game frames
// before it's treated as a teleport rather than motion.
#define SKEL_SNAP_DISTANCE_SQ (256.0f * 256.0f)

typedef struct SkelInterpState {
    void* key;
    s32* model;
    u32 frame;
    f32 x;
    f32 y;
    f32 z;
    u32 drawn[SKEL_INTERP_MAX_PARTS / 32];
    u8 snap;
    u8 shadowDrawn;
} SkelInterpState;

typedef struct SkelSideBank {
    Mtx* owner;
    u32 frame;
    s32 count;
    Gfx lists[SKEL_SIDE_LIST_COUNT][SKEL_SIDE_LIST_LENGTH];
} SkelSideBank;

static SkelInterpState sSkelStates[SKEL_INTERP_STATE_COUNT];
static SkelSideBank sSkelBanks[SKEL_SIDE_BANK_COUNT];
static SkelSideBank* sSkelBank;
static u32 sSkelFrame;
static Mtx* sSkelLastMtx;
static s32 sSkelLastMtxIndex;
// Object passed to func_8001DB38 while it draws, so parts can be keyed by it.
static void* sSkelObject;
// Object most recently drawn by func_8001DB38; its caller queues its shadow next.
static void* sSkelShadowObject;
static void* sShadowObjects[ACTOR_SHADOW_MAX];

// The game fills a separate matrix buffer each frame and resets the matrix
// index once per frame, so either change marks a new game frame. Side lists are
// banked per matrix buffer so a frame's lists stay intact while it's rendered.
static void skel_interp_sync_frame(Mtx* mtx) {
    s32 i;

    if ((sSkelBank == NULL) || (mtx != sSkelLastMtx) || (D_8007B2F8 < sSkelLastMtxIndex)) {
        SkelSideBank* bank = NULL;
        SkelSideBank* oldest = &sSkelBanks[0];

        sSkelFrame++;
        for (i = 0; i < SKEL_SIDE_BANK_COUNT; i++) {
            if (sSkelBanks[i].owner == mtx) {
                bank = &sSkelBanks[i];
                break;
            }
            if (sSkelBanks[i].frame < oldest->frame) {
                oldest = &sSkelBanks[i];
            }
        }
        if (bank == NULL) {
            bank = oldest;
            bank->owner = mtx;
        }
        bank->frame = sSkelFrame;
        bank->count = 0;
        sSkelBank = bank;
    }
    sSkelLastMtx = mtx;
    sSkelLastMtxIndex = D_8007B2F8;
}

static SkelInterpState* skel_interp_find_state(void* key) {
    u32 hash = (((u32) key >> 2) * 2654435761U) >> 25;
    SkelInterpState* reuse = NULL;
    s32 i;

    for (i = 0; i < SKEL_INTERP_PROBES; i++) {
        SkelInterpState* state = &sSkelStates[(hash + i) % SKEL_INTERP_STATE_COUNT];

        if (state->key == key) {
            return state;
        }
        if ((reuse == NULL) && ((state->key == NULL) || (sSkelFrame - state->frame > 2))) {
            reuse = state;
        }
    }
    if (reuse != NULL) {
        bzero(reuse, sizeof(*reuse));
        reuse->key = key;
    }
    return reuse;
}

// Returns the side list to build a part's commands into, or NULL to draw the
// part untagged. Sets *snap when the part shouldn't blend from the last frame.
static Gfx* skel_interp_begin_part(void* key, s32* model, s32 part, Vec3f* pos, Mtx* mtx, s32* snap) {
    SkelInterpState* state;
    u32 bit;

    skel_interp_sync_frame(mtx);
    if ((part < 0) || (part >= SKEL_INTERP_MAX_PARTS) || (sSkelBank->count >= SKEL_SIDE_LIST_COUNT)) {
        return NULL;
    }
    state = skel_interp_find_state(key);
    if (state == NULL) {
        return NULL;
    }
    if (state->frame != sSkelFrame) {
        // First part of this object this frame, normally its root. Snap if it
        // wasn't drawn last frame, changed model, or jumped a long way.
        f32 dx = pos->x - state->x;
        f32 dy = pos->y - state->y;
        f32 dz = pos->z - state->z;

        state->snap = (state->frame + 1 != sSkelFrame) || (state->model != model) ||
                      ((dx * dx) + (dy * dy) + (dz * dz) > SKEL_SNAP_DISTANCE_SQ);
        state->frame = sSkelFrame;
        state->model = model;
        state->x = pos->x;
        state->y = pos->y;
        state->z = pos->z;
        bzero(state->drawn, sizeof(state->drawn));
        state->shadowDrawn = FALSE;
    }
    bit = 1U << (part % 32);
    if (state->drawn[part / 32] & bit) {
        // Drawn twice in one frame; a reused ID would pair the two copies.
        return NULL;
    }
    state->drawn[part / 32] |= bit;
    *snap = state->snap;
    return sSkelBank->lists[sSkelBank->count++];
}

// Same as skel_interp_begin_part, for the shadow of an object whose parts were
// drawn earlier this frame.
static Gfx* skel_interp_begin_shadow(void* key, Mtx* mtx, s32* snap) {
    SkelInterpState* state;

    skel_interp_sync_frame(mtx);
    if ((key == NULL) || (sSkelBank->count >= SKEL_SIDE_LIST_COUNT)) {
        return NULL;
    }
    state = skel_interp_find_state(key);
    if ((state == NULL) || (state->frame != sSkelFrame) || state->shadowDrawn) {
        return NULL;
    }
    state->shadowDrawn = TRUE;
    *snap = state->snap;
    return sSkelBank->lists[sSkelBank->count++];
}

static Gfx* skel_interp_open_group(Gfx* list, u32 id, s32 snap, MtxF* mf) {
    f32 det = mf->mf[0][0] * (mf->mf[1][1] * mf->mf[2][2] - mf->mf[1][2] * mf->mf[2][1]) -
              mf->mf[0][1] * (mf->mf[1][0] * mf->mf[2][2] - mf->mf[1][2] * mf->mf[2][0]) +
              mf->mf[0][2] * (mf->mf[1][0] * mf->mf[2][1] - mf->mf[1][1] * mf->mf[2][0]);

    gEXEnable(list++);
    if (snap) {
        gEXMatrixGroupDecomposedSkipAll(list++, id, G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    } else if (det > 0.0f) {
        gEXMatrixGroupDecomposedNormal(list++, id, G_EX_PUSH, G_MTX_MODELVIEW, G_EX_EDIT_NONE);
    } else {
        // Decomposition can't represent a mirrored matrix, so blend it
        // component-wise instead.
        gEXMatrixGroup(list++, id, G_EX_INTERPOLATE_SIMPLE, G_EX_PUSH, G_MTX_MODELVIEW, G_EX_COMPONENT_INTERPOLATE,
                       G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE, G_EX_COMPONENT_INTERPOLATE,
                       G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_INTERPOLATE, G_EX_ORDER_LINEAR,
                       G_EX_EDIT_NONE, G_EX_ASPECT_AUTO, G_EX_COMPONENT_SKIP, G_EX_COMPONENT_SKIP);
    }
    return list;
}

static void skel_interp_close_group(Gfx* list) {
    gEXPopMatrixGroup(list++, G_MTX_MODELVIEW);
    gSPEndDisplayList(list++);
}

static void skel_draw_part(void* key, s32* model, s32 part, Vec3f* pos, MtxF* mf, Mtx* mtx, Mtx* segMtx, void* dl) {
    s32 snap = 0;
    Gfx* list = skel_interp_begin_part(key, model, part, pos, mtx, &snap);

    if (list == NULL) {
        gSPMatrix(gMasterGfxPos++, segMtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        if (dl != NULL) {
            gSPDisplayList(gMasterGfxPos++, dl);
        }
        return;
    }

    gSPDisplayList(gMasterGfxPos++, list);
    list = skel_interp_open_group(list, 0x40000000 | (((u32) key & 0x7FFFFC) << 5) | part, snap, mf);
    gSPMatrix(list++, segMtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    if (dl != NULL) {
        gSPDisplayList(list++, dl);
    }
    skel_interp_close_group(list);
}

static void gfx_words(Gfx** gfx, u32 w0, u32 w1) {
    (*gfx)->words.w0 = w0;
    (*gfx)->words.w1 = w1;
    (*gfx)++;
}

// Queues a shadow under an actor for func_800118D4 to draw.
// Advanced lighting casts real shadows, so the flat ones are left out.
RECOMP_PATCH void func_80011860(f32 x, f32 y, f32 z, f32 rotX, f32 rotY, f32 rotZ, f32 scale) {
    u32 index = gNumShadowsToDrawThisFrame;

    if (recomp_get_advanced_lighting_enabled()) {
        sSkelShadowObject = NULL;
        return;
    }

    if (index < ACTOR_SHADOW_MAX) {
        ActorShadow* shadow = &gActorShadows[index];

        gNumShadowsToDrawThisFrame = index + 1;
        shadow->x = x;
        shadow->y = y;
        shadow->z = z;
        shadow->rotX = rotX;
        shadow->rotY = rotY;
        shadow->rotZ = rotZ;
        shadow->scale = scale;
        sShadowObjects[index] = sSkelShadowObject;
    }
    sSkelShadowObject = NULL;
}

// Draws the queued actor shadows.
RECOMP_PATCH void func_800118D4(Mtx* arg0) {
    u32 count = gNumShadowsToDrawThisFrame;
    ActorShadow* shadow = gActorShadows;
    void** object = sShadowObjects;
    MtxF sp60;

    if (count != 0) {
        gfx_words(&gMasterGfxPos, 0x06000000, 0x01000270);
        gSPFogPosition(gMasterGfxPos++, D_8007B31A, D_8007B318);
    }
    for (; count != 0; count--, shadow++, object++) {
        s32 snap = 0;
        Gfx* list;
        Gfx* gfx;

        func_80023A08(&sp60, shadow->rotX, shadow->rotY, shadow->rotZ);
        sp60.mf[3][0] = shadow->x;
        sp60.mf[3][1] = shadow->y;
        sp60.mf[3][2] = shadow->z;
        func_80023DF4(&sp60, shadow->scale, shadow->scale, shadow->scale);
        guMtxF2L(sp60.mf, arg0 + 3 + D_8007B2F8);

        list = skel_interp_begin_shadow(*object, arg0, &snap);
        if (list != NULL) {
            gSPDisplayList(gMasterGfxPos++, list);
            gfx = skel_interp_open_group(list, 0x30000000 | (((u32) *object & 0x7FFFFC) << 5), snap, &sp60);
        } else {
            gfx = gMasterGfxPos;
        }
        gSPMatrix(gfx++, D_2000000 + 3 + D_8007B2F8, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        D_8007B2F8++;
        gfx_words(&gfx, 0x0400103F, 0x01000230);
        gfx_words(&gfx, 0xB1000204, 0x00020604);
        if (list != NULL) {
            skel_interp_close_group(gfx);
        } else {
            gMasterGfxPos = gfx;
        }
    }
}

//main draw skeleton function
RECOMP_PATCH void func_8001DB38(s32 arg0, SkelObject* arg1, Unk9* arg2, Mtx* arg3) {
    u16 temp_s5;
    Unk8* temp_s4;
    u16 temp_s6;
    MtxF sp78;
    Vec3f sp6C;
    s32 i;

    D_8008C5E0 = 0;
    func_80023360(&sp78, arg1->unk_0C, arg1->unk_10, arg1->unk_14, arg1->unk_24, arg1->x, arg1->y, arg1->z);

    sp6C.x = arg1->unk_24;
    sp6C.y = arg1->unk_24;
    sp6C.z = arg1->unk_24;
    temp_s4 = arg2->unk_04;
    temp_s5 = arg1->unk_54;
    temp_s6 = arg1->unk_56;

    // arg2 is the entry for the current animation, so it can't identify the
    // object; the object itself stays at the same address while it exists.
    sSkelObject = arg1;
    for (i = 0; i < arg2->unk_00; i++) {
        if (temp_s4[i].unk_14 == 0) {
            func_8001DC78(&sp78, sp6C, arg0, i, temp_s5, temp_s6, arg2, temp_s4, arg3);
        }
    }
    sSkelObject = NULL;
    sSkelShadowObject = arg1;
}

//recursively draws skeleton limb by limb based on animation frame timer
RECOMP_PATCH void func_8001DC78(MtxF* arg0, Vec3f arg1, s32 arg4, s32 arg5, s32 arg6, s32 arg7, Unk9* arg8, Unk8* arg9, Mtx* arg10) {
    s32* temp_a1;
    s32* temp_a2_2;
    Vec3f spD0;
    s32 temp_a2;
    Unk8* temp_s2;
    Unk8* var_s0;
    MtxF* drawn;
    MtxF spA0;
    Unk6 sp7C;
    void* dl;
    s32 i;

    temp_s2 = arg9 + arg5;
    temp_a2 = temp_s2->unk_06;
    if (temp_a2 != 0) {
        temp_a2 = arg6 % temp_a2;
    } else {
        temp_a2 = 0;
    }
    func_80022B40(temp_s2, &sp7C, temp_a2);
    if (temp_s2->unk_14 == 0) {
        if (arg4 & 1) {
            sp7C.unk_08 = sp7C.unk_00 = 0;
        }
        if (arg4 & 2) {
            sp7C.unk_04 = 0;
        }
    }
    func_80023C1C(&spA0, sp7C.unk_00, sp7C.unk_04, sp7C.unk_08, sp7C.unk_10, sp7C.unk_0C, sp7C.unk_14, sp7C.unk_18, sp7C.unk_1C, sp7C.unk_20);
    func_8001E138(&spA0, temp_s2);
    arg1.x *= sp7C.unk_18;
    arg1.y *= sp7C.unk_1C;
    arg1.z *= sp7C.unk_20;
    func_80023E80(arg0, &spA0, &spA0);
    // In the original function, sp+0xD0 aliases row 3 of the matrix at
    // sp+0xA0, so spD0 must hold the joint's world translation.
    spD0.x = spA0.mf[3][0];
    spD0.y = spA0.mf[3][1];
    spD0.z = spA0.mf[3][2];
    if (temp_s2->unk_16 & 1) {
        func_8002371C(&D_8008C5E8, spD0.x, spD0.y, spD0.z, D_80086DC0.unk_0C, D_80086DC0.unk_10, D_80086DC0.unk_14);
        func_80023DF4(&D_8008C5E8, arg1.x, arg1.y, arg1.z);
        drawn = &D_8008C5E8;
    } else {
        drawn = &spA0;
    }
    guMtxF2L(drawn->mf, arg10 + 3 + D_8007B2F8);
    var_s0 = arg9;
    if (temp_s2->unk_16 & 2) {
        D_8008C5B0[D_8008C5E0].x = spD0.x;
        D_8008C5B0[D_8008C5E0].y = spD0.y;
        D_8008C5B0[D_8008C5E0].z = spD0.z;
        D_8008C5E0++;
    }
    temp_a2_2 = temp_s2->unk_18;
    temp_a1 = arg8->unk_08;
    dl = NULL;
    if (temp_a2_2 == 0) {
        i = temp_s2->unk_15;
        if ((i != -1) && (temp_a1[i] != 0)) {
            dl = (void*) temp_a1[i];
        }
    } else {
        dl = (void*) temp_a1[temp_a2_2[arg7 % temp_s2->unk_1C]];
    }
    // Spell models reach here from func_8001A620 with arg8 embedded in their
    // object, so it identifies them when func_8001DB38 isn't the caller.
    skel_draw_part((sSkelObject != NULL) ? sSkelObject : (void*) arg8, temp_a1, arg5, &spD0, drawn, arg10,
                   D_2000000 + 3 + D_8007B2F8, dl);
    D_8007B2F8++;
    for (i = 0; i < arg8->unk_00; i++, var_s0++) {
        if (arg5 + 1 == var_s0->unk_14) {
            func_8001DC78(&spA0, arg1, arg4, i, arg6, arg7, arg8, arg9, arg10);
        }
    }
}
