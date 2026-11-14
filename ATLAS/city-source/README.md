# City Blender scene

`city_blender.py` is the scene provided by the user, stored as runnable Blender
Python (formatting compacted; object construction, random calls, parameters,
lighting and render settings retained). It deletes the current Blender scene;
run only on a new/empty project. The seed is 27. It creates a full Cycles scene;
it is **not yet** an ATLAS runtime asset.

The `City Blender source preview` GitHub workflow runs the script in a fresh
headless Blender process, saves `city.blend`, and renders a 1600x1200 PNG using
the requested 128 Cycles samples. Ubuntu Blender lacks OpenImageDenoise, so
only the CI PNG preview is rendered without denoising; the saved `.blend`
retains the requested setting. Download the artifact from that workflow run.
`export_atlas.py` evaluates Blender geometry (including bevel modifiers), converts
Z-up Blender coordinates to Y-up ATLAS coordinates and writes up to 9000 vertices
per mesh. The resulting nine static batches contain 24500 triangles and are used
by `scenes/city.atlasscene`. Material base colors are copied to vertices. The
runtime uses simplified warm directional/ambient lighting; bright windows and
lamps do not cast light. Cycles shadows, material nodes, roughness, bump mapping,
point lights and the orthographic camera are not reproduced in the mobile renderer.
The scene is decorative and has no physics or navigation.
