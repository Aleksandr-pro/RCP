#include "../../ATLAS/app/src/main/cpp/world/ActiveGrass.h"
#include "../../ATLAS/app/src/main/cpp/world/TerrainMesh.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>
using namespace atlas::world;
static bool same(const GrassPlacement& a,const GrassPlacement& b) {
    return a.x==b.x && a.y==b.y && a.z==b.z && a.height==b.height && a.width==b.width && a.rotationY==b.rotationY && a.id==b.id;
}
static bool same(const std::vector<GrassPlacement>& a,const std::vector<GrassPlacement>& b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](auto& x,auto& y){return same(x,y);});
}
int main() {
    for (int z=-8; z<=8; ++z) for (int x=-8; x<=8; ++x) {
        GrassPlacement a[36]{},b[36]{};
        auto sample=[](double x,double z) { double h=0; assert(sampleTerrainMeshHeight(x,z,h)); return h; };
        auto n=generateGrassForChunk(x,z,CHUNK_SIZE,sample,a,36);
        auto m=generateGrassForChunk(x,z,CHUNK_SIZE,sample,b,36);
        assert(n==m && n<=36);
        assert(generateGrassForChunk(x,z,CHUNK_SIZE,sample,nullptr,36)==0);
        assert(generateGrassForChunk(x,z,CHUNK_SIZE,sample,b,0)==0);
        assert(generateGrassForChunk(x,z,0,sample,b,36)==0);
        auto limited=generateGrassForChunk(x,z,CHUNK_SIZE,sample,b,3);
        assert(limited==std::min(size_t(3),n));
        for (size_t i=0;i<n;++i) {
            assert(a[i].x>x*8.0 && a[i].x<(x+1)*8.0 && a[i].z>z*8.0 && a[i].z<(z+1)*8.0);
            assert(std::abs(a[i].y+0.025-sample(a[i].x,a[i].z))<1e-12);
            assert(a[i].height>=.18f && a[i].height<.35f && a[i].width>=.1f && a[i].width<.18f);
            if (i<limited) assert(same(a[i],b[i]));
        }
    }
    GrassPlacement p{}; p.id=1;
    assert(isGrassVisible(p,10,0)); assert(!isGrassVisible(p,10.0001,0));
    p.id=2; assert(isGrassVisible(p,18,0)); assert(!isGrassVisible(p,18.0001,0));
    double x,z;
    worldCameraXZ(2,3,0,0,1,0.5,x,z); assert(std::abs(x-2)<1e-9 && std::abs(z-1)<1e-9);
    worldCameraXZ(2,3,1.57079632679f,0,1,0.5,x,z); assert(std::abs(x-4)<1e-6 && std::abs(z-3)<1e-6);
    std::vector<GrassPlacement> initial, after, scratch;
    assert(buildActiveGrass({-1,-1},initial) && initial.size()<=324);
    for (int i=-40;i<=40;++i) assert(buildActiveGrass({i,-i},scratch) && scratch.size()<=324);
    assert(buildActiveGrass({-1,-1},after) && same(initial,after));
    assert(!buildActiveGrass({12000,0},after) && same(initial,after));
    size_t close=0,far=0;
    for (const auto& v:initial) { close += isGrassVisible(v,-4,-4); far += isGrassVisible(v,1000,1000); }
    assert(close>0 && far==0);
}
