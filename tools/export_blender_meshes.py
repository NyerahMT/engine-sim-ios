"""Export the evaluated Engine Simulator Blender meshes as a portable OBJ.

Run with Blender, from the repository root:
    blender --background art/assets.blend --python tools/export_blender_meshes.py

Or run it with the standalone `bpy` module:
    uv run --with bpy --python 3.13 tools/export_blender_meshes.py

The runtime uses the generated asset; Blender is only required when the
authored scene intentionally changes. Object transforms are deliberately not
baked because the simulator supplies those transforms while animating parts.
"""

from pathlib import Path

import bpy


MESH_NAMES = (
    "Crankshaft",
    "ConnectingRod",
    "Piston",
    "CylinderHead",
    "Valve",
    "CrankSnoutThreads",
    "CrankSnout",
    "Logo",
)


def write_mesh(output, mesh_name, mesh, vertex_offset):
    mesh.calc_loop_triangles()
    output.write(f"o {mesh_name}\n")
    for vertex in mesh.vertices:
        output.write("v {:.9g} {:.9g} {:.9g}\n".format(*vertex.co))
    for vertex in mesh.vertices:
        output.write("vn {:.9g} {:.9g} {:.9g}\n".format(*vertex.normal))
    for triangle in mesh.loop_triangles:
        indices = [vertex_offset + index + 1 for index in triangle.vertices]
        output.write("f {}//{} {}//{} {}//{}\n".format(
            indices[0], indices[0], indices[1], indices[1], indices[2], indices[2]))
    return vertex_offset + len(mesh.vertices)


def main():
    repository_root = Path(__file__).resolve().parents[1]
    source_path = repository_root / "art" / "assets.blend"
    output_path = repository_root / "assets" / "authored_meshes.obj"

    # `blender --background art/assets.blend --python ...` loads this scene
    # before the script starts. A standalone `bpy` process (for example via
    # `uv run --with bpy`) instead starts with an empty scene, so load the
    # authored source here when the expected objects are absent. Collections
    # do not affect bpy.data.objects lookup.
    if any(mesh_name not in bpy.data.objects for mesh_name in MESH_NAMES):
        bpy.ops.wm.open_mainfile(filepath=str(source_path))
    missing = [mesh_name for mesh_name in MESH_NAMES if mesh_name not in bpy.data.objects]
    if missing:
        raise RuntimeError(f"Missing expected objects in {source_path}: {', '.join(missing)}")

    depsgraph = bpy.context.evaluated_depsgraph_get()
    vertex_offset = 0
    with output_path.open("w", encoding="utf-8", newline="\n") as output:
        output.write("# Generated from art/assets.blend; do not edit by hand.\n")
        for mesh_name in MESH_NAMES:
            source = bpy.data.objects[mesh_name]
            evaluated = source.evaluated_get(depsgraph)
            mesh = evaluated.to_mesh()
            try:
                vertex_offset = write_mesh(output, mesh_name, mesh, vertex_offset)
            finally:
                evaluated.to_mesh_clear()
    print(f"Wrote {output_path}")


main()
