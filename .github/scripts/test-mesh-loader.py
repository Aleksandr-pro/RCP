"""Run the actual production loader with a stub Android asset API on the host."""
from pathlib import Path
import subprocess
import tempfile

source = Path('ATLAS/app/src/main/cpp/renderer/VulkanRenderer.cpp').read_text()
start = source.index('    struct Vertex\n')
end = source.index('    // Матрица 4×4.', start)
loader = source[start:end]
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
constexpr const char* LOG_TAG = "ATLAS";
constexpr int ANDROID_LOG_INFO = 4, ANDROID_LOG_ERROR = 6, AASSET_MODE_STREAMING = 2;
int __android_log_print(int, const char*, const char*, ...) { return 0; }
struct AAssetManager { std::string content; bool missing = false, shortRead = false; };
struct AAsset { AAssetManager* manager; size_t offset = 0; };
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
int main() {
    std::ifstream file("ATLAS/app/src/main/assets/models/pyramid.atlasmesh");
    assert(file);
    const std::string model((std::istreambuf_iterator<char>(file)), {});
    AAssetManager asset{model};
    std::vector<Vertex> vertices;
    assert(loadMesh(&asset, "pyramid", vertices) && vertices.size() == 18);
    assert(vertices[0].position[1] == 0.5f && vertices[0].color[1] == 0.80f);
    assert(!loadMesh(nullptr, "pyramid", vertices) && vertices.empty());
    assert(!loadMesh(&asset, nullptr, vertices));
    asset.missing = true; assert(!loadMesh(&asset, "pyramid", vertices)); asset.missing = false;
    asset.shortRead = true; assert(!loadMesh(&asset, "pyramid", vertices)); asset.shortRead = false;
    for (const std::string& bad : {std::string(""), std::string(" \n\t"), std::string("0 0 0 0 0"),
            std::string("0 0 0 0 0 0"), model + "oops", model + "0 0 0 0 2 0",
            model + "0 0 0 -1 0 0", model + "nan 0 0 0 0 0", std::string(1024*1024+1, ' ')}) {
        asset.content = bad; vertices.push_back(Vertex{});
        assert(!loadMesh(&asset, "bad", vertices) && vertices.empty());
    }
    asset.content.clear();
    for (int i = 0; i < 30000; ++i) asset.content += "0 0 0 0 0 0\n";
    assert(loadMesh(&asset, "limit", vertices) && vertices.size() == 30000);
    asset.content += "0 0 0 0 0 0\n";
    assert(!loadMesh(&asset, "too-many", vertices) && vertices.empty());
}
'''
with tempfile.TemporaryDirectory() as directory:
    test = Path(directory) / 'test.cpp'
    exe = Path(directory) / 'test'
    test.write_text(prefix + loader + suffix)
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', str(test), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Production mesh loader validation passed')
