#pragma once
#include "texture_processor.h"

namespace TextureDecode {
// Rejects invalid/oversized frames before allocation; output is cleared on failure.
std::string decompress(const std::vector<uint8_t>& compressed,std::vector<uint8_t>& raw);
std::string decode(const std::vector<uint8_t>& compressed,bool canvas,RgbaImage& image);
}
