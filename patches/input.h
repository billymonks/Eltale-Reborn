#ifndef __INPUT_H__
#define __INPUT_H__

#include "patch_helpers.h"

typedef enum {
    RECOMP_CAMERA_NORMAL,
    RECOMP_CAMERA_DUALANALOG,
} RecompCameraMode;

extern RecompCameraMode recomp_camera_mode;

DECLARE_FUNC(void, recomp_get_gyro_deltas, float* x, float* y);
DECLARE_FUNC(void, recomp_get_mouse_deltas, float* x, float* y);
DECLARE_FUNC(s32, recomp_get_targeting_mode);
DECLARE_FUNC(void, recomp_get_inverted_axes, s32* x, s32* y);
DECLARE_FUNC(s32, recomp_get_invert_y_axis_mode);
DECLARE_FUNC(s32, recomp_get_radio_comm_box_mode);
DECLARE_FUNC(void, recomp_get_analog_inverted_axes, s32* x, s32* y);
DECLARE_FUNC(void, recomp_get_camera_inputs, float* x, float* y);
// The camera stick's speed settings, as multipliers of the tuned speed (1 = 100%): turning, tilting.
DECLARE_FUNC(void, recomp_get_camera_speed, float* turn, float* tilt);
// The controls style (the Controls page): 0 Classic controls (the original camera), 1 Modern.
DECLARE_FUNC(s32, recomp_get_controls_style);
DECLARE_FUNC(void, recomp_set_right_analog_suppressed, s32 suppressed);

// Actions with no N64 button (the controls page's camera and menu rows), by the stable ids of recomp::GameAction.
typedef enum {
    RECOMP_ACTION_CAMERA_TARGET = 0, // snap the camera behind Brian, lock on in battle
    RECOMP_ACTION_CAMERA_ZOOM = 1,   // zoom the field camera out
    RECOMP_ACTION_PAGE_PREV = 2,     // previous: menu page, item (the game reads it as N64 Z / L there)
    RECOMP_ACTION_PAGE_NEXT = 3,     // next: menu page, item (N64 R / Z there)
} RecompAction;
// 0..1: 0 or 1 for a button or key, a trigger's travel. 0 while the PC menu has the input.
DECLARE_FUNC(float, recomp_get_action_input, u32 action);

#endif
