#include "texture_browser.h"
#include "texture_decode.h"
namespace TextureBrowser {
Result<Preview> loadPreview(ReadSource& source,const Entry& entry,const Cancel& cancel) {
    auto opened=source.open(entry.id.account,cancel);if(!opened)return {{},opened};
    auto files=source.enumerate(cancel);if(!files)return {{},files.status};
    bool fallback=false;
    RgbaImage image;
    if(!entry.canvasRef.empty()) {
        auto bytes=source.read(entry.canvasRef,MaxImageBytes,cancel);
        auto error=bytes?TextureDecode::decode(bytes.value,true,image):"Cannot read canvas";
        if(cancel())return {{},{Error::Cancelled}};
        if(error.empty() && image.width!=image.height)
            return {{image.width,image.height,std::move(image.pixels),true,false},{}};
        fallback=!error.empty();image={};
    }
    auto bytes=source.read(entry.mainRef,MaxImageBytes,cancel);
    if(!bytes)return {{},bytes.status};
    auto error=TextureDecode::decode(bytes.value,false,image);
    if(cancel())return {{},{Error::Cancelled}};
    if(!error.empty())return {{},{Error::Preview}};
    return {{image.width,image.height,std::move(image.pixels),false,fallback},{}};
}
}
