#pragma once

#include <borealis.hpp>
#include <cstddef>
#include <functional>
#include "navigation_focus.hpp"

namespace borealis_app::navigation {
using Action = std::function<void()>;

// Install after createWindow() registers Borealis factories, before creating app views.
void initialize();
void configurePrimaryFont(void* address, size_t size);
void back(brls::View* scope, Action action, const std::string& label = brls::getStr("hints/back"));
void root(brls::View* scope);
void saveExit(brls::View* scope, Action action);
void tabs(brls::View* scope, std::function<void(int)> select);

// Borealis owns the view and restores the entering control through its focus stack.
void push(brls::View* screen);
void pop();
}
