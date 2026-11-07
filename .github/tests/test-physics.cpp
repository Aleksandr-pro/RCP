#include "../../ATLAS/app/src/main/cpp/physics/FallingCube.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    atlas::SceneObject platform{};
    platform.meshName = "cube";
    platform.y = -0.85f;
    platform.scale = 2.2f; platform.scaleY = 0.2f; platform.scaleZ = 1.5f;
    atlas::SceneObject cube{};
    cube.meshName = "cube";
    cube.y = 0.9f;
    cube.scale = cube.scaleY = cube.scaleZ = 0.55f;
    atlas::FallingCube a, b, c;
    assert(a.configure(platform, cube) && b.configure(platform, cube) && c.configure(platform, cube));
    assert(std::abs(a.restY() - (-0.475)) < 1e-6);
    const double initialY = a.y();
    a.advance(1.0 / 60.0);
    assert(a.y() < initialY && a.velocity() < 0);
    b.advance(10.0); c.advance(0.25);
    assert(std::abs(b.y() - c.y()) < 1e-10 && std::abs(b.velocity() - c.velocity()) < 1e-10);
    double previous = a.y();
    for (int i = 0; i < 600; ++i) {
        a.advance(1.0 / 60.0);
        assert(a.y() <= previous + 1e-10 && a.y() >= a.restY());
        previous = a.y();
    }
    assert(a.y() == a.restY() && a.velocity() == 0.0);
    for (int i = 0; i < 600; ++i) { a.advance(1.0 / 30.0); assert(a.y() == a.restY() && a.velocity() == 0.0); }
    atlas::FallingCube paused, control;
    assert(paused.configure(platform, cube) && control.configure(platform, cube));
    paused.advance(0.01); paused.resetAccumulator();
    paused.advance(0.01); control.advance(0.01);
    assert(paused.y() == control.y());
    cube.x = 2.0f; assert(!a.configure(platform, cube));
    std::cout << "Physics: fixed step, clamp, collision, stability and pause accumulator passed\n";
}
