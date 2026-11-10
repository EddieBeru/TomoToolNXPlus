// CPU decoder extracted from LivinTheDreamToolkit / TomoToolNX TextureProcessor.
#include "texture_decode.h"
#include <array>
#include <zstd.h>
#include <cstring>
namespace TextureProcessor {
struct Rgb { uint8_t r,g,b; };
struct ColorTables {
    std::array<uint8_t,256> toLinear{}, toSrgb{};
    ColorTables() {
        for(int i=0;i<256;++i) {
            double v=i/255.0;
            toLinear[i]=uint8_t(std::clamp(int(std::round((v<=0.04045?v/12.92:std::pow((v+0.055)/1.055,2.4))*255)),0,255));
            toSrgb[i]=uint8_t(std::clamp(int(std::round((v<=0.0031308?v*12.92:1.055*std::pow(v,1.0/2.4)-0.055)*255)),0,255));
        }
    }
};
static const ColorTables& tables() { static const ColorTables value; return value; }
void ConvertSrgbToLinear(std::vector<uint8_t>& rgba) {

    for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
        rgba[i]   = tables().toLinear[rgba[i]];
        rgba[i+1] = tables().toLinear[rgba[i+1]];
        rgba[i+2] = tables().toLinear[rgba[i+2]];
    }
}
void ConvertLinearToSrgb(std::vector<uint8_t>& rgba) {

    for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
        rgba[i]   = tables().toSrgb[rgba[i]];
        rgba[i+1] = tables().toSrgb[rgba[i+1]];
        rgba[i+2] = tables().toSrgb[rgba[i+2]];
    }
}
bool DetectUgctexLayoutInner(size_t bytes, UgctexLayout& layout, std::string& errOut) {
    switch (bytes) {
        case 131072: layout = {512, 512, 128, 128, 16, TextureFormat::Bc1}; return true;
        case  98304: layout = {384, 384,  96, 128, 16, TextureFormat::Bc1}; return true;
        case  65536: layout = {256, 256,  64,  64,  8, TextureFormat::Bc3}; return true;
        default:
            errOut = "Unknown ugctex size: " + std::to_string(bytes) + " bytes";
            return false;
    }
}
UgctexLayout DetectUgctexLayout(size_t bytes) {
    UgctexLayout layout{};
    std::string err;
    DetectUgctexLayoutInner(bytes, layout, err);
    return layout;
}
static int DivRoundUp(int n, int d) { return (n + d - 1) / d; }
static int GobAddress(int x, int y, int widthInGobs, int bpe, int blockHeight) {
    int xBytes = x * bpe;
    int gobAddress =
          (y / (8 * blockHeight)) * 512 * blockHeight * widthInGobs
        + (xBytes / 64) * 512 * blockHeight
        + ((y % (8 * blockHeight)) / 8) * 512;
    int xInGob = xBytes % 64;
    int yInGob = y % 8;
    return gobAddress
        + ((xInGob % 64) / 32) * 256
        + ((yInGob % 8) / 2) * 64
        + ((xInGob % 32) / 16) * 32
        + (yInGob % 2) * 16
        + (xInGob % 16);
}
std::vector<uint8_t> DeswizzleBlockLinear(const std::vector<uint8_t>& data,
                                           int width, int height,
                                           int bpe, int blockHeight) {
    if(width<=0 || height<=0 || width>1024 || height>1024 ||
       (bpe!=4 && bpe!=8 && bpe!=16) || blockHeight<=0 || blockHeight>16) return {};
    int widthInGobs  = DivRoundUp(width * bpe, 64);
    int paddedHeight = DivRoundUp(height, 8 * blockHeight) * (8 * blockHeight);
    size_t paddedSize = (size_t)widthInGobs * paddedHeight * 64;

    const uint8_t* src = data.data();
    std::vector<uint8_t> padded;
    if (data.size() < paddedSize) {
        padded.resize(paddedSize, 0);
        memcpy(padded.data(), data.data(), data.size());
        src = padded.data();
    }

    std::vector<uint8_t> output((size_t)width * height * bpe);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int sw  = GobAddress(x, y, widthInGobs, bpe, blockHeight);
            int lin = (y * width + x) * bpe;
            memcpy(output.data() + lin, src + sw, bpe);
        }
    }
    return output;
}
static Rgb Rgb565Decode(uint16_t c) {
    return {
        (uint8_t)(((c >> 11) & 0x1F) * 255 / 31),
        (uint8_t)(((c >>  5) & 0x3F) * 255 / 63),
        (uint8_t)( (c        & 0x1F) * 255 / 31)
    };
}
std::vector<uint8_t> Bc1Decode(const std::vector<uint8_t>& bd, int W, int H) {
    int bx=W/4, by=H/4;
    std::vector<uint8_t> out((size_t)W*H*4);
    uint8_t pal[16];
    for (int iy=0;iy<by;iy++) for (int ix=0;ix<bx;ix++) {
        const uint8_t* b = bd.data()+(iy*bx+ix)*8;
        uint16_t c0r,c1r; memcpy(&c0r,b,2); memcpy(&c1r,b+2,2);
        uint32_t idx; memcpy(&idx,b+4,4);
        auto [r0,g0,b0]=Rgb565Decode(c0r);
        auto [r1,g1,b1]=Rgb565Decode(c1r);
        pal[0]=r0;pal[1]=g0;pal[2]=b0;pal[3]=255;
        pal[4]=r1;pal[5]=g1;pal[6]=b1;pal[7]=255;
        if(c0r>c1r){
            pal[8]=(uint8_t)((2*r0+r1)/3);pal[9]=(uint8_t)((2*g0+g1)/3);
            pal[10]=(uint8_t)((2*b0+b1)/3);pal[11]=255;
            pal[12]=(uint8_t)((r0+2*r1)/3);pal[13]=(uint8_t)((g0+2*g1)/3);
            pal[14]=(uint8_t)((b0+2*b1)/3);pal[15]=255;
        } else {
            pal[8]=(uint8_t)((r0+r1)/2);pal[9]=(uint8_t)((g0+g1)/2);
            pal[10]=(uint8_t)((b0+b1)/2);pal[11]=255;
            pal[12]=pal[13]=pal[14]=pal[15]=0;
        }
        for(int row=0;row<4;row++) for(int col=0;col<4;col++){
            int pi=(idx>>(2*(row*4+col)))&3;
            int dst=((iy*4+row)*W+(ix*4+col))*4;
            memcpy(out.data()+dst,pal+pi*4,4);
        }
    }
    return out;
}
std::vector<uint8_t> Bc3Decode(const std::vector<uint8_t>& bd, int W, int H) {
    int bx=W/4,by=H/4;
    std::vector<uint8_t> out((size_t)W*H*4);
    uint8_t alphas[8];
    for(int iy=0;iy<by;iy++) for(int ix=0;ix<bx;ix++){
        const uint8_t* b=bd.data()+(iy*bx+ix)*16;
        uint8_t a0=b[0],a1=b[1];
        uint64_t aib=0; for(int i=0;i<6;i++) aib|=(uint64_t)b[2+i]<<(8*i);
        alphas[0]=a0;alphas[1]=a1;
        if(a0>a1){
            alphas[2]=(uint8_t)((6*a0+a1)/7);alphas[3]=(uint8_t)((5*a0+2*a1)/7);
            alphas[4]=(uint8_t)((4*a0+3*a1)/7);alphas[5]=(uint8_t)((3*a0+4*a1)/7);
            alphas[6]=(uint8_t)((2*a0+5*a1)/7);alphas[7]=(uint8_t)((a0+6*a1)/7);
        } else {
            alphas[2]=(uint8_t)((4*a0+a1)/5);alphas[3]=(uint8_t)((3*a0+2*a1)/5);
            alphas[4]=(uint8_t)((2*a0+3*a1)/5);alphas[5]=(uint8_t)((a0+4*a1)/5);
            alphas[6]=0;alphas[7]=255;
        }
        uint16_t c0r,c1r; memcpy(&c0r,b+8,2); memcpy(&c1r,b+10,2);
        uint32_t ci; memcpy(&ci,b+12,4);
        auto [r0,g0,b0]=Rgb565Decode(c0r); auto [r1,g1,b1]=Rgb565Decode(c1r);
        uint8_t pr[4]={r0,r1,(uint8_t)((2*r0+r1)/3),(uint8_t)((r0+2*r1)/3)};
        uint8_t pg[4]={g0,g1,(uint8_t)((2*g0+g1)/3),(uint8_t)((g0+2*g1)/3)};
        uint8_t pb[4]={b0,b1,(uint8_t)((2*b0+b1)/3),(uint8_t)((b0+2*b1)/3)};
        for(int row=0;row<4;row++) for(int col=0;col<4;col++){
            int pi=row*4+col;
            int dst=((iy*4+row)*W+(ix*4+col))*4;
            out[dst]=pr[(ci>>(2*pi))&3];out[dst+1]=pg[(ci>>(2*pi))&3];
            out[dst+2]=pb[(ci>>(2*pi))&3];out[dst+3]=alphas[(aib>>(3*pi))&7];
        }
    }
    return out;
}
std::string DecodeCanvas(const std::vector<uint8_t>& raw, RgbaImage& out, bool noSrgb) {
    out={};
    if(raw.empty() || raw.size()%4 || raw.size()>4*1024*1024) return "Invalid canvas size";
    int totalPx = (int)(raw.size() / 4);
    int side = (int)std::sqrt((double)totalPx);
    int w = (side*side == totalPx) ? side : 256;
    int h = totalPx / w;
    if(w<1 || w>1024 || h<1 || h>1024) return "Canvas dimensions exceed limit";
    if ((size_t)(w*h*4) != raw.size())
        return "Canvas: unexpected size " + std::to_string(raw.size());

    auto pixels = DeswizzleBlockLinear(raw, w, h, 4, DefaultBlockHeight);
    if (!noSrgb) ConvertLinearToSrgb(pixels);

    out.width  = w;
    out.height = h;
    out.pixels = std::move(pixels);
    return "";
}
std::string DecodeUgctex(const std::vector<uint8_t>& raw, RgbaImage& out, bool noSrgb) {
    UgctexLayout layout{};
    std::string err;
    if (!DetectUgctexLayoutInner(raw.size(), layout, err)) return err;

    int vbw = layout.Width  / 4;
    int vbh = layout.Height / 4;
    auto blocks = DeswizzleBlockLinear(raw, vbw, vbh, layout.BytesPerBlock(), layout.BlockHeight);

    std::vector<uint8_t> rgba = (layout.Format == TextureFormat::Bc3)
        ? Bc3Decode(blocks, layout.Width, layout.Height)
        : Bc1Decode(blocks, layout.Width, layout.Height);

    if (!noSrgb) ConvertLinearToSrgb(rgba);

    out.width  = layout.Width;
    out.height = layout.Height;
    out.pixels = std::move(rgba);
    return "";
}
std::string DecodeThumb(const std::vector<uint8_t>& raw, RgbaImage& out, bool noSrgb) {
    if(raw.empty() || raw.size()%16 || raw.size()>4*1024*1024) return "Invalid thumbnail size";
    int totalBlocks = (int)(raw.size() / 16);
    int gs = (int)std::sqrt((double)totalBlocks);
    if (gs*gs != totalBlocks)
        return "Thumb: not square BC3 (" + std::to_string(raw.size()) + " bytes)";

    int tw = gs*4, th = gs*4;
    auto blocks = DeswizzleBlockLinear(raw, gs, gs, 16, ThumbBlockHeight);
    auto rgba   = Bc3Decode(blocks, tw, th);
    if (!noSrgb) ConvertLinearToSrgb(rgba);

    out.width  = tw;
    out.height = th;
    out.pixels = std::move(rgba);
    return "";
}
}

