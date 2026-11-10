#include "../../ATLAS/app/src/main/cpp/world/TerrainMesh.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
using namespace atlas::world;
int main()
{
    for (int x : {-100, -2, -1, 0, 1, 100}) for (int z : {-100, -2, -1, 0, 1, 100}) {
        const ChunkCoord base{x,z};
        for (int i=0; i<=TERRAIN_CELLS; ++i) {
            auto a=terrainVertex(base,base,16,i), b=terrainVertex({x+1,z},base,0,i);
            auto c=terrainVertex(base,base,i,16), d=terrainVertex({x,z+1},base,i,0);
            assert(a.uv[0]*CHUNK_SIZE == b.uv[0]*CHUNK_SIZE);
            assert(a.uv[1]*CHUNK_SIZE == b.uv[1]*CHUNK_SIZE);
            assert(std::memcmp(&a,&b,sizeof(a)) == 0);
            assert(std::memcmp(&c,&d,sizeof(c)) == 0);
            for (auto v : {a,c}) {
                float length=std::sqrt(v.normal[0]*v.normal[0]+v.normal[1]*v.normal[1]+v.normal[2]*v.normal[2]);
                assert(v.normal[1] > 0 && std::abs(length-1.0f)<1e-6f);
            }
        }
    }
    std::vector<atlas::Vertex> vertices;
    assert(buildTerrain({-1,-1},vertices) && vertices.size()==TERRAIN_VERTICES);
    auto original=vertices;
    for (size_t i=0; i<vertices.size(); i+=3) {
        auto a=vertices[i], b=vertices[i+1], c=vertices[i+2];
        float y=(b.position[2]-a.position[2])*(c.position[0]-a.position[0]) - (b.position[0]-a.position[0])*(c.position[2]-a.position[2]);
        assert(y>0);
    }
    for(int i=-100;i<=100;i++) { assert(buildTerrain({i,-i},vertices)); assert(vertices.size()==TERRAIN_VERTICES); }
    assert(buildTerrain({-1,-1},vertices));
    assert(std::memcmp(vertices.data(),original.data(),vertices.size()*sizeof(atlas::Vertex))==0);
    auto before=vertices; assert(!buildTerrain({1000001,0},vertices));
    assert(std::memcmp(vertices.data(),before.data(),vertices.size()*sizeof(atlas::Vertex))==0);
}
