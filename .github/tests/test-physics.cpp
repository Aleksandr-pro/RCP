#include "../../ATLAS/app/src/main/cpp/physics/VerticalPhysics.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
using namespace atlas::physics;
static constexpr float dt = 1.0f / 60.0f;
static bool close(float a, float b) { return std::abs(a-b) < 0.00005f; }
int main()
{
    Platform platform{0, -1, 0, 2, .1f, 1};
    std::vector<Box> initial{{-.6f,0,0,.3f,.3f,.3f,0},{-.6f,1,0,.3f,.3f,.3f,0},{.7f,.4f,0,.3f,.3f,.3f,0}};
    assert(validate(platform, initial));
    auto boxes = initial;
    auto reversed = initial;
    std::reverse(reversed.begin(), reversed.end());
    for (int i=0; i<600; ++i) {
        assert(step(platform, boxes, dt));
        assert(step(platform, reversed, dt));
        for (size_t k=0; k<boxes.size(); ++k) {
            assert(close(boxes[k].y, reversed[boxes.size()-1-k].y));
            assert(close(boxes[k].velocityY, reversed[boxes.size()-1-k].velocityY));
            assert(boxes[k].y - boxes[k].halfY >= platform.y + platform.halfY - .00001f);
        }
        assert(boxes[1].y-boxes[1].halfY >= boxes[0].y+boxes[0].halfY - .00001f);
    }
    assert(close(boxes[0].y, -.6f) && close(boxes[1].y, 0.f) && close(boxes[2].y, -.6f));
    for (const auto& b : boxes) assert(b.velocityY == 0.0f);
    auto settled = boxes;
    for (int i=0; i<600; ++i) { assert(step(platform, boxes, dt)); for (size_t j=0;j<boxes.size();++j) assert(boxes[j].y == settled[j].y && boxes[j].velocityY == 0); }
    auto invalid=initial; invalid[1].y=0.4f; assert(!validate(platform, invalid));
    assert(!step(platform, invalid, dt));
    assert(!step(platform, boxes, 0.0f) && !step(platform, boxes, 0.021f));
    assert(!step(platform, boxes, std::numeric_limits<float>::quiet_NaN()));
    std::vector<Box> catchup{{0, 2, 0, .3f,.3f,.3f,-1.f},{0,2.7f,0,.3f,.3f,.3f,-5.f}};
    assert(validate(platform, catchup));
    bool followed=false;
    for (int i=0;i<30;++i) { assert(step(platform, catchup, dt)); assert(catchup[1].y-catchup[1].halfY >= catchup[0].y+catchup[0].halfY-0.00001f); if (catchup[0].velocityY < 0 && close(catchup[1].y-catchup[1].halfY,catchup[0].y+catchup[0].halfY)) { assert(close(catchup[1].velocityY,catchup[0].velocityY)); followed=true; } }
    assert(followed);
    std::vector<Box> separate{{-1,0,0,.3f,.3f,.3f,0},{1,.6f,0,.3f,.3f,.3f,0}};
    for (int i=0;i<600;++i) assert(step(platform, separate, dt));
    assert(close(separate[0].y,-.6f) && close(separate[1].y,-.6f));
    std::cout << "Three cubes, permutation, independent supports, moving support, stability and validation passed\n";
}
