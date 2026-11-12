#include "app_chrome.hpp"

namespace borealis_app {
brls::Dialog* makeDialog(const std::string& text) {
    return new brls::Dialog(text);
}
brls::Dropdown* makeDropdown(const std::string& title, const std::vector<std::string>& values,
    brls::ValueSelectedEvent::Callback selected, int current) {
    return new brls::Dropdown(title, values, std::move(selected), current);
}
}
