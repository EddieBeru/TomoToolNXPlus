#pragma once

#include <borealis.hpp>

namespace borealis_app::navigation {
inline bool contains(brls::View* scope, brls::View* view) {
    for (; view; view = view->getParent())
        if (view == scope) return true;
    return false;
}

inline bool isActive(brls::View* scope) {
    const auto activities = brls::Application::getActivitiesStack();
    return !activities.empty() && contains(activities.back()->getContentView(), scope);
}

inline bool focus(brls::View* scope, brls::View* target) {
    if (!target || !isActive(scope) || !contains(scope, target)) return false;
    auto* resolved = target->getDefaultFocus();
    if (!resolved || !contains(scope, resolved)) return false;
    brls::Application::giveFocus(resolved);
    return true;
}
}
