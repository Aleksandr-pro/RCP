#include "../../ATLAS/app/src/main/cpp/physics/ContactMaterials.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace atlas::physics;
constexpr float dt = 1.0f / 60.0f;
float simulate(BodyMaterial body, SurfaceMaterial surface)
{
    ContactMaterial material{};
    assert(getContactMaterial(body, surface, material));
    assert(material.restSpeed == 0.2f);
    Platform p{0,-1,0,2,.1f,1};
    Box box{.7f,.4f,0,.3f,.3f,.3f,0};
    const float floor = p.y + p.halfY + box.halfY;
    bool bounced = false;
    float firstPeak = floor;
    for (int i=0;i<1200;++i)
    {
        float oldVelocity = box.velocityY;
        assert(stepBouncingBox(p,box,material,dt));
        assert(box.y >= floor - 1e-6f);
        if (!bounced && oldVelocity < 0 && box.velocityY > 0) bounced = true;
        if (bounced && box.velocityY > 0) firstPeak = std::max(firstPeak,box.y);
        if (bounced && box.velocityY <= 0) break;
    }
    for (int i=0;i<1200;++i) { assert(stepBouncingBox(p,box,material,dt)); assert(box.y >= floor - 1e-6f); }
    assert(box.y==floor && box.velocityY==0);
    for (int i=0;i<60;++i) { assert(stepBouncingBox(p,box,material,dt)); assert(box.y==floor && box.velocityY==0); }
    return firstPeak-floor;
}
int main()
{
    ContactMaterial m{.91f,.37f};
    assert(!getContactMaterial(static_cast<BodyMaterial>(999),SurfaceMaterial::Stone,m));
    assert(m.restitution==.91f && m.restSpeed==.37f);
    assert(!getContactMaterial(BodyMaterial::TestRubber,static_cast<SurfaceMaterial>(-1),m));
    assert(m.restitution==.91f && m.restSpeed==.37f);
    const BodyMaterial bodies[] = {BodyMaterial::TestRubber,BodyMaterial::TestWood};
    const SurfaceMaterial surfaces[] = {SurfaceMaterial::Stone,SurfaceMaterial::Ground,SurfaceMaterial::Sand};
    const float expected[2][3] = {{.70f,.40f,.10f},{.35f,.20f,.05f}};
    for (int i=0;i<2;++i) for (int j=0;j<3;++j) { assert(getContactMaterial(bodies[i],surfaces[j],m)); assert(m.restitution==expected[i][j] && m.restSpeed==.20f); }
    float rubberStone=simulate(BodyMaterial::TestRubber,SurfaceMaterial::Stone);
    float rubberSand=simulate(BodyMaterial::TestRubber,SurfaceMaterial::Sand);
    float woodStone=simulate(BodyMaterial::TestWood,SurfaceMaterial::Stone);
    assert(rubberStone>rubberSand && rubberStone>woodStone);
    std::cout << "First rebound heights from identical initial state: rubber/stone=" << rubberStone << " rubber/sand=" << rubberSand << " wood/stone=" << woodStone << '\n';
}
