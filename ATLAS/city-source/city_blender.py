import bpy
import random
import math
from mathutils import Vector

# ============================================================
# НАСТРОЙКИ
# ============================================================
SEED = 27
random.seed(SEED)
RENDER_SAMPLES = 128
RESOLUTION_X = 1600
RESOLUTION_Y = 1200

# ============================================================
# ОЧИСТКА СЦЕНЫ
# ============================================================
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
for collection in bpy.data.collections:
    if collection.users == 0:
        bpy.data.collections.remove(collection)

# ============================================================
# ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
# ============================================================
def material(name, color, roughness=0.5, metallic=0.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    return mat

def emission_material(name, color, strength):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1)
    # Названия входов отличаются в Blender 3.x и 4.x.
    emission = bsdf.inputs.get("Emission Color")
    if emission is None:
        emission = bsdf.inputs.get("Emission")
    if emission is not None:
        emission.default_value = (*color, 1)
    strength_input = bsdf.inputs.get("Emission Strength")
    if strength_input is not None:
        strength_input.default_value = strength
    return mat

def cube(name, location, scale, mat=None, bevel=0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if mat:
        obj.data.materials.append(mat)
    if bevel > 0:
        mod = obj.modifiers.new("Soft edges", "BEVEL")
        mod.width = bevel
        mod.segments = 2
        obj.modifiers.new("Weighted normals", "WEIGHTED_NORMAL")
    return obj

def cylinder(name, location, radius, depth, mat, vertices=12):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=location)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(mat)
    return obj

def point_light(name, location, color, energy, radius=0.5):
    data = bpy.data.lights.new(name, type="POINT")
    data.energy = energy
    data.color = color
    data.shadow_soft_size = radius
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    obj.location = location
    return obj

def look_at(obj, target):
    direction = Vector(target) - obj.location
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()

# ============================================================
# МАТЕРИАЛЫ
# ============================================================
asphalt = material("Asphalt", (0.075, 0.078, 0.08), 0.92)
concrete = material("Sidewalk concrete", (0.38, 0.37, 0.34), 0.88)
curb_mat = material("Curb", (0.49, 0.48, 0.44), 0.9)
road_marking = material("Road paint", (0.78, 0.76, 0.67), 0.8)
dark_metal = material("Dark metal", (0.055, 0.06, 0.065), 0.5, 0.5)
glass = material("Dark windows", (0.055, 0.09, 0.11), 0.25, 0.2)
trunk_mat = material("Tree bark", (0.16, 0.105, 0.065), 0.95)
leaf_mats = [
    material("Leaves dark", (0.075, 0.16, 0.07), 0.95),
    material("Leaves medium", (0.12, 0.22, 0.09), 0.95),
    material("Leaves light", (0.18, 0.27, 0.11), 0.95),
]
facade_mats = [
    material("Warm plaster", (0.55, 0.46, 0.37), 0.9),
    material("Grey plaster", (0.38, 0.39, 0.37), 0.92),
    material("Brick tone", (0.42, 0.27, 0.22), 0.9),
    material("Sandstone", (0.59, 0.53, 0.43), 0.9),
    material("Muted blue", (0.30, 0.36, 0.37), 0.9),
]
window_light = emission_material("Warm interior windows", (1.0, 0.59, 0.25), 1.5)
lamp_glow = emission_material("Street lamp glow", (1.0, 0.72, 0.38), 3.0)
# Лёгкая неровность асфальта через процедурную текстуру.
nodes = asphalt.node_tree.nodes
links = asphalt.node_tree.links
bsdf = nodes.get("Principled BSDF")
noise = nodes.new("ShaderNodeTexNoise")
noise.inputs["Scale"].default_value = 180
noise.inputs["Detail"].default_value = 3
noise.inputs["Roughness"].default_value = 0.75
bump = nodes.new("ShaderNodeBump")
bump.inputs["Strength"].default_value = 0.22
bump.inputs["Distance"].default_value = 0.035
links.new(noise.outputs["Fac"], bump.inputs["Height"])
links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])

# ============================================================
# ЗЕМЛЯ, ДОРОГИ И ТРОТУАРЫ
# ============================================================
cube("Ground", (0, 0, -0.18), (64, 64, 0.3), asphalt)
block_centers = [(-17, -17), (-17, 17), (17, -17), (17, 17)]
for bx, by in block_centers:
    cube("Sidewalk block", (bx, by, 0.08), (24, 24, 0.22), concrete, bevel=0.08)
    edge_x = bx - math.copysign(12, bx)
    edge_y = by - math.copysign(12, by)
    cube("Curb X", (edge_x, by, 0.16), (0.18, 24, 0.25), curb_mat)
    cube("Curb Y", (bx, edge_y, 0.16), (24, 0.18, 0.25), curb_mat)
for i in range(-5, 6):
    p = i * 4.5
    if abs(p) < 7:
        continue
    cube("Lane marking X", (p, 0, 0.012), (2.2, 0.12, 0.02), road_marking)
    cube("Lane marking Y", (0, p, 0.012), (0.12, 2.2, 0.02), road_marking)
for side in (-1, 1):
    for i in range(-3, 4):
        offset = i * 0.65
        cube("Crosswalk X", (side * 6.6, offset, 0.015), (2.3, 0.36, 0.025), road_marking)
        cube("Crosswalk Y", (offset, side * 6.6, 0.015), (0.36, 2.3, 0.025), road_marking)

