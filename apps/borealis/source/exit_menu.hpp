#pragma once
#include <functional>

namespace borealis_app {
void showExitMenu(const std::function<void()>& requestExit,
    const std::function<void()>& saveChanges = {});
}
