# Ray tracing: experiments to try

Ideas for more graphical fidelity in the path tracer (`lib/rt64`), not started yet.

## Volumetric fog and light shafts (first version in)

Graphics menu "Volumetric Fog" (on by default). It keeps the game's look where the air is evenly lit: air in the
sun's or moon's shadow darkens what's behind it (shafts between shadows), and the sun and lights glow in it (screen
blended); it fades out by its distance, so far terrain and the sky are untouched. Rooms with a `"fog"` in
`rt_lights.json`'s room settings use it; outdoor rooms (sun or moon on) without one get the defaults, thicker around
sunrise and sunset, and indoor rooms none. `DirectRayGen` marches the camera ray in 12 segments
that lengthen with distance, each with a sun or moon shadow ray and one shadow ray towards the strongest point light;
the result is blended with last frame's (`gVolumetric`) and blurred edge-aware in `ComposePS`. Per room: density,
glow tint, ambient, sun glow, light glow, shadows, anisotropy, height falloff, distance and how much of the game's N64
fog stays, plus keyframes on the game clock. Edited in Studio's
Rooms panel. Still to find out: the cost per frame (the F9 capture's pass timings, Direct pass), how well it holds up
under fast camera moves and the upscaler, whether 12 segments are enough outdoors, and whether rendering it at half
resolution is needed.

## God rays (first version in)

Graphics menu "God Rays" (on by default). Screen space, in `DirectRayGen` after the primary rays: each pixel gathers
32 samples along the line towards the light's vanishing point, from the sky (camera ray missed) weighted by closeness
to the sun or moon, and from textures of `rt_materials.json` class "window" in their own color (the indoor fake: the
light's direction comes from the room settings' "ray azimuth" / "ray elevation", or the sun's, turned by "window bias"
towards the way the nearest, largest windows face out). Added to the volumetric fog's glow,
so it shares its history and blur. Still to find out: raster-only draws count as sky (the camera ray misses them),
so distant scenery not in the ray traced scene may shine; windows drawn raster-only (blended, no depth write) can't be
flagged.

## Light through windows indoors, ray traced

The screen space god rays fake it; the real thing would treat each window's triangles as an area light shining into
the room along the room's ray direction, colored by the stained glass. The volumetric fog then samples them like the
sun (one shadow ray towards a point on the window), so the beams fall across the floor and through the air, and the
floor gets the colored light too.

## Bloom for emissive surfaces

A glow around bright pixels (lava, torches, magic, materials with glow in `rt_materials.json`), a mip-chain blur of
whatever is over a threshold added back before tone mapping. Cheap and independent of the path tracer; it needs a
strength setting in the graphics menu and care that it doesn't wash out the UI or text.

## Auto-generated normal maps per material

Normal maps made from each texture's luminance (a height map, Sobel filtered) when the game loads it, so stone,
wood and bark catch light at grazing angles instead of shading flat. A strength per material in
`rt_materials.json` (0 off, the default for sprites, cloth and skin) that Studio's material editor can set. Things to
find out: N64 textures are tiny (often 32x32), so the result may look noisy rather than detailed; filtering or
upscaling the height map first may be needed.
