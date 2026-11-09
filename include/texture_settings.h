#pragma once

#include "texture_processor.h"
#include "focus_navigation.h"

// Preference and focus transitions; no I/O.
namespace TextureSettings {

enum class Row { Shape, Encoder, Bc1Mode, Fit, Background, Count };
constexpr int RowCount = static_cast<int>(Row::Count);

struct State {
    TextureProcessor::CanvasShape canvasShape = TextureProcessor::CanvasShape::Square;
    TextureProcessor::Bc1Encoder encoder = TextureProcessor::Bc1Encoder::Custom;
    TextureProcessor::Bc1Mode bc1Mode = TextureProcessor::Bc1Mode::Auto;
    TextureProcessor::FitMode fitMode = TextureProcessor::FitMode::Cover;
    TextureProcessor::Matte matte = {};
    FocusNavigation::Path navigation;
    FocusNavigation::Frame input;
};

struct Outcome {
    bool preferencesChanged = false;
    bool leaveParent = false;
};

Row FocusedRow(const State& state);
bool IsEditing(const State& state);
int FocusedOption(const State& state);
bool IsEnabled(const State& state, Row row);
int ValueCount(Row row);
// Custom matte colors have no preset index (-1).
int ValueIndex(const State& state, Row row);

// Repair disabled focus: prefer the next enabled row, then the previous.
void NormalizeFocus(State& state);
void FocusRow(State& state, Row row);
void Open(State& state);

bool EnterRow(State& state, Row row);
bool PreviewValue(State& state, Row row, int value);
Outcome Apply(State& state, FocusNavigation::Command command);

} // namespace TextureSettings
