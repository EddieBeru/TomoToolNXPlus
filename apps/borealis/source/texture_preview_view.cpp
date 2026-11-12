#include "texture_preview_view.hpp"
#include "texture_preview_layout.hpp"
namespace borealis_app {
TexturePreviewView::TexturePreviewView(){
    setFreeTexture(true);setScalingType(brls::ImageScalingType::FIT);
    setHeight(320);setGrow(0);setShrink(0);
    setHideHighlightBackground(true);
}
void TexturePreviewView::clear(){
    brls::Image::clear();
    setFocusable(false);
}
bool TexturePreviewView::show(const TextureBrowser::Preview& p){
    clear();
    if(p.width<1 || p.width>1024 || p.height<1 || p.height>1024 ||
       p.rgba.size()!=size_t(p.width)*p.height*4)return false;
    int texture=nvgCreateImageRGBA(brls::Application::getNVGContext(),p.width,p.height,0,p.rgba.data());
    if(!texture)return false;
    innerSetImage(texture);setFocusable(true);return true;
}
void TexturePreviewView::draw(NVGcontext* vg,float x,float y,float width,float height,
                             brls::Style,brls::FrameContext*){
    if(!getTexture())return;
    const auto bounds=fitPreview(width,height,getOriginalImageWidth(),getOriginalImageHeight());
    if(bounds.width<=0 || bounds.height<=0)return;
    const float left=x+bounds.x, top=y+bounds.y;
    nvgSave(vg);nvgIntersectScissor(vg,left,top,bounds.width,bounds.height);
    constexpr float tile=20;
    for(int row=0;row<bounds.height/tile+1;++row)for(int col=0;col<bounds.width/tile+1;++col){
        nvgBeginPath(vg);nvgRect(vg,left+col*tile,top+row*tile,tile,tile);
        unsigned char shade=(row+col)%2?115:175;
        nvgFillColor(vg,a(nvgRGB(shade,shade,shade)));nvgFill(vg);
    }
    // Fill only the fitted image, never the surrounding view's wider rectangle.
    const auto paint=nvgImagePattern(vg,left,top,bounds.width,bounds.height,0,getTexture(),1);
    nvgBeginPath(vg);nvgRect(vg,left,top,bounds.width,bounds.height);
    nvgFillPaint(vg,a(paint));nvgFill(vg);
    nvgRestore(vg);
}
}
