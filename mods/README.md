# Mods shipped with Eltale Reborn (PC)

`.nrm` and `.rtz` files in this folder are copied next to the executable on build. The game loads that folder directly. It does not copy them into `%LOCALAPPDATA%\EltaleReborn-PC\mods`.

| File | Mod |
| --- | --- |
| `eltale_reborn.nrm` | Eltale Reborn: Holy Magic Century |
| `quest64_modern_gameplay.nrm` | Quest 64: Modern Gameplay |

Replace those files after rebuilding the mods. From this repo, if Quest-64-Good-Edition is checked out beside it:

```
scripts\stage_mods.ps1
```

Deleting a file from the mods folder next to the executable removes it from the Mods menu on the next launch. A mod installed separately, under a different filename, stays in the app mods folder.
