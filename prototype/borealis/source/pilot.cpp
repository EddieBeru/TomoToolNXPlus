#include "pilot.hpp"
#include "texture_settings_screen.hpp"
#include "texture_settings_store.h"

#include <array>
#include <functional>

namespace pilot
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
    Navigation() : Box(brls::Axis::ROW)
    {
        sidebar = new brls::Sidebar();
        sidebar->setWidth(brls::Application::getStyle()["brls/tab_frame/sidebar_width"]);
        addView(sidebar);
        content = new brls::Box(brls::Axis::COLUMN);
        content->setGrow(1.0f);
        addView(content);

        const std::array<std::string, 5> names = {
            "pilot/webui"_i18n, "pilot/textures"_i18n, "pilot/mii"_i18n,
            "pilot/player"_i18n, "pilot/map"_i18n};
        const std::array<std::string, 5> descriptions = {
            "pilot/webui_note"_i18n, "pilot/textures_note"_i18n, "pilot/mii_note"_i18n,
            "pilot/player_note"_i18n, "pilot/map_note"_i18n};

        for (size_t i = 0; i < pages.size(); ++i)
        {
            auto* list = makeList(names[i], descriptions[i]);
            if (i == 1)
            {
                addAction(list, "pilot/settings"_i18n, "hints/open"_i18n,
                    [this](brls::View*) {
                        brls::Application::pushActivity(new brls::Activity(createTextureSettingsScreen(TextureSettings::ConfigPath)));
                    });
                addSample(list, "pilot/shirt"_i18n, "pilot/ready"_i18n);
                addSample(list, "pilot/room"_i18n, "pilot/ready"_i18n);
                addSample(list, "pilot/empty_slot"_i18n, "pilot/empty"_i18n);
            }
            else if (i == 0)
            {
                addAction(list, "pilot/exit"_i18n, "", [](brls::View*) {
                    brls::Application::quit();
                });
            }
            else
            {
                addSample(list, brls::getStr("pilot/sample_" + std::to_string(i)),
                    "pilot/fictional"_i18n);
            }
            addAction(list, "pilot/tabs"_i18n, "", [this](brls::View*) { focusSidebar(); });
            pages[i] = scroll(list);
            pages[i]->registerAction("pilot/tabs_hint"_i18n, brls::BUTTON_B,
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
        registerAction("pilot/previous_tab"_i18n, brls::BUTTON_LT,
            [this](brls::View*) { switchTab(-1); return true; });
        registerAction("pilot/next_tab"_i18n, brls::BUTTON_RT,
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
            auto* dialog = new brls::Dialog("pilot/sample_note"_i18n);
            dialog->addButton("hints/back"_i18n, [] {});
            dialog->open();
        });
    }

    void activate(size_t index)
    {
        if (index == active)
            return;
        // Keep each tab's controls, in-memory values, focus and scroll position.
        content->clearViews(false);
        active = index;
        content->addView(pages[active]);
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

    brls::Sidebar* sidebar;
    brls::Box* content;
    std::array<brls::ScrollingFrame*, 5> pages{};
    size_t active = 0;
};
} // namespace

brls::View* createRoot()
{
    auto* frame = new brls::AppletFrame(new Navigation());
    frame->setTitle("pilot/title"_i18n);
    // The pilot has an explicit focusable Exit control instead of a root B exit.
    frame->registerAction("", brls::BUTTON_B, [](brls::View*) { return true; }, true);
    return frame;
}
} // namespace pilot
