#pragma once

#include <algorithm>
#include <cmath>

namespace borealis_app {
struct PreviewBounds {
    float x = 0, y = 0, width = 0, height = 0;
};

inline PreviewBounds fitPreview(float width, float height, float imageWidth, float imageHeight) {
    if (!std::isfinite(width) || !std::isfinite(height) ||
        !std::isfinite(imageWidth) || !std::isfinite(imageHeight) ||
        width <= 0 || height <= 0 || imageWidth <= 0 || imageHeight <= 0)
        return {};
    const float scale = std::min(width / imageWidth, height / imageHeight);
    const float fittedWidth = imageWidth * scale;
    const float fittedHeight = imageHeight * scale;
    return {(width - fittedWidth) / 2, (height - fittedHeight) / 2,
        fittedWidth, fittedHeight};
}
}
