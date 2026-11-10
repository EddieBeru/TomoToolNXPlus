#include "texture_decode.h"
#include <zstd.h>
#include <cassert>
#include <iostream>
int main() {
    for (size_t bytes : {131072u,98304u,65536u}) {
        RgbaImage img;
        assert(TextureProcessor::DecodeUgctex(std::vector<uint8_t>(bytes,0),img).empty());
        int side=bytes==131072?512:bytes==98304?384:256;
        assert(img.width==side && img.height==side && img.pixels.size()==size_t(side*side*4));
        for(size_t i=0;i<img.pixels.size();i+=4) {
            assert(img.pixels[i]==0 && img.pixels[i+1]==0 && img.pixels[i+2]==0);
            assert(img.pixels[i+3]==(bytes==65536?0:255));
        }
    }
    for(int h:{128,256,512}) {
        RgbaImage img;
        assert(TextureProcessor::DecodeCanvas(std::vector<uint8_t>(256*h*4,255),img).empty());
        assert(img.width==256 && img.height==h);
        for(auto p:img.pixels) assert(p==255);
    }
    const std::vector<std::pair<size_t,uint64_t>> goldens={
        {131072,7412620954163284142ULL},{98304,3462617646103785179ULL},
        {65536,15638971284140473110ULL},{524288,16984542086128403800ULL},
        {262144,12556033361690192620ULL}};
    for(const auto& golden:goldens){
        std::vector<uint8_t> raw(golden.first);
        for(size_t i=0;i<raw.size();++i)raw[i]=uint8_t(i*17+i/11);
        RgbaImage image;
        auto error=raw.size()>131072?TextureProcessor::DecodeCanvas(raw,image):TextureProcessor::DecodeUgctex(raw,image);
        assert(error.empty());uint64_t hash=1469598103934665603ULL;
        for(auto byte:image.pixels){hash^=byte;hash*=1099511628211ULL;}
        assert(hash==golden.second);
    }
    std::vector<uint8_t> rgba={128,64,0,17};
    TextureProcessor::ConvertLinearToSrgb(rgba);
    assert(rgba==std::vector<uint8_t>({188,137,0,17}));

    for(size_t bytes:{0u,1u,2u,3u,7u,4194308u}) {
        RgbaImage img;
        assert(!TextureProcessor::DecodeCanvas(std::vector<uint8_t>(bytes),img).empty());
    }
    std::vector<uint8_t> raw(256*128*4,255), compressed(ZSTD_compressBound(raw.size())), decoded;
    compressed.resize(ZSTD_compress(compressed.data(),compressed.size(),raw.data(),raw.size(),3));
    assert(TextureDecode::decompress(compressed,decoded).empty() && decoded==raw);
    ZSTD_CCtx* ctx=ZSTD_createCCtx();assert(ctx);
    assert(!ZSTD_isError(ZSTD_CCtx_setParameter(ctx,ZSTD_c_contentSizeFlag,0)));
    std::vector<uint8_t> unknown(ZSTD_compressBound(raw.size()));
    unknown.resize(ZSTD_compress2(ctx,unknown.data(),unknown.size(),raw.data(),raw.size()));
    ZSTD_freeCCtx(ctx);
    assert(TextureDecode::decompress(unknown,decoded).empty() && decoded==raw);
    std::vector<uint8_t> oversized(4*1024*1024+1,0);
    std::vector<uint8_t> oversizedFrame(ZSTD_compressBound(oversized.size()));
    oversizedFrame.resize(ZSTD_compress(oversizedFrame.data(),oversizedFrame.size(),oversized.data(),oversized.size(),3));
    assert(!TextureDecode::decompress(oversizedFrame,decoded).empty() && decoded.empty());
    auto broken=compressed; broken.pop_back();
    assert(!TextureDecode::decompress(broken,decoded).empty() && decoded.empty());
    broken=compressed; broken.insert(broken.end(),compressed.begin(),compressed.end());
    assert(!TextureDecode::decompress(broken,decoded).empty());
    assert(!TextureDecode::decompress({},decoded).empty());
    RgbaImage image;
    assert(TextureDecode::decode(compressed,true,image).empty() && image.height==128);
    std::cout<<"texture decode: OK\n";
}
