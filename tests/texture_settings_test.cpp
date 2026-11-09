// Host checks; see tests/README.md.
#include "texture_settings.h"

#include <cstdio>
#include <cstdlib>

using namespace TextureSettings;
using namespace TextureProcessor;
using FocusNavigation::Command;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "line %d: %s\n", __LINE__, #condition); \
        std::exit(1); \
    } \
} while (false)

static bool SamePreferences(const State& a, const State& b) {
    return a.canvasShape == b.canvasShape && a.encoder == b.encoder &&
           a.bc1Mode == b.bc1Mode && a.fitMode == b.fitMode &&
           a.matte.r == b.matte.r && a.matte.g == b.matte.g &&
           a.matte.b == b.matte.b && a.matte.a == b.matte.a;
}

static Outcome Send(State& state, Command command) {
    state.input.Reset();
    return Apply(state, command);
}

static bool Preview(State& state, Row row, int value) {
    state.input.Reset();
    return PreviewValue(state, row, value);
}

static void CheckSharedNavigation() {
    using namespace FocusNavigation;
    Path path;
    path.Focus(7);
    path.Enter(2);
    path.Enter(4);
    CHECK(path.Depth() == 3 && path.Root() == 7 && path.Current() == 4);
    CHECK(path.Back() && path.Current() == 2);
    CHECK(path.Back() && path.Current() == 7);
    CHECK(!path.Back());
    Frame frame;
    CHECK(frame.Consume() && !frame.Consume());
    frame.Reset();
    CHECK(frame.Consume());
    Input input;
    input.back = input.activate = input.right = true;
    CHECK(Resolve(input, Axis::Horizontal) == Command::Back);
    input.back = false;
    CHECK(Resolve(input, Axis::Horizontal) == Command::Activate);
    input.activate = false;
    input.up = true;
    CHECK(Resolve(input, Axis::Vertical) == Command::Up);
    CHECK(Resolve(input, Axis::Horizontal) == Command::Right);
    input.left = true;
    CHECK(Resolve(input, Axis::Horizontal) == Command::None);
    CHECK(NextIndex(0, -1, 3, true) == 2);
    CHECK(NextIndex(0, -1, 3, false) == 0);
    CHECK(NextIndex(0, 1, 3, false, [](int) { return false; }) == -1);
    CHECK(NextIndex(0, 1, 0, false) == -1);
}

static void CheckNavigation() {
    for (auto encoder : {Bc1Encoder::Custom, Bc1Encoder::PCA}) {
        for (auto fit : {FitMode::Cover, FitMode::Contain, FitMode::Fill}) {
            State s;
            s.encoder = encoder;
            s.fitMode = fit;
            const State before = s;
            CHECK(!Send(s, Command::Up).preferencesChanged);
            CHECK(FocusedRow(s) == Row::Shape);
            for (int i = 0; i < RowCount; ++i) {
                const Row row = static_cast<Row>(i);
                if (!IsEnabled(s, row)) continue;
                CHECK(FocusedRow(s) == row);
                CHECK(!Send(s, Command::Down).preferencesChanged);
                CHECK(IsEnabled(s, FocusedRow(s)));
            }
            const Row last = FocusedRow(s);
            CHECK(last == (fit == FitMode::Contain ? Row::Background : Row::Fit));
            Send(s, Command::Down);
            CHECK(FocusedRow(s) == last);
            for (int i = RowCount - 1; i >= 0; --i) {
                const Row row = static_cast<Row>(i);
                if (!IsEnabled(s, row)) continue;
                CHECK(FocusedRow(s) == row);
                Send(s, Command::Up);
            }
            CHECK(FocusedRow(s) == Row::Shape && SamePreferences(s, before));
            Send(s, Command::Left);
            Send(s, Command::Right);
            CHECK(!IsEditing(s) && SamePreferences(s, before));
        }
    }
}

static void CheckOptionScopes() {
    for (int i = 0; i < RowCount; ++i) {
        State s;
        s.fitMode = FitMode::Contain;
        const Row row = static_cast<Row>(i);
        FocusRow(s, row);
        const State before = s;
        CHECK(!Send(s, Command::Activate).preferencesChanged);
        CHECK(IsEditing(s) && FocusedRow(s) == row);
        CHECK(FocusedOption(s) == ValueIndex(before, row));
        Send(s, Command::Right);
        CHECK(FocusedOption(s) == (ValueIndex(before, row) + 1) % ValueCount(row));
        Send(s, Command::Up);
        Send(s, Command::Down);
        CHECK(FocusedRow(s) == row && SamePreferences(s, before));
        CHECK(!Send(s, Command::Back).leaveParent);
        CHECK(!IsEditing(s) && FocusedRow(s) == row && SamePreferences(s, before));
        Send(s, Command::Activate);
        CHECK(!Send(s, Command::Activate).preferencesChanged);
        CHECK(!IsEditing(s) && SamePreferences(s, before));
        Send(s, Command::Activate);
        const int expected = (ValueIndex(before, row) + ValueCount(row) - 1) % ValueCount(row);
        Send(s, Command::Left);
        CHECK(SamePreferences(s, before));
        CHECK(Send(s, Command::Activate).preferencesChanged);
        CHECK(ValueIndex(s, row) == expected && !IsEditing(s));
        CHECK(FocusedRow(s) == row && Send(s, Command::Back).leaveParent);
    }
}

