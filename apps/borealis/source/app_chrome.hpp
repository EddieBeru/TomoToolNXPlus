#pragma once
#include <borealis.hpp>

namespace borealis_app {
void hideStatusWidgets(brls::View* root);
brls::Dialog* makeDialog(const std::string& text);
brls::Dropdown* makeDropdown(const std::string& title, const std::vector<std::string>& values,
    brls::ValueSelectedEvent::Callback selected, int current);
}
