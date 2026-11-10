#include "../../ATLAS/app/src/main/cpp/world/ActiveStones.h"
#include "../../ATLAS/app/src/main/cpp/world/TerrainMesh.h"
#include <cassert>
#include <cmath>
#include <vector>
using namespace atlas::world;

static bool same(const StonePlacement& a, const StonePlacement& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.scale == b.scale && a.rotationY == b.rotationY;
}
static bool same(const std::vector<StonePlacement>& a, const std::vector<StonePlacement>& b) {
    if (a.size()!=b.size()) return false;
    for (size_t i=0;i<a.size();++i) if (!same(a[i],b[i])) return false;
    return true;
}
int main() {
    for (int z=-5; z<=5; ++z) for (int x=-5; x<=5; ++x) {
        const ChunkCoord c{x,z}; std::vector<StonePlacement> a,b;
        assert(generateStonePlacements(c,a) && generateStonePlacements(c,b) && same(a,b));
        assert(a.size()<=16);
        for (const auto& p:a) {
            assert(p.x>double(x)*CHUNK_SIZE && p.x<double(x+1)*CHUNK_SIZE);
            assert(p.z>double(z)*CHUNK_SIZE && p.z<double(z+1)*CHUNK_SIZE);
            assert(p.scale>=0.1f && p.scale<0.24f && p.rotationY>=0 && p.rotationY<6.283186f);
            double h=0; assert(sampleTerrainMeshHeight(p.x,p.z,h) && p.y==h);
            // Independently interpolate the actual rendered triangle vertices.
            constexpr double step=double(CHUNK_SIZE)/TERRAIN_CELLS;
            int ix=int(std::floor((p.x-double(x)*CHUNK_SIZE)/step));
            int iz=int(std::floor((p.z-double(z)*CHUNK_SIZE)/step));
            double u=(p.x-(double(x)*CHUNK_SIZE+ix*step))/step;
            double v=(p.z-(double(z)*CHUNK_SIZE+iz*step))/step;
            auto v00=terrainVertex(c,c,ix,iz), v10=terrainVertex(c,c,ix+1,iz);
            auto v01=terrainVertex(c,c,ix,iz+1), v11=terrainVertex(c,c,ix+1,iz+1);
            double mesh=u+v<=1 ? (1-u-v)*v00.position[1]+u*v10.position[1]+v*v01.position[1]
                : (1-v)*v10.position[1]+(1-u)*v01.position[1]+(u+v-1)*v11.position[1];
            assert(std::abs(mesh-p.y)<1e-6);
            assert(std::abs((p.y+p.scale*0.5)-p.scale*0.5-p.y)<1e-12);
        }
    }
    std::vector<StonePlacement> initial, after, scratch;
    assert(buildActiveStones({-1,0},initial) && initial.size()<=144);
    for (int x=-10;x<=10;++x) assert(buildActiveStones({x,-3},scratch) && scratch.size()<=144);
    assert(buildActiveStones({-1,0},after) && same(initial,after));
    assert(!generateStonePlacements({12001,0},after) && same(initial,after));
    assert(!buildActiveStones({12000,0},after) && same(initial,after));
    assert(generateStonePlacements({-12000,12000},scratch));
}
