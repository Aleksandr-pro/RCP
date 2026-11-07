#include "../../ATLAS/app/src/main/cpp/physics/JumpTest.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
using namespace atlas::physics;
constexpr float dt = 1.f / 60.f;
int main()
{
    const Platform p{0,-1,0,2,.1f,1};
    const std::vector<Box> initial{{-.6f,0,0,.3f,.3f,.3f,0},{-.6f,1,0,.3f,.3f,.3f,0},{.7f,.4f,0,.3f,.3f,.3f,0}};
    auto run = [&]() {
        auto boxes = initial;
        JumpTest test{};
        assert(!updateJumpTest(test,p,boxes,3,dt));
        assert(!updateJumpTest(test,p,boxes,2,0));
        assert(!updateJumpTest(test,p,boxes,2,.021f));
        assert(!updateJumpTest(test,p,boxes,2,std::numeric_limits<float>::quiet_NaN()));
        int land = -1, jump = -1, secondLand = -1, starts = 0;
        float peak = -100.f;
        bool previousJumped = false;
        for (int i=0; i<600; ++i) {
            if (land < 0 && std::abs(boxes[2].y+.6f) < 1e-5f && boxes[2].velocityY == 0) land=i;
            assert(updateJumpTest(test,p,boxes,2,dt));
            if (test.jumped && !previousJumped) { jump=i; ++starts; assert(boxes[2].velocityY == 4.f); }
            previousJumped = test.jumped;
            assert(step(p,boxes,dt));
            if (test.jumped) peak = std::max(peak, boxes[2].y);
            if (test.jumped && i>jump && boxes[2].velocityY==0 && std::abs(boxes[2].y+.6f)<1e-5f && secondLand<0) secondLand=i;
            assert(boxes[1].y-boxes[1].halfY >= boxes[0].y+boxes[0].halfY-1e-5f);
        }
        assert(land>=0 && jump>=land+59 && jump<=land+61);
        assert(starts==1 && secondLand>jump && test.jumped);
        assert(peak > .15f && peak < .25f); // about 0.8 above center at rest
        assert(std::abs(boxes[0].y+.6f)<1e-5f && std::abs(boxes[1].y)<1e-5f && std::abs(boxes[2].y+.6f)<1e-5f);
        for (const auto& box : boxes) assert(box.velocityY == 0);
        std::cout << "land=" << land << " jump=" << jump << " re-land=" << secondLand << " peak=" << peak << '\n';
    };
    run(); run(); // Fresh simulation state re-arms the test.
    auto boxes=initial; boxes[2].y=-.6f;
    JumpTest pause{};
    for (int i=0; i<30; ++i) assert(updateJumpTest(pause,p,boxes,2,dt));
    const auto before=pause; const auto body=boxes[2];
    // No updates while unfocused: wall-clock duration is deliberately irrelevant.
    assert(pause.groundedTime==before.groundedTime && pause.jumped==before.jumped && boxes[2].y==body.y);
    for (int i=0; i<60 && !pause.jumped; ++i) assert(updateJumpTest(pause,p,boxes,2,dt));
    assert(pause.jumped && boxes[2].velocityY==4.f);
    std::cout << "One-shot jump, stack, rearm, invalid args and paused wait passed\n";
}
