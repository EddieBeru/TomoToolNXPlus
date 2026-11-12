#include "navigation.hpp"
#include "screen_activity.hpp"

namespace borealis_app::navigation {
namespace {
class Footer : public brls::BottomBar {
public:
    Footer() {
        for (const auto* id : {"brls/battery", "brls/wireless", "brls/hints/time"})
            getView(id)->setVisibility(brls::Visibility::GONE);
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style style, brls::FrameContext* context) override {
        // Keep Borealis layout and contextual hints without updating the hidden clock.
        brls::Box::draw(vg, x, y, width, height, style, context);
    }
};

void bindAction(brls::View* scope, const std::string& label, brls::ControllerButton button,
    Action action, bool hidden = false, brls::Sound sound = brls::SOUND_NONE) {
    scope->registerAction(label, button, [action = std::move(action)](brls::View*) {
        action();
        return true;
    }, hidden, false, sound);
}
}

void initialize() {
    // AppletFrame and Dropdown both inflate this factory, including library overlays.
    brls::Application::registerXMLView("brls:BottomBar", [] { return new Footer(); });
}

void back(brls::View* scope, Action action, const std::string& label) {
    bindAction(scope, label, brls::BUTTON_B, std::move(action), false, brls::SOUND_BACK);
}

void root(brls::View* scope) {
    bindAction(scope, "", brls::BUTTON_B, [] {}, true, brls::SOUND_BACK);
}

void saveExit(brls::View* scope, Action action) {
    bindAction(scope, brls::getStr("app/exit_menu"), brls::BUTTON_START, std::move(action));
}

void tabs(brls::View* scope, std::function<void(int)> select) {
    bindAction(scope, brls::getStr("app/previous_tab"), brls::BUTTON_LT, [select] { select(-1); });
    bindAction(scope, brls::getStr("app/next_tab"), brls::BUTTON_RT, [select] { select(1); });
}

void push(brls::View* screen) {
    brls::Application::pushActivity(new ScreenActivity(screen), brls::TransitionAnimation::NONE);
    if (!contains(screen, brls::Application::getCurrentFocus()))
        focus(screen, screen);
}

void pop() {
    brls::Application::popActivity(brls::TransitionAnimation::NONE);
}
}
