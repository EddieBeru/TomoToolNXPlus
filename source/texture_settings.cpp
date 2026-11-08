#include "texture_settings.h"

namespace TextureSettings {
namespace {

using namespace TextureProcessor;

struct RowDefinition {
    int valueCount;
    bool cyclesOnActivate;
    bool (*enabled)(const State&);
    int (*valueIndex)(const State&);
    void (*setValue)(State&, int);
};

bool AlwaysEnabled(const State&) { return true; }

// Indexed by Row; A cycles only Shape and Encoder (legacy behavior).
const RowDefinition kRows[] = {
    {3, true, AlwaysEnabled,
     [](const State& s) { return static_cast<int>(s.canvasShape); },
     [](State& s, int v) { s.canvasShape = static_cast<CanvasShape>(v); }},
    {2, true, AlwaysEnabled,
     [](const State& s) { return static_cast<int>(s.encoder); },
     [](State& s, int v) { s.encoder = static_cast<Bc1Encoder>(v); }},
    {3, false, [](const State& s) { return s.encoder == Bc1Encoder::Custom; },
     [](const State& s) { return static_cast<int>(s.bc1Mode); },
     [](State& s, int v) { s.bc1Mode = static_cast<Bc1Mode>(v); }},
    {3, false, AlwaysEnabled,
     [](const State& s) { return static_cast<int>(s.fitMode); },
     [](State& s, int v) { s.fitMode = static_cast<FitMode>(v); }},
    {3, false, [](const State& s) { return s.fitMode == FitMode::Contain; },
     [](const State& s) {
         if (s.matte.a == 0) return 0;
         if (s.matte.r == 255 && s.matte.g == 255 && s.matte.b == 255) return 1;
         if (s.matte.r == 0 && s.matte.g == 0 && s.matte.b == 0) return 2;
         return 0;
     },
     [](State& s, int v) {
         if (v == 0) s.matte = {};
         else if (v == 1) s.matte = {255, 255, 255, 255};
         else s.matte = {0, 0, 0, 255};
     }},
};
static_assert(sizeof(kRows) / sizeof(kRows[0]) == RowCount,
              "Every texture-settings row needs a definition");

bool IsValid(Row row) {
    return static_cast<int>(row) >= 0 && static_cast<int>(row) < RowCount;
}

bool SamePreferences(const State& a, const State& b) {
    return a.canvasShape == b.canvasShape && a.encoder == b.encoder &&
           a.bc1Mode == b.bc1Mode && a.fitMode == b.fitMode &&
           a.matte.r == b.matte.r && a.matte.g == b.matte.g &&
           a.matte.b == b.matte.b && a.matte.a == b.matte.a;
}

void MoveFocus(State& state, int direction) {
    for (int i = static_cast<int>(state.focus) + direction;
         i >= 0 && i < RowCount; i += direction) {
        if (IsEnabled(state, static_cast<Row>(i))) {
            state.focus = static_cast<Row>(i);
            return;
        }
    }
}

} // namespace

bool IsEnabled(const State& state, Row row) {
    return IsValid(row) && kRows[static_cast<int>(row)].enabled(state);
}

int ValueCount(Row row) {
    return IsValid(row) ? kRows[static_cast<int>(row)].valueCount : 0;
}

int ValueIndex(const State& state, Row row) {
    return IsValid(row) ? kRows[static_cast<int>(row)].valueIndex(state) : -1;
}

void NormalizeFocus(State& state) {
    if (IsEnabled(state, state.focus)) return;
    if (!IsValid(state.focus)) state.focus = Row::Shape;
    if (IsEnabled(state, state.focus)) return;
    MoveFocus(state, 1);
    if (!IsEnabled(state, state.focus)) MoveFocus(state, -1);
}

void FocusRow(State& state, Row row) {
    if (IsEnabled(state, row)) state.focus = row;
}

bool SelectValue(State& state, Row row, int value) {
    if (!IsEnabled(state, row) || value < 0 || value >= ValueCount(row)) return false;
    const State before = state;
    state.focus = row;
    kRows[static_cast<int>(row)].setValue(state, value);
    NormalizeFocus(state);
    return !SamePreferences(before, state);
}

bool Apply(State& state, Action action) {
    if (action == Action::None) return false;
    NormalizeFocus(state);
    if (action == Action::FocusPrevious || action == Action::FocusNext) {
        MoveFocus(state, action == Action::FocusPrevious ? -1 : 1);
        return false;
    }
    const auto& row = kRows[static_cast<int>(state.focus)];
    if (action == Action::Activate && !row.cyclesOnActivate) return false;
    if (action != Action::PreviousValue && action != Action::NextValue &&
        action != Action::Activate) return false;
    const int delta = action == Action::PreviousValue ? -1 : 1;
    const int value = (row.valueIndex(state) + delta + row.valueCount) % row.valueCount;
    return SelectValue(state, state.focus, value);
}

} // namespace TextureSettings
