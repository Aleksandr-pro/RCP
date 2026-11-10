#include "../../ATLAS/app/src/main/cpp/world/TerrainMaterial.h"
#include "../../ATLAS/app/src/main/cpp/physics/TerrainBody.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
using namespace atlas::world;
using namespace atlas::physics;

static double height(double x, double z) { double h; assert(sampleTerrainMeshHeight(x,z,h)); return h; }
static SurfaceMaterial material(double x, double z) { SurfaceMaterial m{}; assert(sampleTerrainMaterial(x,z,m)); return m; }
static double findHeight(double target)
{
    for (int i=-100;i<100;++i) {
        double a=i*.5, b=a+.5, h0=height(a,0), h1=height(b,0);
        if (h0<target && h1>target) return a+.5*(target-h0)/(h1-h0);
    }
    assert(false); return 0;
}
static double firstPeak(double x, double z, SurfaceMaterial expected)
{
    double ground=height(x,z);
    assert(material(x,z)==expected);
    ContactMaterial contact{};
    assert(getContactMaterial(BodyMaterial::TestRubber,expected,contact));
    TerrainBody b{x,ground+.3+2,z,.3,0};
    bool bounced=false; double peak=0;
    for (int i=0;i<1800;++i) {
        double old=b.velocityY;
        assert(stepTerrainBody(b,contact.restitution,contact.restSpeed,1.0/60));
        assert(b.y>=ground+b.halfHeight-1e-10);
        if (old<0 && b.velocityY>0) bounced=true;
        if (bounced) peak=std::max(peak,b.y-ground-b.halfHeight);
        if (bounced && b.velocityY<=0) break;
    }
    assert(bounced); return peak;
}
int main()
{
    for (double target : {-0.20,0.35}) {
        double x=findHeight(target);
        assert(height(x-1e-8,0)<target && height(x+1e-8,0)>target);
        auto below=target<0?SurfaceMaterial::Sand:SurfaceMaterial::Ground;
        auto above=target<0?SurfaceMaterial::Ground:SurfaceMaterial::Stone;
        assert(material(x-1e-8,0)==below && material(x+1e-8,0)==above);
        // Search nearby representable coordinates for exact IEEE threshold value.
        bool exact=false; double probe=x;
        for (int i=0;i<256;++i) probe=std::nextafter(probe,-std::numeric_limits<double>::infinity());
        for (int i=0;i<512;++i) {
            if (height(probe,0)==target) { assert(material(probe,0)==above); exact=true; break; }
            probe=std::nextafter(probe,std::numeric_limits<double>::infinity());
        }
        std::cout<<"Threshold "<<target<<" exact="<<exact<<"\n";
        assert(exact);
    }
    for (double z : {-8.,-0.1,0.,8.}) for (double x : {-8.,-0.1,0.,8.}) {
        const double h=height(x,z); auto m=material(x,z);
        assert(m==(h<-.2?SurfaceMaterial::Sand:h<.35?SurfaceMaterial::Ground:SurfaceMaterial::Stone));
    }
    SurfaceMaterial m=SurfaceMaterial::Stone;
    assert(!sampleTerrainMaterial(NAN,0,m) && m==SurfaceMaterial::Stone);
    assert(!sampleTerrainMaterial(100001,0,m) && m==SurfaceMaterial::Stone);
    double sand=firstPeak(findHeight(-.30),0,SurfaceMaterial::Sand);
    double ground=firstPeak(0,0,SurfaceMaterial::Ground);
    double stone=firstPeak(findHeight(.50),0,SurfaceMaterial::Stone);
    assert(stone>ground && ground>sand);
    std::cout<<"First peaks above contact sand="<<sand<<" ground="<<ground<<" stone="<<stone<<"\n";
}
