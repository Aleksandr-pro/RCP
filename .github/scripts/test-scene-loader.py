"""Run the actual production loader with a stub Android asset API on the host."""
from pathlib import Path
import subprocess
import tempfile

prefix = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iterator>
#include <locale>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <vector>
#include "android/asset_manager.h"
#include "android/log.h"
int __android_log_print(int, const char*, const char*, ...) { return 0; }
AAsset* AAssetManager_open(AAssetManager* m, const char*, int) {
    return m->missing ? nullptr : new AAsset{m};
}
off_t AAsset_getLength(AAsset* a) { return static_cast<off_t>(a->manager->content.size()); }
int AAsset_read(AAsset* a, void* target, size_t size) {
    if (a->manager->shortRead) return 0;
    size = std::min({size, size_t(7), a->manager->content.size() - a->offset});
    std::memcpy(target, a->manager->content.data() + a->offset, size);
    a->offset += size;
    return static_cast<int>(size);
}
void AAsset_close(AAsset* a) { delete a; }
'''
suffix = r'''
using atlas::SceneObject;
using atlas::loadScene;
int main() {
    std::ifstream file("ATLAS/app/src/main/assets/scenes/test.atlasscene");
    assert(file);
    const std::string scene((std::istreambuf_iterator<char>(file)), {});
    AAssetManager asset{scene};
    std::vector<SceneObject> objects;
    assert(loadScene(&asset, "scene", objects) && objects.size() == 9);
    for (size_t i = 0; i < objects.size(); ++i) {
        assert(objects[i].meshName == ((i % 2 == 0) ? "pyramid" : "cube"));
        assert(objects[i].scale == 0.42f && objects[i].scaleY == 0.42f && objects[i].scaleZ == 0.42f);
    }
    assert(objects[4].x == 0.0f && objects[4].y == 0.0f);
    asset.content = "cube_test 0 0 0 1 0\n" + std::string(64, 'a') + " 0 0 0 1 0\n";
    assert(loadScene(&asset, "arbitrary", objects) && objects.size() == 2);
    assert(objects[0].meshName == "cube_test" && objects[1].meshName.size() == 64);
    std::ifstream physicsFile("ATLAS/app/src/main/assets/scenes/physics.atlasscene");
    assert(physicsFile);
    asset.content = std::string((std::istreambuf_iterator<char>(physicsFile)), {});
    assert(loadScene(&asset, "physics", objects) && objects.size() == 4);
    assert(objects[0].scale == 4.0f && objects[0].scaleY == 0.2f && objects[0].scaleZ == 2.0f);
    assert(objects[1].scaleY == 0.6f);
    asset.content = "ATLAS_SCENE_V2\ncube 0 0 0 2 0 2 0\n";
    assert(!loadScene(&asset, "bad v2", objects) && objects.empty());
    asset.content = scene;
    assert(!loadScene(nullptr, "scene", objects) && objects.empty());
    assert(!loadScene(&asset, nullptr, objects) && objects.empty());
    asset.missing = true; assert(!loadScene(&asset, "scene", objects) && objects.empty()); asset.missing = false;
    asset.shortRead = true; assert(!loadScene(&asset, "scene", objects) && objects.empty()); asset.shortRead = false;
    for (const std::string& bad : {std::string(""), std::string(" \n\t"), std::string("cube 0 0"),
            scene + "oops", scene + "../cube 0 0 0 1 0", scene + "Cube 0 0 0 1 0",
            scene + "cube-test 0 0 0 1 0", scene + std::string(65, 'a') + " 0 0 0 1 0", scene + "cube 0 0 0 0 0",
            scene + "cube 0 0 0 -1 0", scene + "cube nan 0 0 1 0", std::string(1024*1024+1, ' ')}) {
        asset.content = bad; objects.push_back(SceneObject{});
        assert(!loadScene(&asset, "bad", objects) && objects.empty());
    }
    asset.content.clear();
    for (int i = 0; i < 10000; ++i) asset.content += "cube 0 0 0 1 0\n";
    assert(loadScene(&asset, "limit", objects) && objects.size() == 10000);
    asset.content += "cube 0 0 0 1 0\n";
    assert(!loadScene(&asset, "too-many", objects) && objects.empty());
}
'''
with tempfile.TemporaryDirectory() as directory:
    test = Path(directory) / 'test.cpp'
    exe = Path(directory) / 'test'
    android = Path(directory) / 'android'
    android.mkdir()
    (android / 'asset_manager.h').write_text('''#pragma once
#include <string>
#include <sys/types.h>
#include <cstddef>
struct AAssetManager { std::string content; bool missing = false, shortRead = false; };
struct AAsset { AAssetManager* manager; size_t offset = 0; };
constexpr int AASSET_MODE_STREAMING = 2;
AAsset* AAssetManager_open(AAssetManager*, const char*, int);
off_t AAsset_getLength(AAsset*);
int AAsset_read(AAsset*, void*, size_t);
void AAsset_close(AAsset*);
''')
    (android / 'log.h').write_text('''#pragma once
constexpr int ANDROID_LOG_INFO = 4, ANDROID_LOG_ERROR = 6;
int __android_log_print(int, const char*, const char*, ...);
''')
    header = Path('ATLAS/app/src/main/cpp/scene/SceneLoader.h').resolve()
    source = Path('ATLAS/app/src/main/cpp/scene/SceneLoader.cpp').resolve()
    test.write_text(prefix + f'\n#include "{header}"\n' + suffix)
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', directory,
                    str(test), str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Production scene loader validation passed')
