#include "../../ATLAS/app/src/main/cpp/physics/SurfaceSlide.h"
#include "../../ATLAS/app/src/main/cpp/physics/ContactMaterials.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
using namespace atlas::physics;
constexpr float dt=1.f/60.f;
const Platform platform{0,-1,0,3,.1f,1};
const Box initial{.4f,.4f,0,.3f,.3f,.3f,0};
float run(BodyMaterial body, bool grounded = false) {
    ContactMaterial c{}; float mu=0;
    assert(getContactMaterial(body,SurfaceMaterial::Stone,c) && getSlidingFriction(body,SurfaceMaterial::Stone,mu));
    Box b=initial; if (grounded) b.y = platform.y + platform.halfY + b.halfY; SlideMotion m{.7f,0}; bool airborne=false, hit=false, braked=false;
    for(int i=0;i<900;++i) {
        float oldv=m.velocityX; float oldy=b.velocityY;
        assert(stepSlidingBox(platform,b,m,c,mu,dt));
        assert(b.x-b.halfX > -.6f + .3f && b.x+b.halfX <= platform.x+platform.halfX);
        assert(b.y-b.halfY >= platform.y+platform.halfY-1e-6f);
        if (b.y-b.halfY > platform.y+platform.halfY+1e-4f) { airborne=true; assert(m.velocityX==oldv); }
        if(oldy<0 && b.velocityY>0) hit=true;
        if(m.velocityX < oldv) braked=true;
        assert(m.velocityX >= 0 && m.velocityX <= oldv);
    }
    assert((grounded || (airborne && hit)) && braked && m.velocityX==0 && b.velocityY==0);
    float x=b.x;
    for(int i=0;i<60;++i){assert(stepSlidingBox(platform,b,m,c,mu,dt));assert(x==b.x && m.velocityX==0);}
    return x-initial.x;
}
int main(){
    float rubber=run(BodyMaterial::TestRubber, true), wood=run(BodyMaterial::TestWood, true);
    float fallingRubber=run(BodyMaterial::TestRubber);
    float fallingWood=run(BodyMaterial::TestWood);
    std::cout << "rubber distance="<<rubber<<", wood distance="<<wood<<'\n';
    assert(wood>rubber);
    std::cout << "total distance from airborne start: rubber=" << fallingRubber << ", wood=" << fallingWood << '\n';
    Box b=initial; SlideMotion m{.7f,.1f}; ContactMaterial c{};
    for(int i=0;i<100;++i) { assert(stepSlidingBox(platform,b,m,c,0,dt)); assert(m.velocityX==.7f && m.velocityZ==.1f); }
    b.x=platform.x+platform.halfX-b.halfX-.001f; b.z=0; m={.7f,0}; Box old=b; SlideMotion oldm=m;
    assert(!stepSlidingBox(platform,b,m,c,.6f,dt));
    assert(b.x==old.x && b.y==old.y && b.z==old.z && b.velocityY==old.velocityY && m.velocityX==oldm.velocityX && m.velocityZ==oldm.velocityZ);
    assert(!stepSlidingBox(platform,b,m,c,-1,dt));
    float mu=123; assert(!getSlidingFriction(static_cast<BodyMaterial>(99),SurfaceMaterial::Stone,mu) && mu==123);
    assert(!getSlidingFriction(BodyMaterial::TestWood,static_cast<SurfaceMaterial>(99),mu) && mu==123);
    std::cout << "Slide, friction, isolated stack, edge rollback, zero friction and invalid inputs passed\n";
}
