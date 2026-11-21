# astra_bridge.py - Astra Productions Blender bridge (Blender 2.90 - 4.x).
#
# Install: Edit > Preferences > Add-ons > Install... > pick this file, then tick
# "Astra Productions Bridge". The Astra tab appears in the 3D View sidebar (N panel).
#
# "Send selection to Astra" exports the selected objects as OBJ into the engine's bridge inbox.
# The editor imports whatever is waiting there (Tools > Import from Blender, or automatically on
# project open), so there is no socket, no port forwarding and nothing to keep running: Blender
# can stay on another machine and write to a shared folder.
#
# The inbox location is printed in the panel; it lives under the per-user engine data directory:
#   Windows  %LOCALAPPDATA%\Astra Productions\engine\bridge\inbox
#   Linux    $XDG_DATA_HOME/astra-productions/engine/bridge/inbox
#   macOS    ~/Library/Application Support/Astra Productions/engine/bridge/inbox

bl_info = {
    "name": "Astra Productions Bridge",
    "author": "Astra Productions",
    "version": (1, 0, 0),
    "blender": (2, 90, 0),
    "location": "View3D > Sidebar > Astra",
    "description": "Send meshes to the Astra Productions engine and report status back",
    "category": "Import-Export",
}

import json
import os
import sys
import time

import bpy

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

def _data_root():
    """Mirror astra::paths::DataRoot() so both sides agree on the inbox."""
    override = os.environ.get("ASTRA_DATA_DIR")
    if override:
        return override
    if sys.platform.startswith("win"):
        base = os.environ.get("LOCALAPPDATA") or os.path.expanduser("~")
        return os.path.join(base, "Astra Productions")
    if sys.platform == "darwin":
        return os.path.join(os.path.expanduser("~"), "Library", "Application Support",
                            "Astra Productions")
    xdg = os.environ.get("XDG_DATA_HOME") or os.path.join(os.path.expanduser("~"), ".local",
                                                          "share")
    return os.path.join(xdg, "astra-productions")


def bridge_root():
    root = os.path.join(_data_root(), "engine", "bridge")
    os.makedirs(root, exist_ok=True)
    return root


def inbox_dir():
    path = os.path.join(bridge_root(), "inbox")
    os.makedirs(path, exist_ok=True)
    return path


def status_path():
    return os.path.join(bridge_root(), "status.json")


def write_status(extra=None):
    status = {
        "blender": bpy.app.version_string,
        "scene": bpy.context.scene.name if bpy.context.scene else "",
        "timestamp": time.time(),
        "bridge": bl_info["version"][0],
    }
    if extra:
        status.update(extra)
    with open(status_path(), "w", encoding="utf-8") as handle:
        json.dump(status, handle, indent=2)


# ---------------------------------------------------------------------------
# Export
# ---------------------------------------------------------------------------

def _selected_objects():
    return [obj for obj in bpy.context.scene.objects if obj.type == "MESH" and obj.select_get()]


def _export_obj(objects, path):
    """Exports `objects` as a single OBJ. Handles the 3.3+ and legacy exporter signatures."""
    previous = [obj.select_get() for obj in bpy.context.scene.objects]
    try:
        for obj in bpy.context.scene.objects:
            obj.select_set(obj in objects)
        if hasattr(bpy.ops.wm, "obj_export"):          # Blender 3.3+
            bpy.ops.wm.obj_export(filepath=path, export_selected_objects=True,
                                  export_materials=False, export_uv=True, export_normals=True,
                                  export_triangulated_mesh=False, axis_forward="-Z",
                                  axis_up="Y", apply_modifiers=True)
        else:                                           # Blender <= 3.2
            if "-Z" == "Y":
                pass
            bpy.ops.export_scene.obj(filepath=path, use_selection=True, use_materials=False,
                                     use_uvs=True, use_normals=True, axis_forward="-Z",
                                     axis_up="Y", use_mesh_modifiers=True)
    finally:
        for obj, state in zip(bpy.context.scene.objects, previous):
            obj.select_set(state)
    return os.path.exists(path)