# ============================================================
# ЗДАНИЯ
# ============================================================
def make_building(x, y, width, depth, floors, facade):
    floor_height = 3.1
    height = floors * floor_height
    base_z = 0.2
    cube("Building", (x, y, base_z + height / 2), (width, depth, height), facade, bevel=0.08)
    cube("Building base", (x, y, base_z + 0.35), (width + 0.12, depth + 0.12, 0.7), dark_metal, bevel=0.04)
    cube("Roof cornice", (x, y, base_z + height + 0.12), (width + 0.35, depth + 0.35, 0.24), curb_mat, bevel=0.04)
    window_width = 1.05
    window_height = 1.65
    for floor in range(floors):
        z = base_z + floor * floor_height + 1.8
        count_x = max(1, int(width / 2.2))
        for index in range(count_x):
            px = x - width / 2 + (index + 0.5) * width / count_x
            for sign in (-1, 1):
                mat = window_light if random.random() < 0.3 else glass
                cube("Window", (px, y + sign * (depth / 2 + 0.025), z), (window_width, 0.06, window_height), mat)
                cube("Window sill", (px, y + sign * (depth / 2 + 0.09), z - 0.87), (window_width + 0.18, 0.18, 0.08), curb_mat)
        count_y = max(1, int(depth / 2.2))
        for index in range(count_y):
            py = y - depth / 2 + (index + 0.5) * depth / count_y
            for sign in (-1, 1):
                mat = window_light if random.random() < 0.3 else glass
                cube("Window", (x + sign * (width / 2 + 0.025), py, z), (0.06, window_width, window_height), mat)
    if random.random() < 0.75:
        cube("Roof equipment", (x + width * 0.15, y, base_z + height + 0.55), (1.4, 1.1, 0.8), dark_metal, bevel=0.05)
for bx, by in block_centers:
    for offset in (-5.3, 5.3):
        x = bx + offset
        y = by
        make_building(x=x, y=y, width=random.uniform(8.0, 9.0), depth=random.uniform(12.0, 15.0), floors=random.randint(3, 5), facade=random.choice(facade_mats))

# ============================================================
# ДЕРЕВЬЯ И ФОНАРИ
# ============================================================
def make_tree(x, y):
    cylinder("Tree trunk", (x, y, 1.65), 0.16, 3.0, trunk_mat)
    for _ in range(7):
        dx = random.uniform(-0.8, 0.8)
        dy = random.uniform(-0.8, 0.8)
        dz = random.uniform(-0.4, 0.9)
        radius = random.uniform(0.8, 1.25)
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=radius, location=(x + dx, y + dy, 3.8 + dz))
        obj = bpy.context.object
        obj.name = "Tree foliage"
        obj.scale = (random.uniform(0.8, 1.1), random.uniform(0.8, 1.1), random.uniform(0.9, 1.3))
        obj.data.materials.append(random.choice(leaf_mats))
    cube("Tree pit", (x, y, 0.205), (1.4, 1.4, 0.03), trunk_mat)

def make_streetlight(x, y):
    cylinder("Lamp post", (x, y, 2.45), 0.075, 4.5, dark_metal)
    cylinder("Lamp base", (x, y, 0.4), 0.16, 0.45, dark_metal)
    cube("Lamp head", (x, y, 4.8), (0.55, 0.55, 0.18), dark_metal)
    cube("Lamp diffuser", (x, y, 4.67), (0.38, 0.38, 0.12), lamp_glow)
    point_light("Street light", (x, y, 4.4), (1.0, 0.68, 0.38), energy=90, radius=0.7)
for sign in (-1, 1):
    for p in (-25, -14, 14, 25):
        make_tree(p, sign * 6.3)
    for p in (-23, -12, 12, 23):
        make_tree(sign * 6.3, p)
    for p in (-20, -8, 8, 20):
        make_streetlight(p, sign * 5.7)
        make_streetlight(sign * 5.7, p)

# ============================================================
# ОСВЕЩЕНИЕ И АТМОСФЕРА
# ============================================================
world = bpy.data.worlds.new("Evening sky")
bpy.context.scene.world = world
world.use_nodes = True
background = world.node_tree.nodes.get("Background")
background.inputs["Color"].default_value = (0.25, 0.32, 0.43, 1)
background.inputs["Strength"].default_value = 0.35
sun_data = bpy.data.lights.new("Evening sun", type="SUN")
sun_data.energy = 1.5
sun_data.color = (1.0, 0.73, 0.52)
sun_data.angle = math.radians(8)
sun = bpy.data.objects.new("Evening sun", sun_data)
bpy.context.collection.objects.link(sun)
sun.rotation_euler = (math.radians(28), math.radians(-32), math.radians(-28))

# ============================================================
# КАМЕРА
# ============================================================
camera_data = bpy.data.cameras.new("City camera")
camera = bpy.data.objects.new("City camera", camera_data)
bpy.context.collection.objects.link(camera)
camera.location = (42, -48, 38)
look_at(camera, (0, 0, 4))
camera_data.type = "ORTHO"
camera_data.ortho_scale = 65
bpy.context.scene.camera = camera

# ============================================================
# РЕНДЕР
# ============================================================
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.samples = RENDER_SAMPLES
scene.cycles.use_denoising = True
scene.render.resolution_x = RESOLUTION_X
scene.render.resolution_y = RESOLUTION_Y
scene.render.resolution_percentage = 100
try:
    scene.view_settings.view_transform = "AgX"
except TypeError:
    pass
scene.render.image_settings.file_format = "PNG"
print("Городская сцена готова! Нажми F12 для рендера.")
