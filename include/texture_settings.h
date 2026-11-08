#pragma once

#include "texture_processor.h"

// Preference and focus transitions; no I/O.
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
// Custom matte colors use index zero for display/cycling.
int ValueIndex(const State& state, Row row);

// Repair disabled focus: prefer the next enabled row, then the previous.
void NormalizeFocus(State& state);
void FocusRow(State& state, Row row);

// True only on preference changes; focus-only changes return false.
bool SelectValue(State& state, Row row, int value);
bool Apply(State& state, Action action);

} // namespace TextureSettings
