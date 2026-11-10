#pragma once

#include <borealis.hpp>
#include "texture_browser_session.hpp"

namespace borealis_app
{
brls::View* createRoot(std::shared_ptr<TextureBrowser::Session> session, std::function<void()> exit);
}
