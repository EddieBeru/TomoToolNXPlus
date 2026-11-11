#include "app_chrome.hpp"
#include "exit_menu.hpp"
#include <borealis.hpp>

namespace borealis_app {
void showExitMenu(const std::function<void()>& requestExit,
    const std::function<void()>& saveChanges) {
    if (!requestExit) return;
    auto* dialog = makeDialog(brls::getStr("app/exit_menu_prompt"));
    dialog->addButton(brls::getStr("hints/cancel"), [] {});
    dialog->addButton(brls::getStr("app/save_only"), [saveChanges] {
        if (saveChanges) saveChanges();
        else brls::Application::notify(brls::getStr("app/no_pending_changes"));
    });
    dialog->addButton(brls::getStr("app/exit_loader"), requestExit);
    dialog->open();
}
}
