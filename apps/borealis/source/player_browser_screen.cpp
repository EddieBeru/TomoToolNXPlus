#include "player_browser_screen.hpp"
#include "navigation.hpp"
#include <algorithm>

namespace borealis_app {
namespace {

using namespace brls::literals;
using namespace PlayerProfile;

std::string tr(const std::string& key) {
    return brls::getStr("app/" + key);
}

std::string errorMessage(PlayerReadError e) {
    switch (e) {
        case PlayerReadError::NoAccountSelected:
            return tr("player_choose_global");
        case PlayerReadError::NoSave:
            return tr("player_no_save");
        case PlayerReadError::AccountMetadataFailed:
        case PlayerReadError::ReadOnlyOpenFailed:
            return tr("player_access_error");
        case PlayerReadError::FileTooLarge:
            return tr("player_limit");
        case PlayerReadError::MissingRequiredField:
            return tr("player_missing_required");
        case PlayerReadError::InvalidHeader:
        case PlayerReadError::CorruptIndex:
        case PlayerReadError::TruncatedPayload:
        case PlayerReadError::TypeMismatch:
            return tr("player_invalid");
        default:
            return tr("player_read_error");
    }
}

class Cell : public brls::DetailCell {
public:
    std::function<void()> focused;
    void onFocusGained() override {
        brls::DetailCell::onFocusGained();
        if (focused)
            focused();
    }
};

Cell* addActionCell(brls::Box* box, const std::string& title, std::function<void()> callback) {
    auto* cell = new Cell();
    cell->setText(title);
    cell->registerClickAction([cell, callback](brls::View*) {
        brls::Application::giveFocus(cell);
        callback();
        return true;
    });
    box->addView(cell);
    return cell;
}

brls::Label* addMultilineLabel(brls::Box* box, const std::string& text) {
    auto* result = new brls::Label();
    result->setSingleLine(false);
    result->setText(text);
    result->setMarginBottom(16);
    box->addView(result);
    return result;
}

} // namespace

PlayerBrowserScreen::PlayerBrowserScreen(
    std::shared_ptr<TextureBrowser::Session> session,
    std::function<void()> tabs)
    : life(std::make_shared<Life>(Life{this})),
      session(std::move(session)),
      focusTabs(std::move(tabs)) {
    list = new brls::Box(brls::Axis::COLUMN);
    list->setPadding(24, 28, 24, 28);
    setContentView(list);
    setGrow(1);

    auto* header = new brls::Header();
    header->setTitle("app/player"_i18n);
    list->addView(header);

    addMultilineLabel(list, tr("player_note"));
    refreshControl = addActionCell(list, tr("browser_refresh"), [this] { refresh(); });

    status = addMultilineLabel(list, tr("player_choose_global"));
    entries = new brls::Box(brls::Axis::COLUMN);
    list->addView(entries);

    tabsControl = addActionCell(list, "app/tabs"_i18n, focusTabs);
    list->setLastFocusedView(refreshControl);
    navigation::back(this, focusTabs, "app/tabs_hint"_i18n);
}

PlayerBrowserScreen::~PlayerBrowserScreen() {
    life->view = nullptr;
    session->cancel();
}

void PlayerBrowserScreen::setProfile(const ProfileSelection& selection) {
    if (selection.state != ProfileSelectionState::Selected || !selection.account.key.valid())
        return;
    profile = selection;
    if (model.hasAccount && model.context.account.key == selection.account.key) {
        model.context.account = selection.account;
        return;
    }
    session->cancel();
    model.select(selection.account);
    initialLoad = true;
    dirty = true;
}

void PlayerBrowserScreen::activateBrowser() {
    active = true;
    if (initialLoad) {
        initialLoad = false;
        refresh(false);
        return;
    }
    if (model.resume())
        refresh(false);
    else if (dirty)
        render();
    else
        restore();
}

void PlayerBrowserScreen::deactivateBrowser() {
    if (!active)
        return;
    active = false;
    model.context.scroll = getContentOffsetY();
    if (model.state == PlayerBrowserState::Loading) {
        model.suspend();
        session->cancel();
    }
}

void PlayerBrowserScreen::willAppear(bool resetState) {
    brls::ScrollingFrame::willAppear(resetState);
    activateBrowser();
}

void PlayerBrowserScreen::willDisappear(bool resetState) {
    deactivateBrowser();
    brls::ScrollingFrame::willDisappear(resetState);
}

void PlayerBrowserScreen::refresh(bool begin) {
    if (!model.hasAccount) {
        render();
        return;
    }
    if (begin && model.state == PlayerBrowserState::Loading)
        return;
    if (begin)
        model.loading();
    render();

    std::weak_ptr<Life> weak = life;
    const auto account = model.context.account.key;
    const auto generation = model.generation;
    session->submit([weak, account, generation](TextureBrowser::ReadSource& source, const TextureBrowser::Cancel& cancel) {
        auto result = LoadFromReadSource(source, account, cancel);
        return [weak, account, generation, result = std::move(result)]() mutable {
            auto locked = weak.lock();
            if (!locked || !locked->view)
                return;
            auto* self = locked->view;
            if (self->model.generation != generation || self->model.context.account.key != account)
                return;
            self->model.accept(std::move(result));
            self->dirty = true;
            if (self->active)
                self->render();
        };
    });
}

void PlayerBrowserScreen::render() {
    if (!active) {
        dirty = true;
        return;
    }
    dirty = false;

    const bool isLoading = (model.state == PlayerBrowserState::Loading);
    const bool canRefresh = model.hasAccount && !isLoading;
    const std::string savedFocus = model.context.focusTarget;

    refreshControl->setFocusable(canRefresh);
    refreshControl->setAlpha(canRefresh ? 1.0f : 0.45f);
    if (navigation::contains(entries, brls::Application::getCurrentFocus()) ||
        (!canRefresh && navigation::contains(refreshControl, brls::Application::getCurrentFocus()))) {
        navigation::focus(this, tabsControl);
    }

    pendingFocus = nullptr;
    entries->clearViews();
    entries->setLastFocusedView(nullptr);

    if (isLoading) {
        status->setText(tr("player_loading"));
        list->setLastFocusedView(tabsControl);
        return;
    }
    if (!model.hasAccount || model.state == PlayerBrowserState::Unselected) {
        status->setText(tr("player_choose_global"));
        list->setLastFocusedView(tabsControl);
        return;
    }
    if (model.state == PlayerBrowserState::Error) {
        status->setText(errorMessage(model.status.error));
        list->setLastFocusedView(refreshControl);
        pendingFocus = (savedFocus == "tabs") ? static_cast<brls::View*>(tabsControl) : static_cast<brls::View*>(refreshControl);
        restore();
        return;
    }
    if (model.state != PlayerBrowserState::Ready) {
        status->setText(tr("player_choose_global"));
        list->setLastFocusedView(tabsControl);
        return;
    }

    std::string headerNotice = tr("player_readonly");
    if (model.summary.hasFieldWarnings)
        headerNotice += "\n" + tr("player_warnings_notice");
    status->setText(headerNotice);

    auto style = brls::Application::getStyle();
    const auto attrs = FormatAttributes(model.summary, [](const std::string& key) {
        return brls::getStr("app/" + key);
    });

    Cell* firstAttr = nullptr;
    Cell* matchedAttr = nullptr;
    for (size_t i = 0; i < attrs.size(); ++i) {
        const auto& attr = attrs[i];
        auto* cell = new Cell();
        cell->setText(attr.title);
        cell->setDetailText(attr.primaryValue);
        cell->registerClickAction([cell](brls::View*) {
            brls::Application::giveFocus(cell);
            return true;
        });
        cell->focused = [this, i, key = attr.focusKey, cell] {
            model.context.selectedAttrIndex = static_cast<int>(i);
            model.context.focusTarget = key;
            entries->setLastFocusedView(cell);
            list->setLastFocusedView(entries);
        };
        entries->addView(cell);

        auto* desc = new brls::Label();
        desc->setSingleLine(false);
        desc->setText(attr.secondaryMetadata + "\n" + attr.description);
        desc->setMargins(
            style["brls/sidebar/item_accent_margin_top_bottom"],
            style["brls/listitem/descriptionIndent"],
            style["brls/listitem/descriptionIndent"],
            style["brls/listitem/descriptionIndent"]);
        entries->addView(desc);

        if (!firstAttr)
            firstAttr = cell;
        if (attr.focusKey == savedFocus)
            matchedAttr = cell;
    }

    if (matchedAttr) {
        pendingFocus = matchedAttr;
        entries->setLastFocusedView(matchedAttr);
        list->setLastFocusedView(entries);
    } else if (firstAttr) {
        entries->setLastFocusedView(firstAttr);
        list->setLastFocusedView(entries);
    }

    restore();
}

void PlayerBrowserScreen::restore() {
    restorePending = true;
    invalidate();
}

void PlayerBrowserScreen::onLayout() {
    brls::ScrollingFrame::onLayout();
    if (restorePending && active && navigation::isActive(this)) {
        restorePending = false;
        if (pendingFocus) {
            navigation::focus(this, pendingFocus);
            pendingFocus = nullptr;
        }
        auto offset = std::max(0.0f, model.context.scroll);
        offset = std::min(offset, std::max(0.0f, list->getHeight() - getHeight()));
        setContentOffsetY(offset, false);
    }
}

} // namespace borealis_app
