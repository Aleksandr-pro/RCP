#pragma once

#include "../scene/TestScene.h"

namespace atlas
{
    // One axis-aligned dynamic cube and one axis-aligned static platform.
    class FallingCube
    {
    public:
        bool configure(const SceneObject& platform, const SceneObject& cube);
        void advance(double elapsedSeconds);
        void resetAccumulator();
        bool configured() const { return configured_; }
        double y() const { return y_; }
        double velocity() const { return velocity_; }
        double restY() const { return restY_; }
    private:
        bool configured_ = false;
        double y_ = 0.0;
        double velocity_ = 0.0;
        double restY_ = 0.0;
        double accumulator_ = 0.0;
    };
}