static void CheckTouchAndConsumption() {
    State s;
    s.bc1Mode = Bc1Mode::ThreeColor;
    CHECK(Preview(s, Row::Encoder, 1));
    CHECK(IsEditing(s) && FocusedRow(s) == Row::Encoder && FocusedOption(s) == 1);
    CHECK(s.encoder == Bc1Encoder::Custom && IsEnabled(s, Row::Bc1Mode));
    CHECK(!Apply(s, Command::Activate).preferencesChanged);
    CHECK(IsEditing(s) && s.encoder == Bc1Encoder::Custom);
    CHECK(!Preview(s, Row::Fit, 1));
    CHECK(IsEditing(s) && FocusedRow(s) == Row::Encoder);
    CHECK(Send(s, Command::Activate).preferencesChanged);
    CHECK(s.encoder == Bc1Encoder::PCA && !IsEnabled(s, Row::Bc1Mode));
    CHECK(s.bc1Mode == Bc1Mode::ThreeColor);
    CHECK(!Preview(s, Row::Bc1Mode, 0));
    CHECK(FocusedRow(s) == Row::Encoder && !IsEditing(s));
    CHECK(Preview(s, Row::Fit, 1));
    CHECK(Send(s, Command::Activate).preferencesChanged);
    CHECK(IsEnabled(s, Row::Background));
    CHECK(Preview(s, Row::Background, 2));
    CHECK(Send(s, Command::Activate).preferencesChanged);
    CHECK(s.matte.a == 255 && s.matte.r == 0);
    CHECK(Preview(s, Row::Fit, 0));
    CHECK(!Send(s, Command::Back).leaveParent);
    CHECK(s.fitMode == FitMode::Contain);
    CHECK(Preview(s, Row::Fit, 0));
    CHECK(Send(s, Command::Activate).preferencesChanged);
    CHECK(!IsEnabled(s, Row::Background) && s.matte.a == 255 && s.matte.r == 0);
    CHECK(!Preview(s, Row::Background, 0));
    s.input.Reset();
    CHECK(!Apply(s, Command::Activate).preferencesChanged);
    CHECK(IsEditing(s));
    CHECK(!Apply(s, Command::Activate).preferencesChanged);
    CHECK(IsEditing(s));
    Open(s);
    CHECK(!IsEditing(s) && FocusedRow(s) == Row::Fit);
}

static void CheckRepairAndInvalidInput() {
    State s;
    s.fitMode = FitMode::Contain;
    FocusRow(s, Row::Background);
    Send(s, Command::Activate);
    s.fitMode = FitMode::Cover;
    NormalizeFocus(s);
    CHECK(!IsEditing(s) && FocusedRow(s) == Row::Fit);
    FocusRow(s, Row::Bc1Mode);
    Send(s, Command::Activate);
    s.encoder = Bc1Encoder::PCA;
    NormalizeFocus(s);
    CHECK(!IsEditing(s) && FocusedRow(s) == Row::Fit);
    const State before = s;
    for (Row row : {static_cast<Row>(-1), Row::Count, static_cast<Row>(99)}) {
        CHECK(!IsEnabled(s, row));
        CHECK(ValueCount(row) == 0 && ValueIndex(s, row) == -1);
        CHECK(!Preview(s, row, 0));
        CHECK(SamePreferences(s, before));
        s.navigation.Reset(static_cast<int>(row));
        NormalizeFocus(s);
        CHECK(FocusedRow(s) == Row::Shape);
    }
    CHECK(!Preview(s, Row::Shape, -1));
    CHECK(!Preview(s, Row::Shape, ValueCount(Row::Shape)));
    CHECK(SamePreferences(s, before));
    s.input.Reset();
    CHECK(!Apply(s, Command::None).preferencesChanged && !s.input.Consumed());
    s.fitMode = FitMode::Contain;
    s.matte = {12, 34, 56, 255};
    const auto custom = s;
    CHECK(ValueIndex(s, Row::Background) == -1);
    CHECK(Preview(s, Row::Background, 0));
    Send(s, Command::Back);
    CHECK(SamePreferences(s, custom));
    CHECK(Preview(s, Row::Background, 0));
    CHECK(Send(s, Command::Activate).preferencesChanged && s.matte.a == 0);
}

int main() {
    CheckSharedNavigation();
    CheckNavigation();
    CheckOptionScopes();
    CheckTouchAndConsumption();
    CheckRepairAndInvalidInput();
    std::puts("Focus navigation and texture settings checks passed.");
}
