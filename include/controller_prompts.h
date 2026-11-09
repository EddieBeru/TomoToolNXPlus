#pragma once

#include <SDL2/SDL.h>

namespace ControllerPrompts {

enum class Icon { A, B, X, Y, ZL, ZR, Minus, Plus, Dpad, LeftStick };

void Init(SDL_Renderer* renderer);
void Quit();
bool Available(Icon icon);
void Draw(Icon icon, int cx, int cy, int size, SDL_Color color);

} // namespace ControllerPrompts
