#include "texture_settings_screen.hpp"
#include "texture_settings_store.h"

#include <array>
#include <cstdio>

namespace pilot {
namespace {
using namespace brls::literals;
using TextureSettings::Row;

class SettingsScreen : public brls::AppletFrame {
public:
    explicit SettingsScreen(const std::string& path)
        : AppletFrame(new brls::ScrollingFrame()), store(path) {
        setTitle("pilot/settings"_i18n);
        auto* list = new brls::Box(brls::Axis::COLUMN);
        auto style = brls::Application::getStyle();
        list->setPadding(style["brls/sidebar/padding_top"], style["brls/sidebar/padding_right"],
            style["brls/sidebar/padding_bottom"], style["brls/sidebar/padding_left"]);
        auto* note = new brls::Label();
        note->setText("preferences/note"_i18n);
        note->setSingleLine(false);
        note->setMarginBottom(style["brls/listitem/descriptionIndent"]);
        list->addView(note);
        status = new brls::Label();
        status->setSingleLine(false);
        status->setMarginBottom(style["brls/listitem/descriptionIndent"]);
        list->addView(status);
        reloadControl = action(list, "preferences/reload"_i18n, [this] {
            if (ready && store.Dirty(state)) {
                auto* dialog = new brls::Dialog("preferences/reload_confirm"_i18n);
                dialog->addButton("hints/cancel"_i18n, [] {});
                dialog->addButton("preferences/reload"_i18n, [this] { load(); });
                dialog->open();
            } else load();
        });
        reloadControl->setMarginBottom(style["brls/listitem/descriptionIndent"]);
        const std::array<const char*, TextureSettings::RowCount> names = {
            "shape", "encoder", "bc1", "fit", "background"};
        for (int i = 0; i < TextureSettings::RowCount; ++i) {
            rows[i] = action(list, brls::getStr(std::string("preferences/") + names[i]),
                [this, i] { choose(static_cast<Row>(i)); });
            descriptions[i] = new brls::Label();
            descriptions[i]->setSingleLine(false);
            descriptions[i]->setMargins(style["brls/sidebar/item_accent_margin_top_bottom"],
                style["brls/listitem/descriptionIndent"], style["brls/listitem/descriptionIndent"],
                style["brls/listitem/descriptionIndent"]);
            list->addView(descriptions[i]);
        }
        saveControl = action(list, "preferences/save"_i18n, [this] { save(false); });
        backControl = action(list, "hints/back"_i18n, [this] { back(); });
        static_cast<brls::ScrollingFrame*>(getContentView())->setContentView(list);
        registerAction("hints/back"_i18n, brls::BUTTON_B,
            [this](brls::View*) { back(); return true; }, false, false, brls::SOUND_BACK);
        list->setLastFocusedView(rows[0]);
        load(false);
    }

private:
    brls::DetailCell* action(brls::Box* list, const std::string& title, std::function<void()> callback) {
        auto* cell = new brls::DetailCell();
        cell->setText(title);
        cell->registerClickAction([cell, callback](brls::View*) {
            brls::Application::giveFocus(cell);
            callback();
            return true;
        });
        list->addView(cell);
        return cell;
    }

    void enabled(brls::DetailCell* cell, bool available) {
        cell->setFocusable(available);
        cell->setActionsAvailable(available);
        auto theme = brls::Application::getTheme();
        cell->setTextColor(theme[available ? "brls/text" : "brls/text_disabled"]);
        cell->setDetailTextColor(theme[available ? "brls/list/listItem_value_color" : "brls/text_disabled"]);
    }

    std::vector<std::string> choices(Row row) const {
        switch (row) {
            case Row::Shape: return {"preferences/square"_i18n, "preferences/book"_i18n, "preferences/tv"_i18n};
            case Row::Encoder: return {"preferences/gamma"_i18n, "preferences/pca"_i18n};
            case Row::Bc1Mode: return {"preferences/auto"_i18n, "preferences/four_color"_i18n, "preferences/three_color"_i18n};
            case Row::Fit: return {"pilot/cover"_i18n, "pilot/contain"_i18n, "pilot/fill"_i18n};
            case Row::Background: {
                std::vector<std::string> values = {
                    "preferences/transparent"_i18n, "preferences/white"_i18n, "preferences/black"_i18n};
                if (TextureSettings::ValueIndex(state, row) < 0) {
                    char hex[8];
                    std::snprintf(hex, sizeof(hex), "#%02x%02x%02x", state.matte.r, state.matte.g, state.matte.b);
                    values.push_back(brls::getStr("preferences/custom_color", hex));
                }
                return values;
            }
            default: return {};
        }
    }

