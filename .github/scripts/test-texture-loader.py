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
    if (a->manager->shortRead && a->offset >= 7) return 0;
    size = std::min({size, size_t(7), a->manager->content.size() - a->offset});
    std::memcpy(target, a->manager->content.data() + a->offset, size);
    a->offset += size;
    return static_cast<int>(size);
}
void AAsset_close(AAsset* a) { delete a; }
'''
suffix = r'''
using atlas::loadTestTexture;
using atlas::loadOrnamentTexture;
int main() {
    std::ifstream file("ATLAS/app/src/main/assets/textures/checker.rgba", std::ios::binary);
    assert(file);
    const std::string data((std::istreambuf_iterator<char>(file)), {});
    assert(data.size() == 256);
    AAssetManager asset{data};
    std::vector<uint8_t> pixels;
    assert(loadTestTexture(&asset, "checker", pixels) && pixels.size() == 256);
    unsigned sum = 0;
    for (size_t y = 0; y < 8; ++y) for (size_t x = 0; x < 8; ++x) {
        for (size_t c = 0; c < 4; ++c) {
            uint8_t expected = c == 3 || ((x+y)%2 == 0) ? 255 : 70;
            assert(pixels[(y*8+x)*4+c] == expected);
            sum += expected;
        }
    }
    assert(sum == 47520);
    std::ifstream ornamentFile("ATLAS/app/src/main/assets/textures/ornament.rgba", std::ios::binary);
    assert(ornamentFile);
    const std::string ornament((std::istreambuf_iterator<char>(ornamentFile)), {});
    assert(ornament.size() == 1024 * 559 * 4);
    AAssetManager actual{ornament};
    assert(loadOrnamentTexture(&actual, "ornament", pixels));
    assert(pixels.size() == ornament.size());
    assert(std::equal(pixels.begin(), pixels.end(), ornament.begin(), [](uint8_t a, char b) { return a == static_cast<uint8_t>(b); }));
    for (size_t i = 3; i < pixels.size(); i += 4) assert(pixels[i] == 255);
    assert(!loadOrnamentTexture(nullptr, "ornament", pixels) && pixels.empty());
    assert(!loadOrnamentTexture(&actual, nullptr, pixels) && pixels.empty());
    actual.missing = true; assert(!loadOrnamentTexture(&actual, "ornament", pixels) && pixels.empty()); actual.missing = false;
    actual.shortRead = true; assert(!loadOrnamentTexture(&actual, "ornament", pixels) && pixels.empty()); actual.shortRead = false;
    for (size_t length : {size_t(0), ornament.size() - 1, ornament.size() + 1}) {
        actual.content.assign(length, 'x');
        assert(!loadOrnamentTexture(&actual, "ornament", pixels) && pixels.empty());
    }

    assert(!loadTestTexture(nullptr, "checker", pixels) && pixels.empty());
    assert(!loadTestTexture(&asset, nullptr, pixels) && pixels.empty());
    asset.missing = true; pixels.push_back(1); assert(!loadTestTexture(&asset, "checker", pixels) && pixels.empty()); asset.missing = false;
    asset.shortRead = true; pixels.push_back(1); assert(!loadTestTexture(&asset, "checker", pixels) && pixels.empty()); asset.shortRead = false;
    for (size_t size : {size_t(0), size_t(255), size_t(257), size_t(1024*1024)}) {
        asset.content.assign(size, 'x'); pixels.push_back(1);
        assert(!loadTestTexture(&asset, "bad", pixels) && pixels.empty());
    }
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
    header = Path('ATLAS/app/src/main/cpp/scene/TextureLoader.h').resolve()
    source = Path('ATLAS/app/src/main/cpp/scene/TextureLoader.cpp').resolve()
    test.write_text(prefix + f'\n#include "{header}"\n' + suffix)
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', directory,
                    str(test), str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Production texture loader validation passed')
