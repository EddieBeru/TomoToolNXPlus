// Host-only regression checks; no SDL, libnx, filesystem, or real saves required.
// Build/run instructions: see tests/README.md.
#include "texture_settings.h"

#include <cstdio>
#include <cstdlib>

using namespace TextureSettings;
using namespace TextureProcessor;

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

static void CheckNavigation() {
    // Every dependency combination must keep focus valid, stop at boundaries,
    // and traverse the same enabled rows forward and backward.
    for (auto encoder : {Bc1Encoder::Custom, Bc1Encoder::PCA}) {
        for (auto fit : {FitMode::Cover, FitMode::Contain, FitMode::Fill}) {
            State s;
            s.encoder = encoder;
            s.fitMode = fit;
            const State before = s;
            CHECK(!Apply(s, Action::FocusPrevious));
            CHECK(s.focus == Row::Shape);
            for (int i = 0; i < RowCount; ++i) {
                const Row target = static_cast<Row>(i);
                if (!IsEnabled(s, target)) continue;
                CHECK(s.focus == target);
                CHECK(!Apply(s, Action::FocusNext));
                CHECK(IsEnabled(s, s.focus));
            }
            const Row last = s.focus;
            CHECK(last == (fit == FitMode::Contain ? Row::Background : Row::Fit));
            CHECK(!Apply(s, Action::FocusNext));
            CHECK(s.focus == last);
            for (int i = RowCount - 1; i >= 0; --i) {
                const Row target = static_cast<Row>(i);
                if (!IsEnabled(s, target)) continue;
                CHECK(s.focus == target);
                CHECK(!Apply(s, Action::FocusPrevious));
            }
            CHECK(s.focus == Row::Shape);
            CHECK(SamePreferences(s, before));
        }
    }
}

static void CheckDependencyChangesAndTouch() {
    State s;
    s.bc1Mode = Bc1Mode::ThreeColor;
    FocusRow(s, Row::Bc1Mode);
    s.encoder = Bc1Encoder::PCA;
    NormalizeFocus(s);
    CHECK(s.focus == Row::Fit);
    CHECK(s.bc1Mode == Bc1Mode::ThreeColor);

    s.fitMode = FitMode::Contain;
    s.matte = {255, 255, 255, 255};
    FocusRow(s, Row::Background);
    s.fitMode = FitMode::Fill;
    NormalizeFocus(s);
    CHECK(s.focus == Row::Fit);
    CHECK(s.matte.a == 255 && s.matte.r == 255);

    const State before = s;
    FocusRow(s, Row::Background);
    FocusRow(s, Row::Bc1Mode);
    CHECK(!SelectValue(s, Row::Background, 0));
    CHECK(!SelectValue(s, Row::Bc1Mode, 0));
    CHECK(s.focus == before.focus && SamePreferences(s, before));

    CHECK(SelectValue(s, Row::Encoder, 0));
    CHECK(s.focus == Row::Encoder && IsEnabled(s, Row::Bc1Mode));
    CHECK(s.bc1Mode == Bc1Mode::ThreeColor);
    CHECK(SelectValue(s, Row::Fit, 1));
    CHECK(s.focus == Row::Fit && IsEnabled(s, Row::Background));
    CHECK(SelectValue(s, Row::Background, 2));
    CHECK(s.focus == Row::Background && s.matte.a == 255 && s.matte.r == 0);
    CHECK(SelectValue(s, Row::Fit, 0));
    CHECK(s.focus == Row::Fit && !IsEnabled(s, Row::Background));
    CHECK(s.matte.a == 255 && s.matte.r == 0);

    FocusRow(s, Row::Encoder);
    CHECK(!SelectValue(s, Row::Shape, 0)); // same value still updates touch focus
    CHECK(s.focus == Row::Shape);
}

static void CheckValuesAndIdle() {
    for (int i = 0; i < RowCount; ++i) {
        State s;
        s.fitMode = FitMode::Contain;
        const Row row = static_cast<Row>(i);
        FocusRow(s, row);
        const State before = s;
        CHECK(!Apply(s, Action::None));
        CHECK(s.focus == row && SamePreferences(s, before));
        const bool cyclesOnA = row == Row::Shape || row == Row::Encoder;
        CHECK(Apply(s, Action::Activate) == cyclesOnA);
        CHECK(ValueIndex(s, row) == (cyclesOnA ? 1 : ValueIndex(before, row)));
        s = before;
        const int original = ValueIndex(s, row);
        CHECK(Apply(s, Action::PreviousValue));
        CHECK(ValueIndex(s, row) == (original + ValueCount(row) - 1) % ValueCount(row));
        CHECK(Apply(s, Action::NextValue));
        CHECK(SamePreferences(s, before));
        for (int n = 0; n < ValueCount(row); ++n) CHECK(Apply(s, Action::NextValue));
        CHECK(SamePreferences(s, before));
        CHECK(!SelectValue(s, row, -1));
        CHECK(!SelectValue(s, row, ValueCount(row)));
        CHECK(SamePreferences(s, before));
    }

    State s;
    s.fitMode = FitMode::Contain;
    s.matte = {12, 34, 56, 255};
    CHECK(ValueIndex(s, Row::Background) == 0);
    CHECK(SelectValue(s, Row::Background, 0)); // fallback index isn't transparent
    CHECK(s.matte.a == 0);
    CHECK(!SelectValue(s, Row::Background, 0));
    CHECK(Apply(s, Action::NextValue));
    CHECK(s.matte.r == 255 && s.matte.g == 255 && s.matte.b == 255 && s.matte.a == 255);
    CHECK(Apply(s, Action::NextValue));
    CHECK(s.matte.r == 0 && s.matte.g == 0 && s.matte.b == 0 && s.matte.a == 255);
}

static void CheckInvalidRows() {
    State s;
    const State before = s;
    for (Row row : {static_cast<Row>(-1), Row::Count, static_cast<Row>(99)}) {
        CHECK(!IsEnabled(s, row));
        CHECK(ValueCount(row) == 0 && ValueIndex(s, row) == -1);
        FocusRow(s, row);
        CHECK(!SelectValue(s, row, 0));
        CHECK(s.focus == before.focus && SamePreferences(s, before));
        s.focus = row;
        NormalizeFocus(s);
        CHECK(s.focus == Row::Shape);
    }
}

int main() {
    CheckNavigation();
    CheckDependencyChangesAndTouch();
    CheckValuesAndIdle();
    CheckInvalidRows();
    std::puts("Texture settings regression checks passed.");
}
