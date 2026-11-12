#include "texture_preview_layout.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

static bool near(float a,float b){return std::abs(a-b)<0.001f;}
static void check(float width,float height,float iw,float ih,
                  float x,float y,float fittedWidth,float fittedHeight){
    const auto result=borealis_app::fitPreview(width,height,iw,ih);
    assert(near(result.x,x) && near(result.y,y));
    assert(near(result.width,fittedWidth) && near(result.height,fittedHeight));
    assert(near(result.width/result.height,iw/ih));
    assert(result.x>=0 && result.y>=0);
    assert(result.x+result.width<=width+0.001f);
    assert(result.y+result.height<=height+0.001f);
}
int main(){
    check(900,320,512,512,290,0,320,320);
    check(900,320,256,512,370,0,160,320);
    check(900,320,256,128,130,0,640,320);
    check(900,320,512,128,0,47.5f,900,225);
    check(120,320,512,512,0,100,120,120);
    check(320,320,384,384,0,0,320,320);
    for(float invalid:{0.0f,-1.0f,std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::quiet_NaN()}){
        assert(borealis_app::fitPreview(invalid,320,512,512).width==0);
        assert(borealis_app::fitPreview(900,invalid,512,512).height==0);
        assert(borealis_app::fitPreview(900,320,invalid,512).width==0);
        assert(borealis_app::fitPreview(900,320,512,invalid).height==0);
    }
    std::puts("Texture preview aspect ratio and bounded drawing: OK");
}
