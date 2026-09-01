"""Property groups for the Entro-Cycles Research Edition addon.

In the built-in kernel build these live directly on scene.cycles
(research_render_mode etc.) and BlenderSync reads them. For stock
Blender we mirror them on RenderSettings so the UI always has
something to show.
"""

import bpy
from bpy.props import BoolProperty, EnumProperty, FloatProperty, StringProperty


class EntroCyclesProperties(bpy.types.PropertyGroup):
    render_mode: EnumProperty(
        name="Render Mode",
        description="OFFLINE: full path tracing. REALTIME: research fast path",
        items=(
            ('OFFLINE', 'Offline', 'Full-featured offline path tracing'),
            ('REALTIME', 'Realtime', 'Research realtime fast path'),
        ),
        default='OFFLINE',
    )
    research_features: StringProperty(
        name="Research Features",
        default="wave_diffraction wave_thin_film wavelength_sampling polarization",
        description="Space-separated feature ids (see research_features.cpp)",
    )
    mis_exponent: FloatProperty(
        name="MIS Exponent",
        default=2.0,
        min=1.0,
        max=8.0,
        description="Power-heuristic exponent (2 = default power, 1 = balance)",
    )
    svgf_temporal: BoolProperty(
        name="SVGF Temporal",
        default=False,
        description="Temporal accumulation for the realtime path",
    )


classes = (EntroCyclesProperties,)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.RenderSettings.entro_cycles = bpy.props.PointerProperty(
        type=EntroCyclesProperties)


def unregister():
    del bpy.types.RenderSettings.entro_cycles
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
