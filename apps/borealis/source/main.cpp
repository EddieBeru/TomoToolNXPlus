#include "app.hpp"
#include "screen_activity.hpp"
#include "texture_read_source.hpp"

#include <cstdlib>

int main()
{
    brls::Platform::APP_LOCALE_DEFAULT = brls::LOCALE_AUTO;
    if (!brls::Application::init())
        return EXIT_FAILURE;

    brls::Application::createWindow(brls::getStr("app/title"));
    brls::Application::setGlobalQuit(false);
    brls::Application::getPlatform()->exitToHomeMode(false);
    const auto profile=borealis_app::selectNativeProfile();
    auto session=std::make_shared<TextureBrowser::Session>(TextureBrowser::createReadSource(),
        [](auto callback){brls::sync(callback);});
    bool closing=false;
    brls::Application::pushActivity(new borealis_app::ScreenActivity(borealis_app::createRoot(session,[&] {
        if(closing)return;
        closing=true;session->stop();
        brls::Application::notify(brls::getStr("app/browser_closing"));
    },profile)));

    if(profile.state==borealis_app::ProfileSelectionState::Failed)
        brls::Application::notify(brls::getStr("app/profile_not_selected"));
    while (brls::Application::mainLoop()) {
        if(closing && session->finished())brls::Application::quit();
    }
    session->stop();
    return EXIT_SUCCESS;
}