def send_selection(material_hint="Lit", name_hint=""):
    objects = _selected_objects()
    if not objects:
        return False, "Select at least one mesh object first."
    stem = name_hint.strip() or objects[0].name
    stem = "".join(c if c not in '/\\:' else "_" for c in stem)
    obj_path = os.path.join(inbox_dir(), stem + ".obj")
    if not _export_obj(objects, obj_path):
        return False, "Blender could not write " + obj_path

    triangles = 0
    for obj in objects:
        mesh = obj.data
        mesh.calc_loop_triangles()
        triangles += len(mesh.loop_triangles)

    meta = {
        "name": stem,
        "material": material_hint,
        "source": "blender",
        "blender": bpy.app.version_string,
        "objects": [obj.name for obj in objects],
        "triangles": triangles,
        "placeholder": False,
    }
    with open(os.path.join(inbox_dir(), stem + ".json"), "w", encoding="utf-8") as handle:
        json.dump(meta, handle, indent=2)

    write_status({"lastExport": stem, "lastTriangles": triangles})
    return True, "Sent %s (%d tris) to the Astra inbox." % (stem, triangles)


def send_test_mesh():
    """Drops a small cube so the editor side can be verified without a scene."""
    bpy.ops.mesh.primitive_cube_add(size=1.0)
    obj = bpy.context.active_object
    for other in bpy.context.scene.objects:
        other.select_set(other == obj)
    return send_selection("Hologram", "astra_bridge_test")


# ---------------------------------------------------------------------------
# UI
# ---------------------------------------------------------------------------

class ASTRA_OT_send_selection(bpy.types.Operator):
    bl_idname = "astra.send_selection"
    bl_label = "Send selection to Astra"
    bl_description = "Export the selected meshes into the engine's bridge inbox"
    bl_options = {"REGISTER"}

    def execute(self, context):
        ok, message = send_selection(context.scene.astra_material, context.scene.astra_name)
        self.report({"INFO"} if ok else {"ERROR"}, message)
        return {"FINISHED"} if ok else {"CANCELLED"}


class ASTRA_OT_send_test(bpy.types.Operator):
    bl_idname = "astra.send_test"
    bl_label = "Send test cube"
    bl_description = "Create a cube and send it, to prove the bridge is wired up"

    def execute(self, context):
        ok, message = send_test_mesh()
        self.report({"INFO"} if ok else {"ERROR"}, message)
        return {"FINISHED"} if ok else {"CANCELLED"}


class ASTRA_OT_ping(bpy.types.Operator):
    bl_idname = "astra.ping"
    bl_label = "Write status"
    bl_description = "Publish Blender's version and scene name for the engine's panel"

    def execute(self, context):
        write_status()
        self.report({"INFO"}, "Astra status written.")
        return {"FINISHED"}


class ASTRA_PT_panel(bpy.types.Panel):
    bl_label = "Astra Productions"
    bl_idname = "ASTRA_PT_panel"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Astra"

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        layout.label(text="Bridge folder:")
        layout.label(text=inbox_dir(), icon="FILE_FOLDER")
        selected = len(_selected_objects())
        layout.label(text="%d mesh object(s) selected" % selected, icon="MESH_DATA")

        layout.prop(scene, "astra_name", text="Name")
        box = layout.box()
        box.label(text="Imported material")
        box.prop(scene, "astra_material", text="")
        layout.operator("astra.send_selection", icon="EXPORT")
        row = layout.row(align=True)
        row.operator("astra.ping", icon="FILE_REFRESH")
        row.operator("astra.send_test", icon="MESH_CUBE")


CLASSES = (ASTRA_OT_send_selection, ASTRA_OT_send_test, ASTRA_OT_ping, ASTRA_PT_panel)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)
    bpy.types.Scene.astra_name = bpy.props.StringProperty(
        name="Name", description="Actor name in Astra (defaults to the first object)", default="")
    bpy.types.Scene.astra_material = bpy.props.EnumProperty(
        name="Material",
        description="Material preset applied when the mesh lands in the level",
        items=[
            ("Lit", "Lit", "Standard lit surface"),
            ("Checker", "Checker", "Debug checkerboard"),
            ("Hologram", "Hologram", "Emissive holographic shell"),
            ("Emissive", "Emissive", "Self-lit surface"),
        ],
        default="Lit")
    write_status({"registered": True})


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
    for prop in ("astra_name", "astra_material"):
        if hasattr(bpy.types.Scene, prop):
            delattr(bpy.types.Scene, prop)


if __name__ == "__main__":
    # Run from Blender's text editor or the command line:
    #   blender --background --python astra_bridge.py
    # registers the addon immediately, and "-- --send" exports the current selection once.
    register()
    argv = sys.argv
    if "--send" in argv:
        ok, message = send_selection()
        print("[astra] " + message)
