# Controls: Modern and Classic

The Controls page offers two controls styles. **Classic** is Eltale Reborn's controls as released: the original
layouts (`classic_n64_controller_mappings`, `classic_n64_keyboard_mappings`) and the original camera. **Modern** is a
modern-controller setup: a layout by position (Xbox-style by default), actions with no N64 button, a free camera on
the right stick, and the camera fixes in `patches/camera.c`.

A new install starts in `recomp::default_controls_style` (`include/recomp_input.h`), Classic. Set it to
`ControlsStyle::Modern` to start new players on the modern controls. A `controls.json` from a release always loads as
Classic with its own bindings, whatever the default.

## Where things live

| What | Where |
|---|---|
| The inputs, their names and order on the page, `GameAction`, `ControlsStyle` | `include/recomp_input.h` |
| Each style's default bindings | `src/game/input.cpp` (`default_n64_*` are Modern's, `classic_n64_*` Classic's) |
| The bindings, one set per style; the style; polling; the actions; the camera stick | `src/game/controls.cpp` |
| Saving and loading `controls.json` | `src/game/config.cpp` |
| Exports the patches call; the controller-read hook | `src/game/recomp_api.cpp`, `us.rev0.toml`, `patches/syms.ld` |
| The camera modes, both styles | `patches/camera.c` |
| The Controls page: rows, style tabs, the pad picture | `assets/config_menu/controls.rml`, `src/ui/ui_config.cpp`, `assets/recomp.rcss` |
| Camera speed and invert (General page) | `assets/config_menu/general.rml`, `src/ui/ui_config.cpp` |

## Bindings and styles

`recomp::is_input_used_by_style` decides which inputs a style uses. Classic binds the N64 buttons, L and Z included,
and none of the actions. Modern binds the actions, not N64 L and Z: the game reads Z as a second Confirm (and a quick
pass of the turn) and L as a camera angle, so Modern covers what they were for with its own actions. That one
function drives polling (`get_n64_input`), the action reads, the page's rows (the `input_row_visible` transform),
what each style saves, and the pad picture.

Each style keeps its own bindings, so switching never loses either. The PC menu's own inputs (`TOGGLE_MENU`,
`ACCEPT_MENU`, `APPLY_MENU`) are one set for both (`is_input_shared_by_styles`), so switching never moves the menu's
buttons.

`controls.json` keeps Classic's bindings under the keys it has always used, `keyboard` and `controller`, so a release
build still reads its own layout from it. Modern's are under `keyboard_modern` and `controller_modern`, and `style`
holds the current style. A file without `style` is a release's: Classic, with its bindings, and Modern's defaults.

In Modern, Previous / Next send N64 L, Z and R where the game reads them as paging: `recomp_menu_page_buttons`
(`recomp_api.cpp`) runs inside the game's controller read (the `[[patches.hook]]` in `func_800314C0`) and adds them in
the item menu and the Start menu only. In Classic it changes nothing.

## Adding an action

1. `include/recomp_input.h`: a `DEFINE_INPUT(NAME, 0, "What it does")` in `DEFINE_ACTION_INPUTS()` (the page lists
   rows in this order), a stable id at the end of `GameAction` (ids are what the patches pass, so append only), a
   field in `DefaultN64Mappings`, and its case in `get_default_mapping_for_input`.
2. `src/game/input.cpp`: its default binding in `default_n64_controller_mappings` and `default_n64_keyboard_mappings`
   (the Classic tables leave it out).
3. `src/game/controls.cpp`: its case in `is_input_used_by_style` (with the other Modern-only actions) and in
   `action_to_input`.
4. `patches/input.h`: the same id in `RecompAction`; a patch reads it with `recomp_get_action_input(id)`, 0..1 (a
   trigger's travel, 0 or 1 for a button), 0 while the PC menu has the input or in Classic.

Saving, loading, the page's row and the pad picture pick it up from there.

## The camera (`patches/camera.c`)

`CameraMode_Field` (`func_80013008`) and `CameraMode_Battle` (`func_80013A20`) are patched whole. They are identical,
instruction for instruction, to the Japanese release's, and the patch follows the matched decompilation of those. A
whole-function patch is what survives a mod hooking these functions (a hook regenerates the function from the ROM,
dropping `[[patches.hook]]` text); mods can still hook them.

Both start with `style_sync`, which reads `recomp_get_controls_style()` and, on a change, clears the modern camera's
state or (switching to Classic) puts back the area's yaw gains and the original battle framing.

- **Classic** runs `classic_field` / `classic_battle`: the original code, with the original field camera's three steps
  in shared helpers (`field_follow`: the leash, the swing aside up close and behind; `field_height`: the floor poll and
  the eye's height spring; `field_look`: the look-at, pitch and yaw).
- **Modern** adds, around the same helpers: the free camera (`sFree`, `sFreeYaw`, the tilt `sTilt`), the snap and
  hold (`read_target`, `sSnapFrames`), the zoom (`zoom_apply`), the battle aim that turns smoothly (`sAimBase`), the
  lock on's lead limit, Brian's action cameras swinging behind him (`act_begin`, `act_swing`), and the escape turn.
  `view_pre` / `view_post` apply the zoom and the tilt for one update and put the room's preset back.

The tuning constants are at the top of the file. Game addresses (Quest 64 US 1.0) are `#define`s next to them; each
was paired instruction by instruction with the Japanese release.

Exports the camera uses (`patches/input.h`): `recomp_get_camera_inputs` (the camera stick, inverted per the General
page), `recomp_get_camera_speed` (the General page's speeds), `recomp_get_action_input`, `recomp_get_controls_style`.

## The pad picture

`ui_config.cpp`'s `PadView` is a reverse lookup of the controller's bindings: for each physical control, the actions
bound to it, and whether it's bound to the highlighted row. It's reached as `inputs.pad` (a child of the page's
`inputs` variable), so every change of a binding or of the highlighted row refreshes it.

The drawing is `assets/icons/VizMap/ModernPad.svg`, and each control's highlight is its own SVG on the same canvas
(`ModernPad_<control>.svg`), laid over the drawing while the control is lit, so it lines up at any size.
`scripts/make_pad.py` writes all of them, and the layer markup for `controls.rml`, from one set of coordinates: edit
the coordinates there rather than the SVGs.
