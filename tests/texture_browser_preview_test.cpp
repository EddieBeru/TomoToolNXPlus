#include "texture_browser.h"
#include "support/texture_browser_fixtures.h"
#include <zstd.h>
#include <cassert>
#include <iostream>
static std::vector<uint8_t> compress(std::vector<uint8_t> raw) {
    std::vector<uint8_t> out(ZSTD_compressBound(raw.size()));
    out.resize(ZSTD_compress(out.data(),out.size(),raw.data(),raw.size(),3));return out;
}
int main(){
    using namespace TextureBrowser;
    Fixtures::Source s;
    Entry entry;entry.id={{{1,0}},"UgcFood000"};
    entry.mainRef="/Ugc/UgcFood000.ugctex.zs";entry.canvasRef="/Ugc/UgcFood000.canvas.zs";
    s.files[entry.mainRef]=compress(std::vector<uint8_t>(131072,0));
    s.files[entry.canvasRef]=compress(std::vector<uint8_t>(256*128*4,255));
    auto p=loadPreview(s,entry,[]{return false;});
    assert(p && p.value.canvas && p.value.width==256 && p.value.height==128);
    s.files[entry.canvasRef]={1,2,3};
    p=loadPreview(s,entry,[]{return false;});assert(p && !p.value.canvas && p.value.fallback);
    s.files[entry.mainRef]={1,2,3};
    p=loadPreview(s,entry,[]{return false;});assert(!p && p.value.rgba.empty());
    assert(!loadPreview(s,entry,[]{return true;}));
    std::cout<<"browser preview: OK\n";
}
