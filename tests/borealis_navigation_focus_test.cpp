#include "navigation_focus.hpp"
#include <cassert>
#include <cstdio>

int main() {
    using namespace borealis_app::navigation;
    brls::View root, browser, entry, detail, back, modal, confirm;
    browser.parent=&root;
    entry.parent=&browser;
    back.parent=&detail;
    confirm.parent=&modal;
    detail.defaultFocus=&back;
    modal.defaultFocus=&confirm;
    brls::Activity rootActivity{&root}, detailActivity{&detail}, modalActivity{&modal};

    assert(!isActive(&root));
    assert(!focus(&browser,&entry));
    brls::Application::activities={&rootActivity};
    assert(focus(&browser,&entry));
    assert(brls::Application::currentFocus==&entry);

    // Entering a detail must isolate it from deferred parent layout/restore work.
    brls::Application::activities.push_back(&detailActivity);
    assert(focus(&detail,&detail));
    assert(brls::Application::currentFocus==&back);
    assert(!focus(&browser,&entry));
    assert(!focus(&detail,&entry));
    assert(brls::Application::currentFocus==&back);

    // Preview completion behind a modal must not steal its controller focus.
    brls::Application::activities.push_back(&modalActivity);
    assert(focus(&modal,&modal));
    assert(!focus(&detail,&back));
    assert(!focus(&browser,&entry));
    assert(brls::Application::currentFocus==&confirm);
    brls::Application::activities.pop_back();
    assert(focus(&detail,&back));

    // A malformed default focus must never escape the current scope.
    detail.defaultFocus=&entry;
    assert(!focus(&detail,&detail));
    assert(brls::Application::currentFocus==&back);
    assert(!focus(&detail,nullptr));

    // Popping the child makes restoration of the original texture valid again.
    brls::Application::activities.pop_back();
    assert(focus(&browser,&entry));
    assert(brls::Application::currentFocus==&entry);
    assert(!focus(&detail,&back));
    std::puts("Borealis navigation focus ownership: OK");
}
