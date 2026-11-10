#include "app.hpp"
#include "texture_read_source.hpp"

#include <cstdlib>

int main()
{
    brls::Platform::APP_LOCALE_DEFAULT = brls::LOCALE_AUTO;
    if (!brls::Application::init())
        return EXIT_FAILURE;

    brls::Application::createWindow(brls::getStr("app/title"));
    brls::Application::setGlobalQuit(false);
    auto session=std::make_shared<TextureBrowser::Session>(TextureBrowser::createReadSource(),
        [](auto callback){brls::sync(callback);});
    bool closing=false;
    brls::Application::pushActivity(new brls::Activity(borealis_app::createRoot(session,[&] {
        closing=true;session->stop();
        brls::Application::notify(brls::getStr("app/browser_closing"));
    })));

    while (brls::Application::mainLoop()) {
        if(closing && session->finished())brls::Application::quit();
    }
    session->stop();
    return EXIT_SUCCESS;
}
