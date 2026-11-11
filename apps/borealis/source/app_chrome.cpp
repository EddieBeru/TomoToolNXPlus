#include "app_chrome.hpp"

namespace borealis_app {
void hideStatusWidgets(brls::View* root) {
    if (!root) return;
    if (root->getView("brls/battery") == root || root->getView("brls/hints/time") == root)
        root->setVisibility(brls::Visibility::GONE);
    if (auto* box = dynamic_cast<brls::Box*>(root))
        for (auto* child : box->getChildren())
            hideStatusWidgets(child);
}
brls::Dialog* makeDialog(const std::string& text) {
    auto* dialog = new brls::Dialog(text);
    hideStatusWidgets(dialog);
    return dialog;
}
brls::Dropdown* makeDropdown(const std::string& title, const std::vector<std::string>& values,
    brls::ValueSelectedEvent::Callback selected, int current) {
    auto* dropdown = new brls::Dropdown(title, values, std::move(selected), current);
    // Dropdown owns an additional BottomBar outside its AppletFrame.
    hideStatusWidgets(dropdown);
    return dropdown;
}
}
