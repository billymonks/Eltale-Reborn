# Ray tracing: experiments to try

Ideas for more graphical fidelity in the path tracer (`lib/rt64`), not started yet.

## Volumetric fog and light shafts

Fog that the sun, moon and point lights scatter through, so windows, torches and forest canopies throw visible shafts.
Ray march the primary ray through a homogeneous medium (a density and colour per room, next to `"sun and moon"` in
`rt_lights.json`'s room settings), sampling one light per step with a shadow ray, and denoise the result at low
resolution. Things to find out: the cost per frame, how well it holds up under the denoiser and upscaler, and
whether the game's own N64 fog should drive the density or be turned off where the volumetric fog is on.

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
