#include "../../ATLAS/app/src/main/cpp/world/TerrainMesh.h"
#include "../../ATLAS/app/src/main/cpp/physics/TerrainBody.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>
using namespace atlas::world;
using namespace atlas::physics;
int main()
{
    // Compare against the actual rendered triangles, including shared edges and the diagonal.
    for (ChunkCoord center : {ChunkCoord{-2,-2}, ChunkCoord{0,0}, ChunkCoord{7,-5}}) {
        std::vector<atlas::Vertex> mesh;
        assert(buildTerrain(center, mesh));
        for (size_t i=0;i<mesh.size();i+=3) {
            const auto &a=mesh[i], &b=mesh[i+1], &c=mesh[i+2];
            for (auto weights : {std::pair<double,double>{0,0},{1,0},{0,1},{.2,.3},{.5,.5}}) {
                double u=weights.first,v=weights.second,w=1-u-v;
                double x=(a.position[0]*w+b.position[0]*u+c.position[0]*v)+double(center.x)*CHUNK_SIZE;
                double z=(a.position[2]*w+b.position[2]*u+c.position[2]*v)+double(center.z)*CHUNK_SIZE;
                double expected=a.position[1]*w+b.position[1]*u+c.position[1]*v, sampled=0;
                assert(sampleTerrainMeshHeight(x,z,sampled));
                assert(std::abs(sampled-expected)<1e-6);
            }
        }
    }
    double unchanged=9; assert(!sampleTerrainMeshHeight(NAN,0,unchanged) && unchanged==9);
    assert(!sampleTerrainMeshHeight(100001,0,unchanged) && unchanged==9);
    for (auto point : {std::pair<double,double>{0,0},{4,4},{-8.2,-4.7},{15.31,-2.68}}) {
        double ground=0; assert(sampleTerrainMeshHeight(point.first,point.second,ground));
        TerrainBody body{point.first,ground+2.3,point.second,.3,0};
        double lastImpact=100; int impacts=0;
        for(int i=0;i<1200;i++) {
            double prev=body.velocityY;
            assert(stepTerrainBody(body,.45,.20,1.0/60));
            assert(body.y>=ground+body.halfHeight-1e-12);
            if(prev<0 && body.velocityY>0) { assert(body.velocityY<lastImpact); lastImpact=body.velocityY; ++impacts; }
        }
        assert(impacts>=2 && body.velocityY==0 && std::abs(body.y-ground-.3)<1e-10);
        TerrainBody zero{point.first,ground+2.3,point.second,.3,0};
        for(int i=0;i<180;i++) { assert(stepTerrainBody(zero,0,.20,1./60)); assert(zero.velocityY<=0); }
        assert(zero.velocityY==0 && std::abs(zero.y-ground-.3)<1e-10);
        auto saved=zero;
        assert(!stepTerrainBody(zero,1.1,.2,.016) && zero.y==saved.y && zero.velocityY==saved.velocityY);
        assert(!stepTerrainBody(zero,.45,-1,.016) && zero.y==saved.y);
        assert(!stepTerrainBody(zero,.45,.2,.021) && zero.y==saved.y);
        assert(!stepTerrainBody(zero,NAN,.2,.016) && zero.y==saved.y);
        zero.y=ground; saved=zero;
        assert(!stepTerrainBody(zero,.45,.2,.016) && zero.y==saved.y && zero.velocityY==saved.velocityY);
    }
}
