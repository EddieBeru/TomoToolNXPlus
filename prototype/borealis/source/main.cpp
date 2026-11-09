#include <borealis.hpp>
#include <cstdlib>

int main()
{
    brls::Platform::APP_LOCALE_DEFAULT = brls::LOCALE_AUTO;
    if (!brls::Application::init())
        return EXIT_FAILURE;

    brls::Application::createWindow("TomoToolNX Borealis Pilot");
    brls::Application::setGlobalQuit(false);

    auto* content = new brls::Box(brls::Axis::COLUMN);
    auto* exit = new brls::Button();
    exit->setText("Exit pilot");
    exit->registerClickAction([](brls::View*) {
        brls::Application::quit();
        return true;
    });
    content->addView(exit);
    auto* frame = new brls::AppletFrame(content);
    frame->setTitle("TomoToolNX Borealis Pilot");
    brls::Application::pushActivity(new brls::Activity(frame));

    while (brls::Application::mainLoop()) {}
    return EXIT_SUCCESS;
}
