#include "app.hpp"
#include "texture_browser_screen.hpp"
#include "texture_settings_screen.hpp"
#include "texture_settings_store.h"

#include <array>
#include <functional>

namespace borealis_app
{
namespace
{
using namespace brls::literals;

brls::Box* makeList(const std::string& title, const std::string& description)
{
    auto* list = new brls::Box(brls::Axis::COLUMN);
    auto style = brls::Application::getStyle();
    list->setPadding(style["brls/sidebar/padding_top"],
        style["brls/sidebar/padding_right"], style["brls/sidebar/padding_bottom"],
        style["brls/sidebar/padding_left"]);
    auto* header = new brls::Header();
    header->setTitle(title);
    list->addView(header);
    auto* label = new brls::Label();
    label->setText(description);
    label->setSingleLine(false);
    label->setMarginBottom(style["brls/sidebar/padding_bottom"]);
    list->addView(label);
    return list;
}

brls::ScrollingFrame* scroll(brls::Box* list)
{
    auto* frame = new brls::ScrollingFrame();
    frame->setContentView(list);
    frame->setGrow(1.0f);
    return frame;
}

brls::DetailCell* addAction(brls::Box* list, const std::string& text,
    const std::string& detail, std::function<void(brls::View*)> action)
{
    auto* cell = new brls::DetailCell();
    cell->setText(text);
    cell->setDetailText(detail);
    cell->registerClickAction([action](brls::View* owner) {
        // Touch activation must save the same return target as controller A.
        brls::Application::giveFocus(owner);
        action(owner);
        return true;
    });
    list->addView(cell);
    return cell;
}

class Navigation : public brls::Box
{
public:
    Navigation(std::shared_ptr<TextureBrowser::Session> session, std::function<void()> exit) : Box(brls::Axis::ROW)
    {
        sidebar = new brls::Sidebar();
        sidebar->setWidth(brls::Application::getStyle()["brls/tab_frame/sidebar_width"]);
        addView(sidebar);
        content = new brls::Box(brls::Axis::COLUMN);
        content->setGrow(1.0f);
        addView(content);

        const std::array<std::string, 5> names = {
            "app/webui"_i18n, "app/textures"_i18n, "app/mii"_i18n,
            "app/player"_i18n, "app/map"_i18n};
        const std::array<std::string, 5> descriptions = {
            "app/webui_note"_i18n, "app/textures_note"_i18n, "app/mii_note"_i18n,
            "app/player_note"_i18n, "app/map_note"_i18n};

        for (size_t i = 0; i < pages.size(); ++i)
        {
            if(i==1) {
                browser=new TextureBrowserScreen(session,[this]{focusSidebar();});
                pages[i]=browser;
                continue;
            }
            auto* list = makeList(names[i], descriptions[i]);
            if (i == 0)
            {
                addAction(list, "app/exit"_i18n, "", [exit](brls::View*) {
                    exit();
                });
            }
            else
            {
                addSample(list, brls::getStr("app/sample_" + std::to_string(i)),
                    "app/fictional"_i18n);
            }
            addAction(list, "app/tabs"_i18n, "", [this](brls::View*) { focusSidebar(); });
            pages[i] = scroll(list);
            pages[i]->registerAction("app/tabs_hint"_i18n, brls::BUTTON_B,
                [this](brls::View*) { focusSidebar(); return true; }, false, false, brls::SOUND_BACK);
        }

        content->addView(pages[active]);
        for (size_t i = 0; i < pages.size(); ++i)
        {
            sidebar->addItem(names[i], [this, i](brls::View*) { activate(i); });
            // B at the root tab selector has no unrelated action or exit dialog.
            sidebar->getItem(i)->registerAction("", brls::BUTTON_B,
                [](brls::View*) { return true; }, true);
        }
        registerAction("app/previous_tab"_i18n, brls::BUTTON_LT,
            [this](brls::View*) { switchTab(-1); return true; });
        registerAction("app/next_tab"_i18n, brls::BUTTON_RT,
            [this](brls::View*) { switchTab(1); return true; });
    }

    ~Navigation() override
    {
        // The attached page belongs to content; detached tabs remain owned here.
        for (size_t i = 0; i < pages.size(); ++i)
            if (i != active)
                pages[i]->freeView();
    }

private:
    void addSample(brls::Box* list, const std::string& name, const std::string& detail)
    {
        addAction(list, name, detail, [](brls::View*) {
            auto* dialog = new brls::Dialog("app/sample_note"_i18n);
            dialog->addButton("hints/back"_i18n, [] {});
            dialog->open();
        });
    }

    void activate(size_t index)
    {
        if (index == active)
            return;
        // Keep each tab's controls, in-memory values, focus and scroll position.
        if(active==1)browser->deactivateBrowser();
        content->clearViews(false);
        active = index;
        content->addView(pages[active]);
        if(active==1)browser->activateBrowser();
    }

    void switchTab(int direction)
    {
        const int count = static_cast<int>(pages.size());
        const int next = (static_cast<int>(active) + direction + count) % count;
        brls::Application::giveFocus(sidebar->getItem(next));
        brls::Application::giveFocus(pages[active]);
    }

    void focusSidebar()
    {
        brls::Application::giveFocus(sidebar->getItem(active));
    }

    TextureBrowserScreen* browser=nullptr;
    brls::Sidebar* sidebar;
    brls::Box* content;
    std::array<brls::ScrollingFrame*, 5> pages{};
    size_t active = 0;
};
} // namespace

brls::View* createRoot(std::shared_ptr<TextureBrowser::Session> session, std::function<void()> exit)
{
    auto* frame = new brls::AppletFrame(new Navigation(std::move(session),std::move(exit)));
    frame->setTitle("app/title"_i18n);
    // The app has an explicit focusable Exit control instead of a root B exit.
    frame->registerAction("", brls::BUTTON_B, [](brls::View*) { return true; }, true);
    return frame;
}
} // namespace borealis_app
