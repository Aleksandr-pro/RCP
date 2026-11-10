# City Blender scene

`city_blender.py` is the scene provided by the user, stored as runnable Blender
Python (formatting compacted; object construction, random calls, parameters,
lighting and render settings retained). It deletes the current Blender scene;
run only on a new/empty project. The seed is 27. It creates a full Cycles scene;
it is **not yet** an ATLAS runtime asset.

The `City Blender source preview` GitHub workflow runs the script in a fresh
headless Blender process, saves `city.blend`, and renders a 1600x1200 PNG using
the requested 128 Cycles samples. Download the artifact from that workflow run.
Blender/Cycles settings, procedural nodes, lights and camera do not automatically
work in the current mobile Vulkan renderer; bringing the scene into the engine
requires an explicit export and mobile rendering implementation.
