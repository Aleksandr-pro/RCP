"""Exercise production asset loader and material parser with an Android asset stub."""
from pathlib import Path
import subprocess
import tempfile

with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    (tmp / 'android').mkdir()
    (tmp / 'android/asset_manager.h').write_text('''#pragma once
#include <string>
#include <sys/types.h>
#include <cstddef>
struct AAssetManager { std::string content; bool missing = false; bool shortRead = false; int closed = 0; };
struct AAsset { AAssetManager* manager; size_t offset = 0; };
constexpr int AASSET_MODE_STREAMING = 2;
AAsset* AAssetManager_open(AAssetManager*, const char*, int);
off_t AAsset_getLength(AAsset*);
int AAsset_read(AAsset*, void*, size_t);
void AAsset_close(AAsset*);
''')
    (tmp / 'android/log.h').write_text('''#pragma once
constexpr int ANDROID_LOG_INFO = 4, ANDROID_LOG_ERROR = 6;
int __android_log_print(int, const char*, const char*, ...);
''')
    test = tmp / 'test.cpp'
    test.write_text(r'''
#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include "android/asset_manager.h"
#include "android/log.h"
#include "MaterialConfig.h"
#include "../physics/MaterialNames.h"
int __android_log_print(int, const char*, const char*, ...) { return 0; }
AAsset* AAssetManager_open(AAssetManager* m, const char*, int) { return m->missing ? nullptr : new AAsset{m}; }
off_t AAsset_getLength(AAsset* a) { return static_cast<off_t>(a->manager->content.size()); }
int AAsset_read(AAsset* a, void* target, size_t size) {
    if (a->manager->shortRead && a->offset >= 3) return 0;
    size = std::min({size, size_t(3), a->manager->content.size() - a->offset});
    std::memcpy(target, a->manager->content.data()+a->offset, size);
    a->offset += size; return int(size);
}
void AAsset_close(AAsset* a) { ++a->manager->closed; delete a; }
int main() {
    using namespace atlas::physics;
    BodyMaterial body = BodyMaterial::TestWood;
    SurfaceMaterial surface = SurfaceMaterial::Sand;
    assert(!parseBodyMaterial("WOOD", body) && body == BodyMaterial::TestWood);
    assert(!parseSurfaceMaterial("mud", surface) && surface == SurfaceMaterial::Sand);
    assert(parseBodyMaterial("test_rubber", body) && body == BodyMaterial::TestRubber);
    assert(parseBodyMaterial("test_wood", body) && body == BodyMaterial::TestWood);
    assert(parseSurfaceMaterial("stone", surface) && surface == SurfaceMaterial::Stone);
    assert(parseSurfaceMaterial("ground", surface) && surface == SurfaceMaterial::Ground);
    assert(parseSurfaceMaterial("sand", surface) && surface == SurfaceMaterial::Sand);
    std::ifstream file("ATLAS/app/src/main/assets/scenes/physics_materials.txt");
    assert(file);
    AAssetManager asset{std::string((std::istreambuf_iterator<char>(file)), {})};
    ContactMaterial result{.91f,.37f};
    float friction = 42.0f;
    assert(atlas::loadMaterialConfig(&asset,"file",result,friction));
    assert(result.restitution == .7f && result.restSpeed == .2f && friction == .6f && asset.closed == 1);
    asset.content = " \n test_rubber\t sand\n";
    assert(atlas::loadMaterialConfig(&asset,"file",result,friction) && result.restitution == .1f && friction == 1.0f);
    auto fail = [&]() { result={.91f,.37f}; int before=asset.closed; assert(!atlas::loadMaterialConfig(&asset,"file",result,friction)); assert(result.restitution==.91f && result.restSpeed==.37f && asset.closed==before+1); };
    for (const std::string& bad : {std::string(""),std::string("  \n"),std::string("test_rubber"), std::string("test_rubber sand extra"),std::string("invalid stone"),std::string("test_rubber unknown"),std::string(257,'x'), std::string("test_rubber\0 stone",18)}) { asset.content=bad; fail(); }
    asset.content="test_rubber stone"; asset.shortRead=true; fail(); asset.shortRead=false;
    result={.91f,.37f}; asset.missing=true; int before=asset.closed;
    assert(!atlas::loadMaterialConfig(&asset,"file",result,friction) && asset.closed==before && result.restitution==.91f);
    assert(!atlas::loadMaterialConfig(nullptr,"file",result,friction));
    assert(!atlas::loadMaterialConfig(&asset,nullptr,result,friction));
    asset.missing=false; asset.content="test_rubber stone"+std::string(256-17,' ');
    assert(asset.content.size()==256 && atlas::loadMaterialConfig(&asset,"file",result,friction));
}
''')
    root = Path('ATLAS/app/src/main/cpp/scene').resolve()
    exe = tmp / 'test'
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', str(tmp), '-I', str(root), str(test), str(root/'MaterialConfig.cpp'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Material config asset and name validation passed')
