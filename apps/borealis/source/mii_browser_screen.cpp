#include "mii_browser_screen.hpp"
#include "exit_menu.hpp"
#include "navigation.hpp"
#include "profile_header.hpp"
#include <algorithm>

namespace borealis_app {
namespace {

using namespace brls::literals;
using namespace MiiRoster;

std::string tr(const std::string& key) {
    return brls::getStr("app/" + key);
}

std::string errorMessage(MiiReadError e) {
    switch (e) {
        case MiiReadError::NoAccountSelected:
            return tr("mii_choose_global");
        case MiiReadError::NoSave:
            return tr("mii_no_save");
        case MiiReadError::AccountMetadataFailed:
        case MiiReadError::ReadOnlyOpenFailed:
            return tr("mii_access_error");
        case MiiReadError::FileTooLarge:
            return tr("mii_limit");
        case MiiReadError::MissingRequiredField:
            return tr("mii_missing_required");
        case MiiReadError::InvalidHeader:
        case MiiReadError::CorruptIndex:
        case MiiReadError::TruncatedPayload:
        case MiiReadError::TypeMismatch:
            return tr("mii_invalid");
        default:
            return tr("mii_read_error");
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

class MiiSectionScreen : public brls::AppletFrame {
public:
    MiiSectionScreen(
        std::shared_ptr<const MiiRosterSummary> snapshot,
        size_t residentIndex,
        MiiSectionId sectionId,
        const std::string& sectionTitle,
        const ProfileSelection& profile,
        std::function<void()> exit)
        : AppletFrame(new brls::ScrollingFrame()),
          snapshot(std::move(snapshot)),
          requestExit(std::move(exit)) {
        const auto& resident = this->snapshot->residents[residentIndex];
        setTitle(resident.stats.name + " — " + sectionTitle);
        addProfileHeader(this, profile);

        auto* box = new brls::Box(brls::Axis::COLUMN);
        box->setPadding(20, 28, 20, 28);
        auto* scrolling = static_cast<brls::ScrollingFrame*>(getContentView());
        scrolling->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
        scrolling->setContentView(box);

        auto* backControl = addActionCell(box, "hints/back"_i18n, [] { navigation::pop(); });
        auto style = brls::Application::getStyle();
        const auto rows = FormatSectionRows(resident, sectionId, [](const std::string& k) {
            return brls::getStr("app/" + k);
        });

        for (const auto& row : rows) {
            auto* cell = new Cell();
            cell->setText(row.title);
            cell->setDetailText(row.primaryValue);
            cell->registerClickAction([cell](brls::View*) {
                brls::Application::giveFocus(cell);
                return true;
            });
            box->addView(cell);

            if (!row.secondaryText.empty()) {
                auto* sub = new brls::Label();
                sub->setSingleLine(false);
                sub->setText(row.secondaryText);
                sub->setMargins(
                    style["brls/sidebar/item_accent_margin_top_bottom"],
                    style["brls/listitem/descriptionIndent"],
                    style["brls/listitem/descriptionIndent"],
                    style["brls/listitem/descriptionIndent"]);
                box->addView(sub);
            }
        }

        navigation::saveExit(this, [this] { showExitMenu(requestExit); });
        navigation::back(this, [] { navigation::pop(); });
        setLastFocusedView(backControl);
        box->setLastFocusedView(backControl);
    }

private:
    std::shared_ptr<const MiiRosterSummary> snapshot;
    std::function<void()> requestExit;
};

class MiiDetailScreen : public brls::AppletFrame {
public:
    MiiDetailScreen(
        std::shared_ptr<const MiiRosterSummary> snapshot,
        size_t residentIndex,
        const ProfileSelection& profile,
        std::function<void()> exit,
        std::function<void()> returned)
        : AppletFrame(new brls::ScrollingFrame()),
          snapshot(std::move(snapshot)),
          residentIndex(residentIndex),
          profile(profile),
          requestExit(std::move(exit)),
          returned(std::move(returned)) {
        const auto& resident = this->snapshot->residents[residentIndex];
        setTitle(resident.stats.name + " · #" + std::to_string(resident.slotNumber));
        addProfileHeader(this, profile);

        auto* box = new brls::Box(brls::Axis::COLUMN);
        box->setPadding(20, 28, 20, 28);
        auto* scrolling = static_cast<brls::ScrollingFrame*>(getContentView());
        scrolling->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);
        scrolling->setContentView(box);

        auto* backControl = addActionCell(box, "hints/back"_i18n, [this] { back(); });
        addMultilineLabel(box, tr("mii_note"));

        const auto overviews = FormatSectionOverviews(resident, [](const std::string& k) {
            return brls::getStr("app/" + k);
        });
        for (const auto& ov : overviews) {
            auto* cell = new Cell();
            cell->setText(ov.title);
            cell->setDetailText(ov.summaryText);
            cell->focused = [box, cell] { box->setLastFocusedView(cell); };
            cell->registerClickAction([this, cell, sectionId = ov.id, title = ov.title](brls::View*) {
                brls::Application::giveFocus(cell);
                navigation::push(new MiiSectionScreen(
                    this->snapshot, this->residentIndex, sectionId, title, this->profile, this->requestExit));
                return true;
            });
            box->addView(cell);
        }

        navigation::saveExit(this, [this] { showExitMenu(requestExit); });
        navigation::back(this, [this] { back(); });
        setLastFocusedView(backControl);
        box->setLastFocusedView(backControl);
    }

private:
    std::shared_ptr<const MiiRosterSummary> snapshot;
    size_t residentIndex = 0;
    ProfileSelection profile;
    std::function<void()> requestExit;
    std::function<void()> returned;

    void back() {
        if (returned)
            returned();
        navigation::pop();
    }
};

} // namespace

MiiBrowserScreen::MiiBrowserScreen(
    std::shared_ptr<TextureBrowser::Session> session,
    std::function<void()> tabs,
    std::function<void()> exit)
    : life(std::make_shared<Life>(Life{this})),
      session(std::move(session)),
      focusTabs(std::move(tabs)),
      requestExit(std::move(exit)) {
    list = new brls::Box(brls::Axis::COLUMN);
    list->setPadding(24, 28, 24, 28);
    setContentView(list);
    setGrow(1);

    auto* header = new brls::Header();
    header->setTitle("app/mii"_i18n);
    list->addView(header);

    addMultilineLabel(list, tr("mii_note"));
    refreshControl = addActionCell(list, tr("browser_refresh"), [this] { refresh(); });

    status = addMultilineLabel(list, tr("mii_choose_global"));
    entries = new brls::Box(brls::Axis::COLUMN);
    list->addView(entries);

    tabsControl = addActionCell(list, "app/tabs"_i18n, focusTabs);
    list->setLastFocusedView(refreshControl);
    navigation::back(this, focusTabs, "app/tabs_hint"_i18n);
}

MiiBrowserScreen::~MiiBrowserScreen() {
    life->view = nullptr;
    session->cancel();
}

void MiiBrowserScreen::setProfile(const ProfileSelection& selection) {
    if (selection.state != ProfileSelectionState::Selected || !selection.account.key.valid())
        return;
    profile = selection;
    if (model.hasAccount && model.context.account.key == selection.account.key) {
        model.context.account = selection.account;
        return;
    }
    session->cancel();
    snapshot.reset();
    model.select(selection.account);
    initialLoad = true;
    dirty = true;
}

void MiiBrowserScreen::activateBrowser() {
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

void MiiBrowserScreen::deactivateBrowser() {
    if (!active)
        return;
    active = false;
    model.context.scroll = getContentOffsetY();
    if (model.state == MiiBrowserState::Loading) {
        model.suspend();
        session->cancel();
    }
}

void MiiBrowserScreen::willAppear(bool resetState) {
    brls::ScrollingFrame::willAppear(resetState);
    activateBrowser();
}

void MiiBrowserScreen::willDisappear(bool resetState) {
    deactivateBrowser();
    brls::ScrollingFrame::willDisappear(resetState);
}

void MiiBrowserScreen::refresh(bool begin) {
    if (!model.hasAccount) {
        render();
        return;
    }
    if (begin && model.state == MiiBrowserState::Loading)
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

void MiiBrowserScreen::render() {
    if (!active) {
        dirty = true;
        return;
    }
    dirty = false;

    const bool isLoading = (model.state == MiiBrowserState::Loading);
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
        status->setText(tr("mii_loading"));
        list->setLastFocusedView(tabsControl);
        return;
    }
    if (!model.hasAccount || model.state == MiiBrowserState::Unselected) {
        status->setText(tr("mii_choose_global"));
        list->setLastFocusedView(tabsControl);
        return;
    }
    if (model.state == MiiBrowserState::Error) {
        snapshot.reset();
        status->setText(errorMessage(model.status.error));
        list->setLastFocusedView(refreshControl);
        pendingFocus = (savedFocus == "tabs")
            ? static_cast<brls::View*>(tabsControl)
            : static_cast<brls::View*>(refreshControl);
        restore();
        return;
    }
    if (model.state == MiiBrowserState::Empty) {
        snapshot.reset();
        status->setText(tr("mii_empty"));
        list->setLastFocusedView(refreshControl);
        pendingFocus = (savedFocus == "tabs")
            ? static_cast<brls::View*>(tabsControl)
            : static_cast<brls::View*>(refreshControl);
        restore();
        return;
    }
    if (model.state != MiiBrowserState::Ready) {
        status->setText(tr("mii_choose_global"));
        list->setLastFocusedView(tabsControl);
        return;
    }

    snapshot = std::make_shared<const MiiRosterSummary>(model.summary);
    std::string headerNotice = tr("mii_readonly") + " (" + std::to_string(snapshot->residents.size()) + ")";
    if (snapshot->mapNamesWarning)
        headerNotice += "\n" + tr("mii_map_warning");
    if (snapshot->hasFieldWarnings)
        headerNotice += "\n" + tr("mii_warnings_notice");
    status->setText(headerNotice);

    auto loc = [](const std::string& k) { return brls::getStr("app/" + k); };
    Cell* firstCell = nullptr;
    Cell* matchedCell = nullptr;
    for (size_t i = 0; i < snapshot->residents.size(); ++i) {
        const auto& resident = snapshot->residents[i];
        const std::string focusKey = "mii_" + std::to_string(resident.slotNumber);

        auto* cell = new Cell();
        cell->setText(resident.stats.name);
        cell->setDetailText(FormatResidentSummary(resident, loc));
        cell->focused = [this, slotNum = resident.slotNumber, focusKey, cell] {
            model.context.selectedSlotNumber = slotNum;
            model.context.focusTarget = focusKey;
            entries->setLastFocusedView(cell);
            list->setLastFocusedView(entries);
        };
        cell->registerClickAction([this, i, slotNum = resident.slotNumber, focusKey, cell](brls::View*) {
            brls::Application::giveFocus(cell);
            model.context.selectedSlotNumber = slotNum;
            model.context.focusTarget = focusKey;
            model.context.scroll = getContentOffsetY();
            if (snapshot && i < snapshot->residents.size()) {
                navigation::push(new MiiDetailScreen(
                    snapshot, i, profile, requestExit, [this] { restore(); }));
            }
            return true;
        });
        entries->addView(cell);

        if (!firstCell)
            firstCell = cell;
        if (focusKey == savedFocus)
            matchedCell = cell;
    }

    if (matchedCell) {
        pendingFocus = matchedCell;
        entries->setLastFocusedView(matchedCell);
        list->setLastFocusedView(entries);
    } else if (firstCell) {
        entries->setLastFocusedView(firstCell);
        list->setLastFocusedView(entries);
    }

    restore();
}

void MiiBrowserScreen::restore() {
    restorePending = true;
    invalidate();
}

void MiiBrowserScreen::onLayout() {
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