namespace TextureDecode {
std::string decompress(const std::vector<uint8_t>& compressed,std::vector<uint8_t>& raw) {
    raw.clear();
    constexpr size_t limit=4*1024*1024;
    if(compressed.empty() || compressed.size()>limit) return "Invalid compressed size";
    const auto announced=ZSTD_getFrameContentSize(compressed.data(),compressed.size());
    if(announced==ZSTD_CONTENTSIZE_ERROR ||
       (announced!=ZSTD_CONTENTSIZE_UNKNOWN && announced>limit)) return "Invalid frame size";
    const size_t frameSize=ZSTD_findFrameCompressedSize(compressed.data(),compressed.size());
    if(ZSTD_isError(frameSize) || frameSize!=compressed.size()) return "Incomplete or multiple frames";
    ZSTD_DCtx* ctx=ZSTD_createDCtx();
    if(!ctx) return "Cannot allocate decoder";
    size_t setup=ZSTD_DCtx_setParameter(ctx,ZSTD_d_windowLogMax,23);
    if(ZSTD_isError(setup)) {ZSTD_freeDCtx(ctx);return "Cannot limit decoder";}
    std::vector<uint8_t> out(limit);
    ZSTD_inBuffer input{compressed.data(),compressed.size(),0};
    ZSTD_outBuffer output{out.data(),out.size(),0};
    size_t remaining=1;
    while(remaining && input.pos<input.size && output.pos<output.size) {
        size_t before=input.pos;
        remaining=ZSTD_decompressStream(ctx,&output,&input);
        if(ZSTD_isError(remaining) || (remaining && input.pos==before))break;
    }
    ZSTD_freeDCtx(ctx);
    if(remaining || input.pos!=input.size) return "Invalid compressed image";
    out.resize(output.pos); raw=std::move(out); return {};
}
std::string decode(const std::vector<uint8_t>& compressed,bool canvas,RgbaImage& image) {
    image={};
    std::vector<uint8_t> raw;
    auto err=decompress(compressed,raw);
    if(!err.empty()) return err;
    return canvas?TextureProcessor::DecodeCanvas(raw,image):TextureProcessor::DecodeUgctex(raw,image);
}
}
