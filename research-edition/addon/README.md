# Entro-Cycles Research Edition - Blender addon

Wave-optics research features for Cycles: thin-film interference,
slit diffraction, configurable MIS exponent, SVGF temporal and a
render-mode switch.

## Two operation modes

### 1. Built-in kernel (recommended)
Blender compiled from the **Entro-Cycles tree** ships the actual C++
closures (ShaderNodeWaveThinFilm, ShaderNodeWaveDiffraction) and
the scene.cycles.research_* properties. The addon detects them and
only adds UI conveniences (demo scene button). See
[../08_blender_integration.md](../08_blender_integration.md) for how
to build that Blender.

### 2. Stock Blender (OSL fallback)
On an unmodified Blender the addon provides approximate Open Shading
Language implementations of both shaders. Enable **Open Shading
Language** in Render Properties, then press *Install OSL Fallback
Shaders* to create the EntroCycles_WaveThinFilm /
EntroCycles_WaveDiffraction node groups.

> The OSL fallbacks are approximations (normal-incidence thin film,
> Fraunhofer single slit). Use the built-in kernel build for
> research-grade results.

## Install

1. Zip the entro_cycles/ folder (or download the release zip).
2. Blender: Edit > Preferences > Add-ons > Install... select the zip.
3. Enable **Entro-Cycles Research Edition**.
4. In Render Properties you get the **Research Edition** panel; the
   **Add Research Demo Scene** button builds a two-cube demo.

## Features

| Feature | Built-in kernel | OSL fallback |
|---|---|---|
| Wave Thin-Film BSDF (iridescence) | full SVM closure | approx. 3-band Airy |
| Wave Diffraction BSDF (slit) | full SVM closure | approx. Fraunhofer |
| MIS exponent | kernel MIS (power heuristic ^n) | - |
| SVGF temporal | realtime path | - |
| render mode (offline/realtime) | integrator socket | - |

## Demo scene

The addon's Add Research Demo Scene creates:
- a cube with Wave Thin Film (orange at 120 nm film),
- a cube with Wave Diffraction (white from a 1 cm slit),
- a camera aimed at them and a sun light.

Press F12 to render. With the built-in kernel, the thin-film cube
shows interference colour (60 nm ~ blue, 120 nm ~ orange-red,
200 nm ~ yellow) and the diffraction cube shows the slit response.

## License

Apache-2.0 (closures) / GPL-2.0-or-later (Blender integration glue).
