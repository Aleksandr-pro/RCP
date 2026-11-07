#include "FallingCube.h"
#include <algorithm>
#include <cmath>

namespace atlas
{
    bool FallingCube::configure(const SceneObject& platform, const SceneObject& cube)
    {
        configured_ = false;
        accumulator_ = 0.0;
        velocity_ = 0.0;
        if (platform.meshName != "cube" || cube.meshName != "cube" ||
            platform.rotationOffset != 0.0f || cube.rotationOffset != 0.0f ||
            std::abs(cube.x - platform.x) + cube.scale * 0.5 > platform.scale * 0.5 ||
            std::abs(cube.z - platform.z) + cube.scaleZ * 0.5 > platform.scaleZ * 0.5)
            return false;
        restY_ = double(platform.y) + double(platform.scaleY) * 0.5 + double(cube.scaleY) * 0.5;
        y_ = cube.y;
        if (y_ < restY_) return false;
        configured_ = true;
        return true;
    }

    void FallingCube::advance(double elapsedSeconds)
    {
        if (!configured_ || !std::isfinite(elapsedSeconds) || elapsedSeconds <= 0.0) return;
        constexpr double step = 1.0 / 60.0;
        accumulator_ = std::min(accumulator_ + elapsedSeconds, 0.25);
        while (accumulator_ + 1e-12 >= step)
        {
            velocity_ -= 9.81 * step;
            y_ += velocity_ * step;
            if (y_ <= restY_ && velocity_ < 0.0)
            {
                y_ = restY_;
                velocity_ = 0.0;
            }
            accumulator_ -= step;
        }
    }

    void FallingCube::resetAccumulator() { accumulator_ = 0.0; }
}
