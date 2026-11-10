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
using atlas::Vertex;
using atlas::loadMesh;
void checkNormals(const std::vector<Vertex>& vertices) {
    for (size_t i = 0; i < vertices.size(); i += 3) {
        const Vertex& a = vertices[i];
        float length = 0.0f, outward = 0.0f;
        for (int k = 0; k < 3; ++k) {
            length += a.normal[k] * a.normal[k];
            outward += a.normal[k] * (a.position[k] + vertices[i+1].position[k] + vertices[i+2].position[k]) / 3.0f;
            assert(a.normal[k] == vertices[i+1].normal[k] && a.normal[k] == vertices[i+2].normal[k]);
        }
        assert(std::abs(length - 1.0f) < 1e-5f);
        assert(outward > 0.0f); // Origin lies inside both models.
        const float ax = std::abs(a.normal[0]), ay = std::abs(a.normal[1]), az = std::abs(a.normal[2]);
        for (size_t j = 0; j < 3; ++j) {
            const Vertex& v = vertices[i+j];
            const float expectedU = (ax >= ay && ax >= az) ? v.position[2] + 0.5f : v.position[0] + 0.5f;
            const float expectedV = (ax < ay && ay >= az) ? v.position[2] + 0.5f : v.position[1] + 0.5f;
            assert(std::abs(v.uv[0] - expectedU) < 1e-6f && std::abs(v.uv[1] - expectedV) < 1e-6f);
        }
    }
}
int main() {
    std::ifstream file("ATLAS/app/src/main/assets/models/pyramid.atlasmesh");
    assert(file);
    const std::string model((std::istreambuf_iterator<char>(file)), {});
    AAssetManager asset{model};
    std::vector<Vertex> vertices;
    assert(loadMesh(&asset, "pyramid", vertices) && vertices.size() == 18);
    assert(vertices[0].position[1] == 0.5f && vertices[0].color[1] == 0.80f);
    checkNormals(vertices);
    std::ifstream stoneFile("ATLAS/app/src/main/assets/models/stone.atlasmesh");
    assert(stoneFile);
    asset.content = std::string((std::istreambuf_iterator<char>(stoneFile)), {});
    assert(loadMesh(&asset, "stone", vertices) && vertices.size() == 78);
    checkNormals(vertices);
    float minY=1.0f, maxY=-1.0f;
    for (auto &v : vertices) { minY=std::min(minY,v.position[1]); maxY=std::max(maxY,v.position[1]); }
    assert(minY == -0.5f && maxY == 0.5f);
    std::ifstream cubeFile("ATLAS/app/src/main/assets/models/cube.atlasmesh");
    assert(cubeFile);
    asset.content = std::string((std::istreambuf_iterator<char>(cubeFile)), {});
    assert(loadMesh(&asset, "cube", vertices) && vertices.size() == 36);
    checkNormals(vertices);
    asset.content = model;
    assert(!loadMesh(nullptr, "pyramid", vertices) && vertices.empty());
    assert(!loadMesh(&asset, nullptr, vertices));
    asset.missing = true; assert(!loadMesh(&asset, "pyramid", vertices)); asset.missing = false;
    asset.shortRead = true; assert(!loadMesh(&asset, "pyramid", vertices)); asset.shortRead = false;
    for (const std::string& bad : {std::string(""), std::string(" \n\t"), std::string("0 0 0 0 0"),
            std::string("0 0 0 0 0 0"), model + "oops", model + "0 0 0 0 2 0",
            model + "0 0 0 -1 0 0", model + "nan 0 0 0 0 0",
            model + "0 0 0 1 1 1 0 0 0 1 1 1 0 0 0 1 1 1", std::string(1024*1024+1, ' ')}) {
        asset.content = bad; vertices.push_back(Vertex{});
        assert(!loadMesh(&asset, "bad", vertices) && vertices.empty());
    }
    asset.content.clear();
    for (int i = 0; i < 10000; ++i) asset.content += "0 0 0 0 0 0\n1 0 0 0 0 0\n0 1 0 0 0 0\n";
    assert(loadMesh(&asset, "limit", vertices) && vertices.size() == 30000);
    asset.content += "0 0 0 0 0 0\n";
    assert(!loadMesh(&asset, "too-many", vertices) && vertices.empty());
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
    header = Path('ATLAS/app/src/main/cpp/scene/MeshLoader.h').resolve()
    source = Path('ATLAS/app/src/main/cpp/scene/MeshLoader.cpp').resolve()
    test.write_text(prefix + f'\n#include "{header}"\n' + suffix)
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', directory,
                    str(test), str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Production mesh loader validation passed')
