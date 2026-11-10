#pragma once
#include <borealis.hpp>
#include "texture_browser.h"
namespace borealis_app {
class TexturePreviewView:public brls::Image {
public:
    TexturePreviewView();
    bool show(const TextureBrowser::Preview& preview);
    void draw(NVGcontext* vg,float x,float y,float width,float height,
              brls::Style style,brls::FrameContext* ctx) override;
};
}
