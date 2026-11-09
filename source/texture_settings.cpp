#include "texture_settings.h"

namespace TextureSettings {
namespace {

using namespace TextureProcessor;

struct RowDefinition {
    int valueCount;
    bool (*enabled)(const State&);
    int (*valueIndex)(const State&);
    void (*setValue)(State&, int);
};

bool AlwaysEnabled(const State&) { return true; }

// Indexed by Row.
const RowDefinition kRows[] = {
    {3, AlwaysEnabled,
     [](const State& s) { return static_cast<int>(s.canvasShape); },
     [](State& s, int v) { s.canvasShape = static_cast<CanvasShape>(v); }},
    {2, AlwaysEnabled,
     [](const State& s) { return static_cast<int>(s.encoder); },
     [](State& s, int v) { s.encoder = static_cast<Bc1Encoder>(v); }},
    {3, [](const State& s) { return s.encoder == Bc1Encoder::Custom; },
     [](const State& s) { return static_cast<int>(s.bc1Mode); },
     [](State& s, int v) { s.bc1Mode = static_cast<Bc1Mode>(v); }},
    {3, AlwaysEnabled,
     [](const State& s) { return static_cast<int>(s.fitMode); },
     [](State& s, int v) { s.fitMode = static_cast<FitMode>(v); }},
    {3, [](const State& s) { return s.fitMode == FitMode::Contain; },
     [](const State& s) {
         if (s.matte.a == 0) return 0;
         if (s.matte.r == 255 && s.matte.g == 255 && s.matte.b == 255) return 1;
         if (s.matte.r == 0 && s.matte.g == 0 && s.matte.b == 0) return 2;
         return -1;
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

bool ValidPreferences(const State& state) {
    for (int i = 0; i < RowCount - 1; ++i) {
        const auto row = static_cast<Row>(i);
        const int value = ValueIndex(state, row);
        if (value < 0 || value >= ValueCount(row)) return false;
    }
    return state.matte.a == 0 || state.matte.a == 255;
}

bool SelectValue(State& state, Row row, int value) {
    if (!ValidPreferences(state) || !IsEnabled(state, row) ||
        value < 0 || value >= ValueCount(row)) return false;
    const State before = state;
    kRows[static_cast<int>(row)].setValue(state, value);
    NormalizeFocus(state);
    return !SamePreferences(before, state);
}

Row FocusedRow(const State& state) {
    return static_cast<Row>(state.navigation.Root());
}

bool IsEditing(const State& state) {
    return state.navigation.Depth() > 1;
}

int FocusedOption(const State& state) {
    return IsEditing(state) ? state.navigation.Current() : -1;
}

void Open(State& state) {
    state.navigation.Reset(state.navigation.Root());
    NormalizeFocus(state);
}

void NormalizeFocus(State& state) {
    const Row row = FocusedRow(state);
    if (!IsEnabled(state, row)) {
        const int current = static_cast<int>(row);
        const auto enabled = [&](int index) { return IsEnabled(state, static_cast<Row>(index)); };
        int next = FocusNavigation::NextIndex(current, 1, RowCount, false, enabled);
        if (next < 0) next = FocusNavigation::NextIndex(current, -1, RowCount, false, enabled);
        state.navigation.Reset(next);
    } else if (IsEditing(state) &&
               (FocusedOption(state) < 0 || FocusedOption(state) >= ValueCount(row))) {
        state.navigation.Focus(std::max(0, ValueIndex(state, row)));
    }
}

void FocusRow(State& state, Row row) {
    if (!IsEditing(state) && IsEnabled(state, row)) state.navigation.Focus(static_cast<int>(row));
}

bool EnterRow(State& state, Row row) {
    if (IsEditing(state) || !IsEnabled(state, row) || !state.input.Consume()) return false;
    state.navigation.Focus(static_cast<int>(row));
    state.navigation.Enter(std::max(0, ValueIndex(state, row)));
    return true;
}

bool PreviewValue(State& state, Row row, int value) {
    if (!IsEnabled(state, row) || value < 0 || value >= ValueCount(row)) return false;
    if (IsEditing(state) && FocusedRow(state) != row) return false;
    if (!state.input.Consume()) return false;
    if (!IsEditing(state)) {
        state.navigation.Focus(static_cast<int>(row));
        state.navigation.Enter(value);
    } else state.navigation.Focus(value);
    return true;
}

Outcome Apply(State& state, FocusNavigation::Command command) {
    using FocusNavigation::Command;
    Outcome result;
    if (command == Command::None || state.input.Consumed()) return result;
    NormalizeFocus(state);
    const Row row = FocusedRow(state);
    if (command == Command::Back) {
        state.input.Consume();
        result.leaveParent = !state.navigation.Back();
    } else if (command == Command::Activate) {
        if (!IsEditing(state)) { EnterRow(state, row); return result; }
        state.input.Consume();
        result.preferencesChanged = SelectValue(state, row, FocusedOption(state));
        state.navigation.Back();
        NormalizeFocus(state);
    } else if (!IsEditing(state) && (command == Command::Up || command == Command::Down)) {
        state.input.Consume();
        const int next = FocusNavigation::NextIndex(static_cast<int>(row),
            command == Command::Up ? -1 : 1, RowCount, false,
            [&](int index) { return IsEnabled(state, static_cast<Row>(index)); });
        state.navigation.Focus(next);
    } else if (IsEditing(state) && (command == Command::Left || command == Command::Right)) {
        state.input.Consume();
        state.navigation.Focus(FocusNavigation::NextIndex(FocusedOption(state),
            command == Command::Left ? -1 : 1, ValueCount(row), true));
    }
    return result;
}

} // namespace TextureSettings
