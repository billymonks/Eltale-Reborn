# Eltale Reborn PC

Eltale Reborn: Holy Magic Century is a modified PC Port of Quest 64 that overhauls the game into the RPG it should have been: a deeper story built on the original premise (Brian, his missing father Lord Bartholomy, the Eletale Book, Celtland), reactive NPCs, new events and side quests, and reworked progression with far less grind.  

This repository does not include the ROM. You need your own copy of the US Quest 64 ROM to play or to build.

A [Dreamcast port](https://github.com/billymonks/Eltale-Reborn-DC-Port) is also available. A patch with these changes for the N64 version can be found [here](https://github.com/dpranker/Eltale-Reborn-N64-Port).

## Saves

Saves and settings for this build stay separate from a Quest 64: Recompiled install.

- Windows: `%LOCALAPPDATA%\EltaleReborn-PC`
- Linux: `~/.config/EltaleReborn-PC`
- macOS: `~/Library/Application Support/EltaleReborn-PC`

Save files are in `saves` under that path. Put a file named `portable.txt` next to the executable and start the game from that folder to keep saves, settings, and installed mods there instead.

## Playing

On the main menu, give it the US Quest 64 ROM. Another file format of that same ROM is converted for you. This build runs that ROM only.

Graphics, audio, controls, and mods are in the in-game menus, with a mouse, a keyboard, or a controller. It runs at the monitor's refresh rate unless you set the framerate slider in the graphics menu. The framerate does not change gameplay. Widescreen and ultrawide both work, and the HUD can stay at 16:9 on an ultrawide display. Very wide aspect ratios can show animation quirks at the edge of some cutscenes. An external framerate cap, such as the one in the NVIDIA control panel, can stutter; use the in-game slider instead.

Path traced lighting (optional, needs a GPU with hardware ray tracing) adds bounced light, ray traced shadows and reflections, and glowing light sources. The Light Samples, Denoiser and Light Accumulation settings trade performance for image quality, and AMD FSR 3 upscaling (FSR 1 as a fallback) boosts performance.

## Controls

The controls menu has two styles, Classic and Modern, and each keeps its own bindings. Classic is the default and plays as before.

### Classic

Reset to defaults in the controls menu restores a Dreamcast pad: A on the bottom, B on the right, Y on the top, X on the left. Adapters that report the triggers as shoulder buttons get the same bindings.

| Button        | Action                                      |
| ------------- | ------------------------------------------- |
| Stick         | Walk, and move cursors                      |
| A             | Talk, confirm, attack                       |
| B             | Cancel, close menus                         |
| X             | Sprint                                      |
| Y             | Item menu                                   |
| D-pad         | Magic                                       |
| Left trigger  | Scroll the item list left                   |
| Right trigger | Scroll the item list right                  |
| Start         | Open menu                                   |

Keyboard defaults: WASD to walk, the arrow keys for the spirits, Space for A, Left Shift for B, Q for the right trigger, E to scroll the item list left, R for the item menu, Enter for Start. Sprint has no keyboard key.

### Modern

Modern adds a free camera on the right stick, a focus camera that swings behind Brian (and locks on in battle), and a zoom out for the field camera. The battle camera turns smoothly, and the camera for Brian's attacks and spells shows them from behind him. Fixed camera rooms and battles keep the original camera.

| Button        | Action                                      |
| ------------- | ------------------------------------------- |
| Left stick    | Walk, and move cursors                      |
| Right stick   | Camera                                      |
| A             | Talk, confirm, attack                       |
| B             | Cancel, close menus                         |
| X             | Item menu                                   |
| Y             | Sprint                                      |
| D-pad         | Magic                                       |
| Left trigger  | Focus camera                                |
| LB            | Zoom out, previous page or item             |
| RB            | Next page or item                           |
| Start         | Open menu                                   |
| Back          | PC menu                                     |

Keyboard defaults: WASD to walk, IJKL for the camera, the arrow keys for the spirits, Space for A, Left Shift for B, R for the item menu, Enter for Start, Z for the focus camera, C to zoom out, and Q and E for the previous and next page or item. Sprint has no keyboard key.

The camera speed and invert options are in the General menu.

## Credits

Eltale Reborn is an independent, noncommercial fan modification. Not affiliated with or endorsed by Imagineer, SEGA, Nintendo, THQ, or Konami.

Original game © 1998 Imagineer Co., Ltd.



The port is forked from [Quest 64: Recompiled](https://github.com/Rainchus/Quest64-Recomp) by Rainchus, using [N64: Recompiled](https://github.com/Mr-Wiseguy/N64Recomp) and [RT64](https://github.com/rt64/rt64). Headers and a few functions come from the [Quest 64 decompilation](https://github.com/Rainchus/Quest64-Decomp).

Eltale Reborn: Holy Magic Century and Quest 64: Modern Gameplay mods are created by Billy Monks.

The Modern controls and camera are by [CanadaX21](https://github.com/CanadaX21), creator of the Eltale Monsters English translation.

Also used: [RmlUi](https://github.com/mikke89/RmlUi), [lunasvg](https://github.com/sammycage/lunasvg), and [FreeType](https://freetype.org/) for the menus; [AMD FidelityFX SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK) (FSR 3 and FSR 1) for upscaling; [moodycamel::ConcurrentQueue](https://github.com/cameron314/concurrentqueue); [Gamepad Motion Helpers](https://github.com/JibbSmart/GamepadMotionHelpers); [Ares](https://github.com/ares-emulator/ares) for the RSP vector instruction references; and [Quest 64 Tale: Recompiled](https://github.com/boricuapab/quest64tale-recompiled) by boricuapab (GPL-3.0) inspired some of the fixes.

Thanks to [ZeldaRET](https://github.com/zeldaret), [decompals](https://github.com/decompals), [Wiseguy](https://github.com/Mr-Wiseguy), and [DarioSamo](https://github.com/DarioSamo).
