#pragma once

#include "texture_processor.h"

// Pure preference and focus transitions. Rendering, controller decoding, config
// persistence, and game-save operations belong to the caller.
namespace TextureSettings {

enum class Row { Shape, Encoder, Bc1Mode, Fit, Background, Count };
constexpr int RowCount = static_cast<int>(Row::Count);

enum class Action { None, FocusPrevious, FocusNext, PreviousValue, NextValue, Activate };

struct State {
    TextureProcessor::CanvasShape canvasShape = TextureProcessor::CanvasShape::Square;
    TextureProcessor::Bc1Encoder encoder = TextureProcessor::Bc1Encoder::Custom;
    TextureProcessor::Bc1Mode bc1Mode = TextureProcessor::Bc1Mode::Auto;
    TextureProcessor::FitMode fitMode = TextureProcessor::FitMode::Cover;
    TextureProcessor::Matte matte = {};
    Row focus = Row::Shape;
};

bool IsEnabled(const State& state, Row row);
int ValueCount(Row row);
// Custom matte colors retain the legacy display/cycling fallback to index zero.
int ValueIndex(const State& state, Row row);

// Repair unavailable focus after loading or externally changing preferences.
// Prefer the next enabled row, falling back to the previous one.
void NormalizeFocus(State& state);
void FocusRow(State& state, Row row);

// Return true only when preferences actually change, so the caller can persist
// once. Focus-only changes and invalid/disabled selections return false.
bool SelectValue(State& state, Row row, int value);
bool Apply(State& state, Action action);

} // namespace TextureSettings
