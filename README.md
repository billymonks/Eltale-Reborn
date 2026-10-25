# Eltale Reborn PC

Eltale Reborn PC is a fork of [Quest 64: Recompiled](https://github.com/Rainchus/Quest64-Recomp) with a few modifications for playing the "Eltale Reborn:  Holy Magic Century" and "Quest 64: Modern Gameplay" mods. The mods come bundled with this release and attempt to improve the game in every way, resulting in a fresh RPG experience.

This repository does not include the ROM. You need your own copy of the US Quest 64 ROM to play or to build.

## Saves

Saves and settings for this build stay separate from a Quest 64: Recompiled install.

- Windows: `%LOCALAPPDATA%\EltaleReborn-PC`
- Linux: `~/.config/EltaleReborn-PC`
- macOS: `~/Library/Application Support/EltaleReborn-PC`

Save files are in `saves` under that path. Put a file named `portable.txt` next to the executable and start the game from that folder to keep saves, settings, and installed mods there instead.

## Playing

On the main menu, give it the US Quest 64 ROM. Another file format of that same ROM is converted for you. This build runs that ROM only.

Graphics, audio, controls, and mods are in the in-game menus, with a mouse, a keyboard, or a controller. It runs at the monitor's refresh rate unless you set the framerate slider in the graphics menu. The framerate does not change gameplay. Widescreen and ultrawide both work, and the HUD can stay at 16:9 on an ultrawide display. Very wide aspect ratios can show animation quirks at the edge of some cutscenes. An external framerate cap, such as the one in the NVIDIA control panel, can stutter; use the in-game slider instead.

Texture packs install the same way as mods. [RT64's texture pack notes](https://github.com/rt64/rt64/blob/main/TEXTURE-PACKS.md) describe the format.

MSI Afterburner, Wallpaper Engine, and similar overlays can keep the game from rendering. Turn them off if the picture breaks.

You need a GPU with Direct3D 12 (shader model 6), Vulkan 1.2, or Metal argument buffers tier 2. On x86-64 the CPU needs SSE4.1. A crash at startup is often an old graphics driver.

## Controls

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

## Credits

The port is forked from [Quest 64: Recompiled](https://github.com/Rainchus/Quest64-Recomp) by Rainchus, using [N64: Recompiled](https://github.com/Mr-Wiseguy/N64Recomp) and [RT64](https://github.com/rt64/rt64). Headers and a few functions come from the [Quest 64 decompilation](https://github.com/Rainchus/Quest64-Decomp).

Eltale Reborn: Holy Magic Century and Quest 64: Modern Gameplay mods are created by Billy Monks.

Also used: [RmlUi](https://github.com/mikke89/RmlUi), [lunasvg](https://github.com/sammycage/lunasvg), and [FreeType](https://freetype.org/) for the menus; [moodycamel::ConcurrentQueue](https://github.com/cameron314/concurrentqueue); [Gamepad Motion Helpers](https://github.com/JibbSmart/GamepadMotionHelpers); [Ares](https://github.com/ares-emulator/ares) for the RSP vector instruction references; and [Quest 64 Tale: Recompiled](https://github.com/boricuapab/quest64tale-recompiled) by boricuapab (GPL-3.0) for the Brian interpolation tagging, the skeleton joint-position fix, the widescreen screen-fade fix, and the Quest 64 control names.

Thanks to [ZeldaRET](https://github.com/zeldaret), [decompals](https://github.com/decompals), [Wiseguy](https://github.com/Mr-Wiseguy), and [DarioSamo](https://github.com/DarioSamo).
