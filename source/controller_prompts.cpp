#include "controller_prompts.h"

#include <SDL2/SDL_image.h>
#include <array>

namespace ControllerPrompts {
namespace {

constexpr std::array<const char*, 10> kPaths = {{
    "romfs:/ui/controller/a.png",
    "romfs:/ui/controller/b.png",
    "romfs:/ui/controller/x.png",
    "romfs:/ui/controller/y.png",
    "romfs:/ui/controller/zl.png",
    "romfs:/ui/controller/zr.png",
    "romfs:/ui/controller/minus.png",
    "romfs:/ui/controller/plus.png",
    "romfs:/ui/controller/dpad.png",
    "romfs:/ui/controller/left-stick.png",
}};

std::array<SDL_Texture*, kPaths.size()> gTextures{};
SDL_Renderer* gRenderer = nullptr;

} // namespace

void Init(SDL_Renderer* renderer) {
    gRenderer = renderer;
    for (size_t i = 0; i < kPaths.size(); ++i) {
        SDL_Surface* surface = IMG_Load(kPaths[i]);
        if (!surface) continue;
        gTextures[i] = SDL_CreateTextureFromSurface(gRenderer, surface);
        SDL_FreeSurface(surface);
        if (gTextures[i]) SDL_SetTextureBlendMode(gTextures[i], SDL_BLENDMODE_BLEND);
    }
}

void Quit() {
    for (SDL_Texture*& texture : gTextures) {
        if (texture) SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    gRenderer = nullptr;
}

bool Available(Icon icon) {
    const size_t index = static_cast<size_t>(icon);
    return index < gTextures.size() && gTextures[index] != nullptr;
}

void Draw(Icon icon, int cx, int cy, int size, SDL_Color color) {
    if (!gRenderer || size <= 0 || !Available(icon)) return;
    SDL_Texture* texture = gTextures[static_cast<size_t>(icon)];
    SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(texture, color.a);
    SDL_Rect dst{cx - size / 2, cy - size / 2, size, size};
    SDL_RenderCopy(gRenderer, texture, nullptr, &dst);
}

} // namespace ControllerPrompts
