#include "exit_menu.hpp"
#include <borealis.hpp>

namespace borealis_app {
void showExitMenu(const std::function<void()>& requestExit) {
    if (!requestExit) return;
    auto* dialog = new brls::Dialog(brls::getStr("app/exit_confirm"));
    dialog->addButton(brls::getStr("hints/cancel"), [] {});
    dialog->addButton(brls::getStr("app/exit_loader"), requestExit);
    dialog->open();
}
}
