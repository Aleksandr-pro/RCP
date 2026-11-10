#include "../../ATLAS/app/src/main/cpp/world/ChunkWorld.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <set>
#include <utility>
using namespace atlas::world;
int main() {
    ChunkCoord c{};
    for (auto p : {std::pair<float,int>{0,0},{7.99f,0},{8,1},{-.1f,-1},{-8,-1},{-8.1f,-2}}) {
        assert(worldToChunk(p.first,p.first,c)); assert(c.x==p.second && c.z==p.second);
    }
    c={12,13}; assert(!worldToChunk(std::numeric_limits<float>::infinity(),0,c) && c.x==12 && c.z==13);
    assert(!worldToChunk(8000010.f,0,c) && c.x==12);
    std::vector<ChunkCoord> v{{22,23}};
    assert(!collectActiveChunks({0,0},5,v) && v.size()==1 && v[0].x==22);
    assert(!collectActiveChunks({1000001,0},1,v) && v.size()==1 && v[0].x==22);
    assert(collectActiveChunks({0,0},4,v) && v.size()==81);
    ChunkWorld w; bool changed=false; assert(w.moveTo(0,0,changed) && changed);
    auto initial=w.active();
    for (int pass=0; pass<2; ++pass) for (int j=0;j<=20000;++j) {
        int i = pass ? 10000-j : j-10000;
        assert(w.moveTo(i*8.f, -i*8.f, changed)); assert(w.active().size()==9);
        std::set<std::pair<int,int>> unique;
        for(auto item:w.active()) { unique.emplace(item.x,item.z); assert(std::abs(item.x-w.center().x)<=1 && std::abs(item.z-w.center().z)<=1); }
        assert(unique.size()==9);
    }
    assert(w.moveTo(0,0,changed) && changed && w.active()==initial);
    assert(w.moveTo(7.99f,.1f,changed) && !changed);
    auto before=w.active(); assert(!w.moveTo(NAN,0,changed) && w.active()==before);
}
