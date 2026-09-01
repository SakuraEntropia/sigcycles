# ############################################################
# Entro-Cycles - Research Edition addon for Blender
# ############################################################
# Wave optics research features for Cycles:
#   - Wave Thin-Film BSDF (single dielectric film iridescence)
#   - Wave Diffraction BSDF (Fraunhofer slit / double-slit)
#   - MIS exponent knob, SVGF temporal, render mode
#
# Two operation modes:
#   1. Built-in kernel (recommended): Blender compiled from the
#      Entro-Cycles tree ships ShaderNodeWaveThinFilm /
#      ShaderNodeWaveDiffraction and scene.cycles.research_*
#      properties. The addon detects them and only adds UI
#      conveniences (templates, quick setup).
#   2. Stock Blender: registers OSL fallback nodes so the
#      research shaders are usable with OSL rendering.
#
# Install: Edit > Preferences > Add-ons > Install... > select
# this directory's zip (or the entro_cycles/ folder) > Enable.

bl_info = {
    "name": "Entro-Cycles Research Edition",
    "author": "Entro-Cycles",
    "version": (1, 0, 0),
    "blender": (5, 3, 0),
    "location": "Render Properties > Research Edition",
    "description": "Wave optics research features for Cycles "
                   "(thin-film interference, diffraction, MIS, SVGF)",
    "category": "Render",
}

import bpy

from . import properties, ui, nodes_osl


def _has_builtin_kernel():
    """True when the running Blender was built from the Entro-Cycles tree."""
    cscene = getattr(bpy.context.scene, "cycles", None)
    if cscene is None:
        return False
    return hasattr(cscene, "research_render_mode")


def register():
    properties.register()
    ui.register()
    nodes_osl.register()


def unregister():
    nodes_osl.unregister()
    ui.unregister()
    properties.unregister()


if __name__ == "__main__":
    register()