    void choose(Row row) {
        if (!ready || !TextureSettings::IsEnabled(state, row)) return;
        const int index = static_cast<int>(row);
        auto values = choices(row);
        const int selected = TextureSettings::ValueIndex(state, row);
        auto* dropdown = new brls::Dropdown(rows[index]->title->getFullText(), values,
            [this, row](int value) {
                // The extra custom-color choice preserves an existing color as-is.
                if (TextureSettings::SelectValue(state, row, value)) {
                    status->setText("preferences/unsaved"_i18n);
                    refresh();
                }
            }, selected < 0 ? static_cast<int>(values.size()) - 1 : selected);
        brls::Application::pushActivity(new brls::Activity(dropdown));
    }

    void refresh() {
        const std::array<std::string, TextureSettings::RowCount> help = {
            "preferences/shape_help"_i18n, "preferences/encoder_help"_i18n,
            TextureSettings::IsEnabled(state, Row::Bc1Mode) ? "preferences/bc1_help"_i18n : "preferences/bc1_disabled"_i18n,
            "preferences/fit_help"_i18n,
            TextureSettings::IsEnabled(state, Row::Background) ? "preferences/background_help"_i18n : "preferences/background_disabled"_i18n};
        for (int i = 0; i < TextureSettings::RowCount; ++i) {
            const auto row = static_cast<Row>(i);
            const auto values = choices(row);
            const int selected = TextureSettings::ValueIndex(state, row);
            rows[i]->setDetailText(ready ? values[selected < 0 ? values.size() - 1 : selected] : "");
            enabled(rows[i], ready && TextureSettings::IsEnabled(state, row));
            descriptions[i]->setText(help[i]);
        }
        enabled(saveControl, ready && store.Dirty(state));
    }

    std::string errorMessage(const TextureSettings::ConfigResult& result) const {
        using TextureSettings::ConfigError;
        const char* key = "preferences/read_error";
        switch (result.error) {
            case ConfigError::Invalid: key = "preferences/invalid_error"; break;
            case ConfigError::Conflict: key = "preferences/conflict_error"; break;
            case ConfigError::Backup: key = "preferences/backup_error"; break;
            case ConfigError::Write: case ConfigError::Verify: key = "preferences/write_error"; break;
            default: break;
        }
        std::string message = brls::getStr(key);
        if (!result.backupPath.empty()) message += "\n" + brls::getStr("preferences/backup_location", result.backupPath);
        if (result.mayHaveChanged) message += "\n" + "preferences/restore_required"_i18n;
        return message;
    }

    void load(bool focus = true) {
        const auto result = store.Load(state);
        ready = static_cast<bool>(result);
        status->setText(ready ? (store.Exists() ? "preferences/loaded"_i18n : "preferences/defaults"_i18n)
                              : errorMessage(result));
        refresh();
        if (focus) brls::Application::giveFocus(ready ? rows[0] : reloadControl);
    }

    void save(bool returnAfter) {
        const auto result = store.Save(state);
        if (!result) {
            if (result.mayHaveChanged) ready = false;
            status->setText(errorMessage(result));
            refresh();
            auto* dialog = new brls::Dialog(errorMessage(result));
            dialog->addButton("hints/back"_i18n, [this] { brls::Application::giveFocus(reloadControl); });
            dialog->getAppletFrame()->registerAction("hints/back"_i18n, brls::BUTTON_B,
                [this, dialog](brls::View*) {
                    dialog->close([this] { brls::Application::giveFocus(reloadControl); });
                    return true;
                }, false, false, brls::SOUND_BACK);
            dialog->open();
            return;
        }
        status->setText("preferences/saved"_i18n);
        brls::Application::notify("preferences/saved"_i18n);
        refresh();
        if (returnAfter) brls::Application::popActivity();
        else brls::Application::giveFocus(backControl);
    }

    void back() {
        if (!ready || !store.Dirty(state)) { brls::Application::popActivity(); return; }
        auto* dialog = new brls::Dialog("preferences/leave_confirm"_i18n);
        dialog->addButton("preferences/save_return"_i18n, [this] { save(true); });
        dialog->addButton("preferences/discard"_i18n, [] { brls::Application::popActivity(); });
        dialog->addButton("preferences/keep_editing"_i18n, [] {});
        dialog->open();
    }

    TextureSettings::ConfigStore store;
    TextureSettings::State state;
    bool ready = false;
    brls::Label* status;
    std::array<brls::DetailCell*, TextureSettings::RowCount> rows{};
    std::array<brls::Label*, TextureSettings::RowCount> descriptions{};
    brls::DetailCell *saveControl, *reloadControl, *backControl;
};
} // namespace

brls::View* createTextureSettingsScreen(const std::string& configPath) {
    return new SettingsScreen(configPath);
}
} // namespace pilot
