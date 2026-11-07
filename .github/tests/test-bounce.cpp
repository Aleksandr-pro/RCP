#include "../../ATLAS/app/src/main/cpp/physics/SurfaceBounce.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>
using namespace atlas::physics;
constexpr float dt = 1.0f / 60.0f;
const Platform platform{0,-1,0,2,.1f,1};
const Box initial{.7f,.4f,0,.3f,.3f,.3f,0};
struct Result { std::vector<float> peaks; int impacts = 0; Box last; };
Result simulate(float restitution)
{
    Box box = initial;
    ContactMaterial material{restitution, .2f};
    const float floor = platform.y + platform.halfY + box.halfY;
    Result result;
    bool rising = false;
    float peak = floor;
    for (int i=0; i<1200; ++i)
    {
        float prev = box.velocityY;
        assert(stepBouncingBox(platform, box, material, dt));
        assert(box.y >= floor - 1e-6f && std::isfinite(box.y));
        if (prev < 0 && box.velocityY > 0 && std::abs(box.y-floor)<1e-6f) { ++result.impacts; rising=true; peak=floor; }
        if (rising) {
            peak = std::max(peak, box.y);
            if (box.velocityY <= 0) { result.peaks.push_back(peak-floor); rising=false; }
        }
    }
    assert(box.y == floor && box.velocityY == 0.0f);
    result.last=box;
    return result;
}
int main()
{
    auto a=simulate(.45f), b=simulate(.7f), c=simulate(0.f);
    assert(a.impacts >= 2 && a.peaks.size() >= 2);
    for (size_t i=1; i<a.peaks.size(); ++i) assert(a.peaks[i] < a.peaks[i-1]);
    assert(!b.peaks.empty() && b.peaks[0] > a.peaks[0] * 1.5f);
    assert(c.impacts == 0 && c.peaks.empty());
    Box box=initial; ContactMaterial m{}; const Box before=box;
    assert(!stepBouncingBox(platform,box,m,0));
    assert(!stepBouncingBox(platform,box,m,.021f));
    assert(!stepBouncingBox(platform,box,m,std::numeric_limits<float>::quiet_NaN()));
    m.restitution=1.1f; assert(!stepBouncingBox(platform,box,m,dt));
    m.restitution=.45f; m.restSpeed=-1; assert(!stepBouncingBox(platform,box,m,dt));
    assert(box.y == before.y && box.velocityY == before.velocityY);
    m.restSpeed=.2f; box.y=-1.f; assert(!stepBouncingBox(platform,box,m,dt));
    std::cout << "bounce peaks e=.45: "; for(auto peak:a.peaks) std::cout << peak << ' ';
    std::cout << " e=.7 first=" << b.peaks[0] << "; zero, landing, invalid inputs passed\n";
}
