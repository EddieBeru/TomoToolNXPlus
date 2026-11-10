#include "texture_preview_view.hpp"
#include <algorithm>
namespace borealis_app {
TexturePreviewView::TexturePreviewView(){
    setFreeTexture(true);setScalingType(brls::ImageScalingType::FIT);setHeight(320);setGrow(1);
}
bool TexturePreviewView::show(const TextureBrowser::Preview& p){
    clear();
    if(p.width<1 || p.width>1024 || p.height<1 || p.height>1024 ||
       p.rgba.size()!=size_t(p.width)*p.height*4)return false;
    int texture=nvgCreateImageRGBA(brls::Application::getNVGContext(),p.width,p.height,0,p.rgba.data());
    if(!texture)return false;
    innerSetImage(texture);return true;
}
void TexturePreviewView::draw(NVGcontext* vg,float x,float y,float width,float height,
                             brls::Style style,brls::FrameContext* ctx){
    nvgSave(vg);nvgIntersectScissor(vg,x,y,width,height);
    constexpr float tile=20;
    for(int row=0;row<height/tile+1;++row)for(int col=0;col<width/tile+1;++col){
        nvgBeginPath(vg);nvgRect(vg,x+col*tile,y+row*tile,tile,tile);
        unsigned char shade=(row+col)%2?115:175;
        nvgFillColor(vg,nvgRGB(shade,shade,shade));nvgFill(vg);
    }
    nvgRestore(vg);brls::Image::draw(vg,x,y,width,height,style,ctx);
}
}
