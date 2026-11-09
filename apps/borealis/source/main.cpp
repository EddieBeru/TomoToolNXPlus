#include "app.hpp"

#include <cstdlib>

int main()
{
    brls::Platform::APP_LOCALE_DEFAULT = brls::LOCALE_AUTO;
    if (!brls::Application::init())
        return EXIT_FAILURE;

    brls::Application::createWindow(brls::getStr("app/title"));
    brls::Application::setGlobalQuit(false);
    brls::Application::pushActivity(new brls::Activity(borealis_app::createRoot()));

    while (brls::Application::mainLoop()) {}
    return EXIT_SUCCESS;
}
