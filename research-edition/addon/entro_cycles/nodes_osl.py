"""OSL fallback shaders for stock Blender.

When the running Blender does not ship the compiled Entro-Cycles
kernel (no ShaderNodeWaveThinFilm), this module registers custom
shader node groups implemented in Open Shading Language. Enable
OSL (Render Properties > Open Shading Language) to use them.

The OSL sources are embedded; the nodes are built as node groups
whose Script nodes point at files written into the addon's
temporary directory on first use.
"""

import os
import tempfile

import bpy

# ------------------------------------------------------------
# OSL sources
# ------------------------------------------------------------
OSL_THIN_FILM = r"""
/* Entro-Cycles Wave Thin-Film interference (OSL fallback).
 * Single dielectric film of thickness d (nm) between ambient (n0),
 * film (n1) and substrate (n2). 3-band Airy reflectance at normal
 * incidence approximated; for research-grade results use the
 * compiled kernel build. */
shader wave_thin_film_osl(
    float n0 = 1.0,
    float n1 = 1.5,
    float n2 = 2.0,
    float d = 120.0,
    color Color = color(1.0),
    output closure color BSDF = 0)
{
    float lams[3] = {611.36, 549.15, 464.28};  /* sRGB primaries, nm */
    float R[3];
    float r01 = (n1 - n0) / (n1 + n0);
    float r12 = (n2 - n1) / (n2 + n1);
    for (int i = 0; i < 3; ++i) {
        float delta = 2.0 * M_PI * n1 * d / lams[i];  /* cosi = 1 */
        float c = cos(2.0 * delta);
        R[i] = (r01*r01 + r12*r12 + 2.0*r01*r12*c) /
               (1.0 + r01*r01*r12*r12 + 2.0*r01*r12*c);
    }
    color albedo = color(R[0], R[1], R[2]);
    BSDF = Color * albedo * diffuse(N);
}
"""

OSL_DIFFRACTION = r"""
/* Entro-Cycles Wave Diffraction (OSL fallback).
 * Fraunhofer single-slit intensity modulation of a glossy lobe.
 * Research-grade result requires the compiled kernel; this is an
 * approximate demo for stock Blender. */
shader wave_diffraction_osl(
    float width = 0.01,
    float height = 0.01,
    float wavelength = 550.0,
    float dispersion = 0.0,
    float roughness = 0.05,
    color Color = color(1.0),
    output closure color BSDF = 0)
{
    /* Fraunhofer single-slit: I = sinc^2(pi * a * sin(theta) / lambda).
     * Approximate theta from the view direction vs normal. */
    vector V = -I;
    float cosi = abs(dot(V, N));
    float sini = sqrt(max(0.0, 1.0 - cosi*cosi));
    float x = M_PI * width * sini / (wavelength * 1e-9);
    float sinc = (abs(x) < 1e-6) ? 1.0 : sin(x) / x;
    float I = sinc * sinc;
    color tint = color(I, I, I);
    if (dispersion > 0.0) {
        float xr = M_PI * width * sini / (611.36e-9);
        float xg = M_PI * width * sini / (549.15e-9);
        float xb = M_PI * width * sini / (464.28e-9);
        float sr = (abs(xr) < 1e-6) ? 1.0 : sin(xr) / xr;
        float sg = (abs(xg) < 1e-6) ? 1.0 : sin(xg) / xg;
        float sb = (abs(xb) < 1e-6) ? 1.0 : sin(xb) / xb;
        tint = color(sr*sr, sg*sg, sb*sb);
    }
    BSDF = Color * tint * glossy(N, roughness);
}
"""


def _ensure_osl_dir():
    d = os.path.join(tempfile.gettempdir(), "entro_cycles_osl")
    os.makedirs(d, exist_ok=True)
    return d


def _write_osl_files():
    d = _ensure_osl_dir()
    tf = os.path.join(d, "wave_thin_film.osl")
    df = os.path.join(d, "wave_diffraction.osl")
    if not os.path.exists(tf):
        with open(tf, "w") as f:
            f.write(OSL_THIN_FILM)
    if not os.path.exists(df):
        with open(df, "w") as f:
            f.write(OSL_DIFFRACTION)
    return tf, df


def _make_osl_group(name, filepath):
    if name in bpy.data.node_groups:
        return bpy.data.node_groups[name]
    ng = bpy.data.node_groups.new(name, 'ShaderNodeTree')
    ng.use_osl = True
    script = ng.nodes.new('ShaderNodeScript')
    script.mode = 'EXTERNAL'
    script.filepath = filepath
    # Re-export inputs/outputs from the OSL file.
    try:
        ng.interface.new_socket("BSDF", in_out='OUTPUT', socket_type='NodeSocketShader')
    except Exception:
        pass
    return ng


class ENTRORENDER_OT_install_osl(bpy.types.Operator):
    """Install OSL fallback node groups into this blend file."""
    bl_idname = "entro_cycles.install_osl"
    bl_label = "Install OSL Fallback Shaders"

    def execute(self, context):
        tf, df = _write_osl_files()
        _make_osl_group("EntroCycles_WaveThinFilm", tf)
        _make_osl_group("EntroCycles_WaveDiffraction", df)
        self.report({'INFO'}, "OSL fallback groups created: EntroCycles_WaveThinFilm, EntroCycles_WaveDiffraction")
        return {'FINISHED'}


classes = (ENTRORENDER_OT_install_osl,)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
