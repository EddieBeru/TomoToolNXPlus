#pragma once
#include <borealis.hpp>

namespace borealis_app {
// Borealis Activity does not forward pause/resume to the view by default.
class ScreenActivity : public brls::Activity {
public:
    explicit ScreenActivity(brls::View* view) : Activity(view) {}
    void onPause() override { getContentView()->willDisappear(false); }
    void onResume() override { getContentView()->willAppear(false); }
};
}
