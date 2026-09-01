"""UI panels for the Entro-Cycles Research Edition addon."""

import bpy


class ENTRORENDER_PT_research(bpy.types.Panel):
    bl_label = "Research Edition"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = 'render'

    @classmethod
    def poll(cls, context):
        return context.scene.render.engine == 'CYCLES'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        cscene = scene.cycles

        if hasattr(cscene, "research_render_mode"):
            # Built-in kernel build: mirror the real engine properties.
            box = layout.box()
            box.label(text="Built-in Research Edition kernel", icon='CHECKMARK')
            col = box.column(align=True)
            col.prop(cscene, "research_render_mode")
            col.prop(cscene, "research_features")
            col.prop(cscene, "research_mis_exponent")
            col.prop(cscene, "research_svgf_temporal")
        else:
            # Stock Blender: expose the addon's mirrored properties.
            props = scene.render.entro_cycles
            col = layout.column(align=True)
            col.prop(props, "render_mode")
            col.prop(props, "research_features")
            col.prop(props, "mis_exponent")
            col.prop(props, "svgf_temporal")
            box = layout.box()
            box.label(text="Stock Blender: OSL fallback shaders", icon='INFO')
            box.label(text="Enable OSL (Render > Open Shading Language) and add")
            box.label(text="the Wave Thin Film / Wave Diffraction shader nodes.")

        layout.operator("entro_cycles.sample_scene", icon='MESH_CUBE')


class ENTRORENDER_OT_sample_scene(bpy.types.Operator):
    """Build a demo scene: thin-film cube + diffraction cube."""
    bl_idname = "entro_cycles.sample_scene"
    bl_label = "Add Research Demo Scene"
    bl_description = "Create a small scene demonstrating both research BSDFs"

    def execute(self, context):
        from mathutils import Vector

        scene = context.scene
        # Remove existing demo objects.
        for obj in list(scene.collection.objects):
            if obj.name.startswith("EntroDemo"):
                scene.collection.objects.unlink(obj)

        def make_cube(loc, mat_name, is_film=True):
            # Build the cube mesh data directly (background-safe).
            import mathutils
            mesh = bpy.data.meshes.new("EntroDemoMesh_" + mat_name)
            s = 1.0
            verts = [(-s, -s, -s), (s, -s, -s), (s, s, -s), (-s, s, -s),
                     (-s, -s, s), (s, -s, s), (s, s, s), (-s, s, s)]
            faces = [(0, 1, 2, 3), (4, 7, 6, 5), (0, 4, 5, 1),
                     (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)]
            mesh.from_pydata(verts, [], faces)
            mesh.update()
            obj = bpy.data.objects.new("EntroDemo_" + mat_name, mesh)
            obj.location = loc
            scene.collection.objects.link(obj)

            mat = bpy.data.materials.new(name="EntroDemo_" + mat_name)
            mat.use_nodes = True
            ntree = mat.node_tree
            ntree.nodes.clear()
            out = ntree.nodes.new('ShaderNodeOutputMaterial')
            if hasattr(bpy.types, 'ShaderNodeWaveThinFilm'):
                node = ntree.nodes.new('ShaderNodeWaveThinFilm')
                if is_film:
                    node.inputs['Ambient Index'].default_value = 1.0
                    node.inputs['Film Index'].default_value = 1.5
                    node.inputs['Substrate Index'].default_value = 2.0
                    node.inputs['Film Thickness (nm)'].default_value = 120.0
                else:
                    node.inputs['Width'].default_value = 0.01
                    node.inputs['Height'].default_value = 0.01
                    node.inputs['Wavelength'].default_value = 550.0
            else:
                node = ntree.nodes.new('ShaderNodeGroup')
                node.node_tree = bpy.data.node_groups.get('EntroCycles_WaveThinFilm') or                                 bpy.data.node_groups.get('EntroCycles_WaveDiffraction')
            ntree.links.new(node.outputs['BSDF'], out.inputs['Surface'])
            obj.data.materials.append(mat)
            return obj

        make_cube((-3.0, 0, 0), "ThinFilm", is_film=True)
        make_cube((3.0, 0, 0), "Diffraction", is_film=False)

        cam_data = bpy.data.cameras.new("EntroDemoCam")
        cam = bpy.data.objects.new("EntroDemoCam", cam_data)
        cam.location = (0, -7, 0.5)
        dir_vec = Vector((0, 0, 0)) - cam.location
        cam.rotation_euler = dir_vec.to_track_quat('-Z', 'Y').to_euler()
        scene.collection.objects.link(cam)
        scene.camera = cam

        sun_data = bpy.data.lights.new("EntroDemoSun", type='SUN')
        sun = bpy.data.objects.new("EntroDemoSun", sun_data)
        sun.location = (0, 0, 8)
        scene.collection.objects.link(sun)
        self.report({'INFO'}, "Demo scene created - press F12 to render")
        return {'FINISHED'}


classes = (
    ENTRORENDER_PT_research,
    ENTRORENDER_OT_sample_scene,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
