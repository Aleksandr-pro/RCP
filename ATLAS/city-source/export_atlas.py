"""Export the user's generated Blender scene as bounded, static ATLAS mesh batches.

Run after city_blender.py inside Blender. Mesh vertices are transformed into
Blender world coordinates then rotated into ATLAS's Y-up right-handed basis:
(x,y,z) -> (x,z,-y). Shaded material base colors are copied to vertex colors;
node textures, lights, transparency and Cycles shading do not transfer.
"""
import bpy
from pathlib import Path
import json
import math
import os

out = Path(os.environ['ATLAS_CITY_OUTPUT'])
models = out / 'models'
scenes = out / 'scenes'
models.mkdir(parents=True, exist_ok=True)
scenes.mkdir(parents=True, exist_ok=True)
limit = 9000  # divisible by 3; stays below the production loader's size limits
parts = []
vertices = []
triangles = 0
skipped = 0
counts = {}

def save_part():
    if not vertices:
        return
    name = f'city_{len(parts):03d}'
    path = models / f'{name}.atlasmesh'
    path.write_text(''.join(vertices), encoding='utf-8')
    assert path.stat().st_size < 1024*1024 and len(vertices) <= 30000
    parts.append(name)
    vertices.clear()

depsgraph = bpy.context.evaluated_depsgraph_get()
for obj in bpy.context.scene.objects:
    if obj.type != 'MESH' or obj.hide_render:
        continue
    evaluated = obj.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh(preserve_all_data_layers=True, depsgraph=depsgraph)
    try:
        mesh.calc_loop_triangles()
        matrix = evaluated.matrix_world
        for tri in mesh.loop_triangles:
            pts = [matrix @ mesh.vertices[i].co for i in tri.vertices]
            # The existing production mesh loader rejects degenerate triangles.
            if (pts[1] - pts[0]).cross(pts[2] - pts[0]).length_squared <= 1e-20:
                skipped += 1
                continue
            mat = mesh.materials[tri.material_index] if tri.material_index < len(mesh.materials) else None
            if mat and mat.use_nodes:
                bsdf = mat.node_tree.nodes.get('Principled BSDF')
                color = bsdf.inputs['Base Color'].default_value if bsdf else (0.5,0.5,0.5,1)
            else:
                color = mat.diffuse_color if mat else (0.5,0.5,0.5,1)
            c = tuple(max(0.0, min(1.0, float(v))) for v in color[:3])
            assert all(math.isfinite(v) for v in c)
            counts[mat.name if mat else '(none)'] = counts.get(mat.name if mat else '(none)', 0) + 1
            for p in pts:
                x,y,z = float(p.x), float(p.z), -float(p.y)
                assert all(math.isfinite(v) for v in (x,y,z))
                vertices.append(f'{x:.6f} {y:.6f} {z:.6f} {c[0]:.6f} {c[1]:.6f} {c[2]:.6f}\n')
            triangles += 1
            if len(vertices) >= limit:
                save_part()
    finally:
        evaluated.to_mesh_clear()
save_part()
assert parts and triangles > 1000
(scenes / 'city.atlasscene').write_text('ATLAS_SCENE_V2\n' + ''.join(
    f'{name} 0 -0.5 0 0.08 0.08 0.08 0\n' for name in parts), encoding='utf-8')
(out / 'city-export.json').write_text(json.dumps({'parts':parts,'triangles':triangles,'skipped_degenerate':skipped,'material_triangles':counts},ensure_ascii=False,indent=2)+'\n')
print(f'ATLAS_CITY_EXPORT: {len(parts)} batches, {triangles} triangles, {skipped} degenerate skipped', flush=True)
