#include "support/player_profile_fixtures.h"
#include "support/texture_read_shim.h"

#include "app.hpp"
#include "exit_menu.hpp"
#include "navigation.hpp"
#include "player_browser_screen.hpp"
#include "profile_header.hpp"
#include "profile_selector.hpp"
#include "texture_browser_screen.hpp"
#include "texture_browser_session.hpp"
#include "texture_preview_view.hpp"
#include "texture_settings_screen.hpp"
#include "texture_settings_store.h"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <zstd.h>

#include <sys/stat.h>
#include <unistd.h>

namespace {
Result pselCreateRc = 0;
Result pselShowRc = 0;
AccountUid pselUid{{1, 0}};
u32 pselAvatarSize = 0;
Result pselImageSizeRc = 0;
Result pselImageLoadRc = 0;
} // namespace

Result pselUiCreate(PselUiSettings* ui, PselUiMode mode) {
    assert(mode == PselUiMode_UserSelector);
    *ui = {};
    return pselCreateRc;
}
void pselUiSetAllowUserCreation(PselUiSettings* ui, bool enabled) { ui->allowCreation = enabled; }
void pselUiSetNetworkServiceRequired(PselUiSettings* ui, bool enabled) { ui->networkRequired = enabled; }
void pselUiSetSkipButtonEnabled(PselUiSettings* ui, bool enabled) { ui->skipButton = enabled; }
Result pselUiShow(PselUiSettings* ui, AccountUid* uid) {
    assert(!ui->allowCreation && !ui->networkRequired && ui->skipButton);
    *uid = pselUid;
    return pselShowRc;
}
Result accountProfileGetImageSize(AccountProfile*, u32* size) {
    *size = pselAvatarSize;
    return pselImageSizeRc;
}
Result accountProfileLoadImage(AccountProfile*, void* out, size_t size, u32* actual) {
    assert(size == pselAvatarSize);
    if (size > 0)
        std::memset(out, 0x42, size);
    *actual = static_cast<u32>(size);
    return pselImageLoadRc;
}

namespace {
struct Queue {
    std::mutex mutex;
    std::condition_variable ready;
    std::vector<TextureBrowser::Session::Completion> items;

    void push(TextureBrowser::Session::Completion c) {
        std::lock_guard<std::mutex> lock(mutex);
        items.push_back(std::move(c));
        ready.notify_all();
    }

    size_t drain() {
        std::unique_lock<std::mutex> lock(mutex);
        assert(ready.wait_for(lock, std::chrono::seconds(5), [&] { return !items.empty(); }));
        auto next = std::move(items);
        items.clear();
        lock.unlock();
        for (auto& c : next)
            c();
        return next.size();
    }

    size_t drainAvailable() {
        std::unique_lock<std::mutex> lock(mutex);
        auto next = std::move(items);
        items.clear();
        lock.unlock();
        for (auto& c : next)
            c();
        return next.size();
    }
};

std::vector<uint8_t> compressZstd(const std::vector<uint8_t>& raw) {
    std::vector<uint8_t> out(ZSTD_compressBound(raw.size()));
    const size_t n = ZSTD_compress(out.data(), out.size(), raw.data(), raw.size(), 1);
    assert(!ZSTD_isError(n));
    out.resize(n);
    return out;
}

template <typename T>
void collectViews(brls::View* root, std::vector<T*>& out) {
    if (!root)
        return;
    if (auto* typed = dynamic_cast<T*>(root))
        out.push_back(typed);
    if (auto* box = dynamic_cast<brls::Box*>(root)) {
        for (auto* child : box->children)
            collectViews(child, out);
    } else if (auto* scroll = dynamic_cast<brls::ScrollingFrame*>(root)) {
        collectViews(scroll->getContentView(), out);
    } else if (auto* frame = dynamic_cast<brls::AppletFrame*>(root)) {
        collectViews(frame->getView("brls/applet_frame/hint_box"), out);
        collectViews(frame->getContentView(), out);
    }
}

template <typename T>
std::vector<T*> findAll(brls::View* root) {
    std::vector<T*> out;
    collectViews(root, out);
    return out;
}

brls::DetailCell* findCell(brls::View* root, const std::string& title) {
    for (auto* cell : findAll<brls::DetailCell>(root)) {
        if (cell->titleLabel.getFullText() == title)
            return cell;
    }
    return nullptr;
}

brls::DetailCell* findCellByDetail(brls::View* root, const std::string& detail) {
    for (auto* cell : findAll<brls::DetailCell>(root)) {
        if (cell->detailLabel.getFullText() == detail)
            return cell;
    }
    return nullptr;
}

bool press(brls::ControllerButton button) {
    for (auto* view = brls::Application::getCurrentFocus(); view; view = view->getParent()) {
        const auto it = view->actions.find(button);
        if (it != view->actions.end()) {
            assert(!it->second.repeating);
            if (it->second.callback(view))
                return true;
        }
    }
    return false;
}

bool pressOn(brls::View* target, brls::ControllerButton button) {
    assert(target && target->isFocusable());
    brls::Application::giveFocus(target);
    assert(brls::Application::getCurrentFocus() == target);
    return press(button);
}

void tap(brls::View* target) {
    assert(target && target->isFocusable() && target->clickAction);
    assert(target->clickAction(target));
}

brls::View* topContent() {
    assert(!brls::Application::activities.empty());
    return brls::Application::activities.back()->getContentView();
}

borealis_app::TextureBrowserScreen* findBrowser(brls::View* root) {
    auto list = findAll<borealis_app::TextureBrowserScreen>(root);
    return list.empty() ? nullptr : list.front();
}

borealis_app::PlayerBrowserScreen* findPlayerBrowser(brls::View* root) {
    auto list = findAll<borealis_app::PlayerBrowserScreen>(root);
    return list.empty() ? nullptr : list.front();
}

void setBrowserScrollableHeight(brls::View* root) {
    if (auto* browser = findBrowser(root)) {
        if (browser->getContentView())
            browser->getContentView()->setHeight(4000.0f);
    }
    if (auto* playerBrowser = findPlayerBrowser(root)) {
        if (playerBrowser->getContentView())
            playerBrowser->getContentView()->setHeight(4000.0f);
    }
}

void populateAccountSaves() {
    saves.clear();
    const auto mainA = compressZstd(std::vector<uint8_t>(131072, 0x11));
    const auto mainB = compressZstd(std::vector<uint8_t>(131072, 0x77));
    const auto rectCanvas = compressZstd(std::vector<uint8_t>(256 * 512 * 4, 0x55));
    const auto squareCanvas = compressZstd(std::vector<uint8_t>(256 * 256 * 4, 0x33));

    for (size_t i = 0; i < 100; ++i) {
        char stem[32];
        std::snprintf(stem, sizeof(stem), "UgcFood%03zu", i);
        saves[1][std::string("/Ugc/") + stem + ".ugctex.zs"] = mainA;
    }
    // Ambiguous auxiliary case names trigger associationWarning while main entry remains accessible.
    saves[1]["/Ugc/UgcFood001.canvas.zs"] = rectCanvas;
    saves[1]["/Ugc/UgcFood001.CANVAS.zs"] = rectCanvas;
    // Square canvas: detail preview prioritizes main ugctex.zs (512x512).
    saves[1]["/Ugc/UgcFood010.canvas.zs"] = squareCanvas;
    // Corrupt canvas: detail preview falls back to main with fallback warning.
    saves[1]["/Ugc/UgcFood011.canvas.zs"] = {0x01, 0x02, 0x03};
    // Corrupt main texture: detail preview reports local error and clears image.
    saves[1]["/Ugc/UgcFood012.ugctex.zs"] = {0x01, 0x02, 0x03};
    // Target entry below the first screen with valid rectangular 256x512 canvas.
    saves[1]["/Ugc/UgcFood049.canvas.zs"] = rectCanvas;
    saves[1]["/Player.sav"] = PlayerProfileFixtures::makeSyntheticPlayerSavA(true, true);

    saves[2]["/Ugc/UgcFood000.ugctex.zs"] = mainB;
    saves[2]["/Ugc/UgcFood049.ugctex.zs"] = mainB;
    saves[2]["/Player.sav"] = PlayerProfileFixtures::makeSyntheticPlayerSavB(true);
}

void resetShimState() {
    calls.clear();
    failure = 0;
    activeAccount = 1;
    activeRef.clear();
    partialBytes = 65536;
    filesOpen = 0;
    dirsOpen = 0;
    filesystemOpen = false;
    beforeRead = {};
    metadataIndex = 0;
    metadataDone = false;
    dirDone = false;
    noSave = false;
    missing = false;
    interrupted = false;
    metadataFailure = 0;
    readOnlyFailure = 0;
    profileFailure = 0;
    records = 1;
    recordIndex = 0;
    accountCount = 1;
    fileSize = 131072;
    shortRead = false;
    changeSize = false;
    sizeCalls = 0;
    sizeFailure = 0;
    readFailure = 0;

    pselCreateRc = 0;
    pselShowRc = 0;
    pselUid = {{1, 0}};
    pselAvatarSize = 128;
    pselImageSizeRc = 0;
    pselImageLoadRc = 0;
}

void verifySettingsFlow(const std::string& tempDir, const borealis_app::ProfileSelection& profile) {
    const std::string cfgPath = tempDir + "/config.ini";
    {
        std::ofstream out(cfgPath, std::ios::binary);
        out << "# user comment\n"
            << "theme=dark\n"
            << "canvas_shape=square\n"
            << "encoder=custom\n"
            << "bc1_mode=auto\n"
            << "fit_mode=contain\n"
            << "matte=123456\n"
            << "custom_flag=preserve_me\n";
    }

    int exitCalls = 0;
    auto* screen = borealis_app::createTextureSettingsScreen(cfgPath, [&] { ++exitCalls; }, profile);
    borealis_app::navigation::push(screen);
    assert(topContent() == screen);

    auto headers = findAll<borealis_app::ProfileHeader>(screen);
    assert(headers.size() == 1);
    auto* shapeCell = findCell(screen, "preferences/shape");
    auto* encoderCell = findCell(screen, "preferences/encoder");
    auto* bc1Cell = findCell(screen, "preferences/bc1");
    auto* bgCell = findCell(screen, "preferences/background");
    auto* saveCell = findCell(screen, "preferences/save");
    auto* reloadCell = findCell(screen, "preferences/reload");
    auto* backCell = findCell(screen, "hints/back");
    assert(shapeCell && encoderCell && bc1Cell && bgCell && saveCell && reloadCell && backCell);
    assert(bc1Cell->isFocusable());
    assert(bgCell->isFocusable());
    assert(!saveCell->isFocusable()); // Clean initially.
    assert(bgCell->detailLabel.getFullText() == "preferences/custom_color:#123456");

    // Controller: open Shape dropdown, press B to cancel without mutating draft.
    assert(pressOn(shapeCell, brls::BUTTON_A));
    auto* dropdown = dynamic_cast<brls::Dropdown*>(topContent());
    assert(dropdown && dropdown->optionCells.size() == 3);
    assert(!press(brls::BUTTON_START));
    assert(!press(brls::BUTTON_LT));
    assert(press(brls::BUTTON_B));
    assert(topContent() == screen);
    assert(!saveCell->isFocusable());
    assert(brls::Application::getCurrentFocus() == shapeCell);

    // Controller: select PCA encoder -> disables Bc1Mode, enables Save, keeps custom matte.
    assert(pressOn(encoderCell, brls::BUTTON_A));
    dropdown = dynamic_cast<brls::Dropdown*>(topContent());
    assert(dropdown);
    assert(pressOn(dropdown->optionCells[1], brls::BUTTON_A));
    assert(topContent() == screen);
    assert(!bc1Cell->isFocusable());
    assert(saveCell->isFocusable());
    assert(bgCell->detailLabel.getFullText() == "preferences/custom_color:#123456");

    // Dirty Back prompt -> Keep editing, then Plus -> Save keeps screen open.
    assert(press(brls::BUTTON_B));
    auto* dialog = dynamic_cast<brls::Dialog*>(topContent());
    assert(dialog && dialog->message == "preferences/leave_confirm");
    assert(dialog->buttons.size() == 3);
    assert(pressOn(dialog->buttons[2], brls::BUTTON_A)); // keep_editing
    assert(topContent() == screen);
    assert(saveCell->isFocusable());

    // Plus -> Save only persists draft and creates verified backup.
    assert(press(brls::BUTTON_START));
    dialog = dynamic_cast<brls::Dialog*>(topContent());
    assert(dialog && dialog->message == "app/exit_menu_prompt");
    assert(pressOn(dialog->buttons[1], brls::BUTTON_A)); // save_only
    assert(topContent() == screen);
    assert(!saveCell->isFocusable());

    // Plus -> Save again when clean notifies no_pending_changes.
    brls::Application::notifications.clear();
    assert(press(brls::BUTTON_START));
    dialog = dynamic_cast<brls::Dialog*>(topContent());
    assert(dialog);
    assert(pressOn(dialog->buttons[1], brls::BUTTON_A));
    assert(!brls::Application::notifications.empty());
    assert(brls::Application::notifications.back() == "app/no_pending_changes");

    // Touch: change Encoder back to Custom -> Bc1Mode re-enables, dirty Exit menu offers Save & Exit.
    tap(encoderCell);
    dropdown = dynamic_cast<brls::Dropdown*>(topContent());
    assert(dropdown);
    tap(dropdown->optionCells[0]); // custom
    assert(bc1Cell->isFocusable());
    assert(saveCell->isFocusable());

    // Trigger external conflict before Save to verify error dialog focus recovery via B and Back button.
    {
        std::ofstream out(cfgPath, std::ios::app);
        out << "# external edit\n";
    }
    tap(saveCell);
    dialog = dynamic_cast<brls::Dialog*>(topContent());
    assert(dialog && dialog->message.find("preferences/conflict_error") != std::string::npos);
    assert(press(brls::BUTTON_B));
    assert(topContent() == screen);
    assert(brls::Application::getCurrentFocus() == reloadCell);

    // Reload dirty confirmation via touch, then Discard via dirty Exit dialog.
    tap(reloadCell);
    dialog = dynamic_cast<brls::Dialog*>(topContent());
    assert(dialog && dialog->message == "preferences/reload_confirm");
    tap(dialog->buttons[0]); // cancel reload
    assert(topContent() == screen);

    auto* exitMenuCell = findCell(screen, "app/exit_menu");
    assert(exitMenuCell);
    tap(exitMenuCell);
    dialog = dynamic_cast<brls::Dialog*>(topContent());
    assert(dialog);
    tap(dialog->buttons[2]); // exit_loader -> opens exit_dirty dialog
    dialog = dynamic_cast<brls::Dialog*>(topContent());
    assert(dialog && dialog->message == "app/exit_dirty");
    tap(dialog->buttons[2]); // discard_exit
    assert(exitCalls == 1);

    // Reload clean config and verify Save & Return from dirty Back prompt.
    tap(reloadCell);
    dialog = dynamic_cast<brls::Dialog*>(topContent());
    assert(dialog && dialog->message == "preferences/reload_confirm");
    tap(dialog->buttons[1]); // confirm reload
    assert(topContent() == screen);
    assert(!saveCell->isFocusable());
    tap(shapeCell);
    dropdown = dynamic_cast<brls::Dropdown*>(topContent());
    tap(dropdown->optionCells[2]); // tv
    tap(backCell);
    dialog = dynamic_cast<brls::Dialog*>(topContent());
    assert(dialog && dialog->message == "preferences/leave_confirm");
    tap(dialog->buttons[0]); // save_return
    assert(topContent() != screen);

    // Verify backup exists and unrelated keys were preserved before corrupting cfgPath.
    {
        std::ifstream backupIn(cfgPath + ".borealis-backup.0");
        assert(backupIn.good());
        std::ifstream finalIn(cfgPath);
        std::string finalData((std::istreambuf_iterator<char>(finalIn)), std::istreambuf_iterator<char>());
        assert(finalData.find("theme=dark") != std::string::npos);
        assert(finalData.find("custom_flag=preserve_me") != std::string::npos);
    }

    // Corrupt config on disk and verify Plus->Save reports save_unavailable without saving.
    {
        std::ofstream out(cfgPath, std::ios::binary);
        out << "canvas_shape=not_a_valid_shape\n";
    }
    auto* invalidScreen = borealis_app::createTextureSettingsScreen(cfgPath, [&] { ++exitCalls; }, profile);
    borealis_app::navigation::push(invalidScreen);
    assert(press(brls::BUTTON_START));
    dialog = dynamic_cast<brls::Dialog*>(topContent());
    assert(dialog);
    assert(pressOn(dialog->buttons[1], brls::BUTTON_A));
    dialog = dynamic_cast<brls::Dialog*>(topContent());
    assert(dialog && dialog->message == "app/save_unavailable");
    assert(pressOn(dialog->buttons[0], brls::BUTTON_A));
    assert(topContent() == invalidScreen);
    assert(press(brls::BUTTON_B));
}
} // namespace

int main() {
    resetShimState();
    populateAccountSaves();
    const auto snapshot = saves;

    borealis_app::navigation::initialize();

    // Verify initial profile selection and root construction.
    const auto initialProfile = borealis_app::selectNativeProfile();
    assert(initialProfile.state == borealis_app::ProfileSelectionState::Selected);
    assert(initialProfile.account.key.words[0] == 1);
    assert(initialProfile.avatarJpeg.size() == 128);

    Queue queue;
    auto session = std::make_shared<TextureBrowser::Session>(
        TextureBrowser::createReadSource(),
        [&](TextureBrowser::Session::Completion c) { queue.push(std::move(c)); });

    bool closing = false;
    int exitRequests = 0;
    auto* root = borealis_app::createRoot(session, [&] {
        ++exitRequests;
        if (closing)
            return;
        closing = true;
        session->stop();
    }, initialProfile);
    borealis_app::navigation::push(root);
    assert(topContent() == root);
    setBrowserScrollableHeight(root);

    // 1. Controller-only flow: error states & recovery without restart.
    auto* browser = findBrowser(root);
    assert(browser);
    auto* refreshCell = findCell(browser, "app/browser_refresh");
    auto* settingsCell = findCell(browser, "app/settings");
    assert(refreshCell && settingsCell);
    queue.drain(); // initial load succeeds with 100 entries.
    assert(findAll<brls::DetailCell>(browser).size() >= 103);

    // Verify simultaneous namesWarning + associationWarning while all 100 entries remain accessible.
    auto labels = findAll<brls::Label>(browser);
    bool sawNamesWarn = false, sawAssocWarn = false;
    for (auto* lbl : labels) {
        if (lbl->getFullText().find("app/browser_names_warning") != std::string::npos)
            sawNamesWarn = true;
        if (lbl->getFullText().find("app/browser_association_warning") != std::string::npos)
            sawAssocWarn = true;
    }
    assert(sawNamesWarn && sawAssocWarn);

    // Inject NoSave, Metadata, Open, UgcMissing, Enumeration, ShortRead and Empty; recover with Retry.
    noSave = true;
    assert(pressOn(refreshCell, brls::BUTTON_A));
    queue.drain();
    assert(findCellByDetail(browser, "UgcFood049") == nullptr);
    noSave = false;

    metadataFailure = 4;
    assert(pressOn(refreshCell, brls::BUTTON_A));
    queue.drain();
    metadataFailure = 0;

    readOnlyFailure = 4;
    assert(pressOn(refreshCell, brls::BUTTON_A));
    queue.drain();
    readOnlyFailure = 0;

    missing = true;
    assert(pressOn(refreshCell, brls::BUTTON_A));
    queue.drain();
    missing = false;

    interrupted = true;
    assert(pressOn(refreshCell, brls::BUTTON_A));
    queue.drain();
    interrupted = false;

    // Empty collection then recover back to 100 entries.
    auto savedAccount1 = saves[1];
    saves[1].clear();
    saves[1]["/Player.sav"] = savedAccount1["/Player.sav"];
    assert(pressOn(refreshCell, brls::BUTTON_A));
    queue.drain();
    assert(findCellByDetail(browser, "UgcFood000") == nullptr);
    saves[1] = savedAccount1;

    assert(pressOn(refreshCell, brls::BUTTON_A));
    // Double-activation guard: repeating refresh while CatalogLoading must not duplicate.
    assert(!refreshCell->isFocusable());
    browser->activateBrowser();
    queue.drain();
    assert(queue.drainAvailable() == 0);
    setBrowserScrollableHeight(root);
    assert(findCellByDetail(browser, "UgcFood049") != nullptr);

    // 2. Profile header change/cancel/failure isolation via controller.
    auto headerControls = findAll<borealis_app::ProfileHeader>(root);
    assert(headerControls.size() == 1);
    auto* headerControl = headerControls.front();

    // Cancelled selection retains Account 1 and its 100 entries.
    pselUid = {{0, 0}};
    assert(pressOn(headerControl, brls::BUTTON_A));
    assert(queue.drainAvailable() == 0);
    assert(findCellByDetail(browser, "UgcFood099") != nullptr);

    // Failed selection retains Account 1 and notifies.
    brls::Application::notifications.clear();
    pselShowRc = 5;
    assert(pressOn(headerControl, brls::BUTTON_A));
    assert(!brls::Application::notifications.empty());
    assert(queue.drainAvailable() == 0);
    assert(findCellByDetail(browser, "UgcFood099") != nullptr);
    pselShowRc = 0;

    // Confirmed switch to Account 2 loads Account 2, then switch back to Account 1.
    pselUid = {{2, 0}};
    pselAvatarSize = 0; // missing avatar uses placeholder fallback
    assert(pressOn(headerControl, brls::BUTTON_A));
    queue.drain();
    assert(findCellByDetail(browser, "UgcFood099") == nullptr);
    assert(findCellByDetail(browser, "UgcFood049") != nullptr);

    pselUid = {{1, 0}};
    pselAvatarSize = 128;
    assert(pressOn(headerControl, brls::BUTTON_A));
    queue.drain();
    setBrowserScrollableHeight(root);
    assert(findCellByDetail(browser, "UgcFood099") != nullptr);

    // 3. 20 detail/back cycles on entry #49 preserving focus and scroll position (controller-only).
    for (int cycle = 0; cycle < 20; ++cycle) {
        auto* entry49 = findCellByDetail(browser, "UgcFood049");
        assert(entry49);
        browser->setContentOffsetY(640.0f, false);
        assert(pressOn(entry49, brls::BUTTON_A));
        auto* detail = topContent();
        assert(detail != root);
        auto* backBtn = findCell(detail, "hints/back");
        auto* retryBtn = findCell(detail, "app/browser_refresh");
        auto previews = findAll<borealis_app::TexturePreviewView>(detail);
        assert(backBtn && retryBtn && previews.size() == 1);
        assert(brls::Application::getCurrentFocus() == backBtn);
        assert(!retryBtn->isFocusable());
        assert(!previews[0]->isFocusable());
        // ZL/ZR cannot switch root tabs behind detail.
        assert(!press(brls::BUTTON_LT));
        assert(!press(brls::BUTTON_RT));

        queue.drain();
        assert(retryBtn->isFocusable());
        assert(previews[0]->isFocusable());
        assert(previews[0]->getOriginalImageWidth() == 256);
        assert(previews[0]->getOriginalImageHeight() == 512);
        const int beforeDraws = brls::Application::nvg.drawCalls;
        previews[0]->draw(&brls::Application::nvg, 0, 0, 800, 320, {}, nullptr);
        assert(brls::Application::nvg.drawCalls > beforeDraws);

        if (cycle == 0) {
            // Test Retry moves focus to Back before disabling and keeps focus on Back when done.
            assert(pressOn(retryBtn, brls::BUTTON_A));
            assert(brls::Application::getCurrentFocus() == backBtn);
            assert(!retryBtn->isFocusable());
            queue.drain();
            assert(retryBtn->isFocusable());
            assert(brls::Application::getCurrentFocus() == backBtn);
        }

        assert(press(brls::BUTTON_B));
        assert(topContent() == root);
        assert(browser->getContentOffsetY() == 640.0f);
        assert(brls::Application::getCurrentFocus() == entry49);
    }

    // Check square-canvas priority (UgcFood010 -> main 512x512), corrupt canvas fallback (UgcFood011),
    // and corrupt main texture clearing previous texture and disabling preview focus (UgcFood012).
    {
        auto* entry10 = findCellByDetail(browser, "UgcFood010");
        assert(entry10);
        assert(pressOn(entry10, brls::BUTTON_A));
        auto* detail10 = topContent();
        queue.drain();
        auto previews = findAll<borealis_app::TexturePreviewView>(detail10);
        assert(previews[0]->getOriginalImageWidth() == 512 && previews[0]->getOriginalImageHeight() == 512);
        assert(press(brls::BUTTON_B));

        auto* entry11 = findCellByDetail(browser, "UgcFood011");
        assert(entry11);
        assert(pressOn(entry11, brls::BUTTON_A));
        auto* detail11 = topContent();
        queue.drain();
        bool sawFallback = false;
        for (auto* lbl : findAll<brls::Label>(detail11)) {
            if (lbl->getFullText().find("app/browser_fallback") != std::string::npos)
                sawFallback = true;
        }
        assert(sawFallback);
        assert(press(brls::BUTTON_B));

        auto* entry12 = findCellByDetail(browser, "UgcFood012");
        assert(entry12);
        assert(pressOn(entry12, brls::BUTTON_A));
        auto* detail12 = topContent();
        queue.drain();
        auto previews12 = findAll<borealis_app::TexturePreviewView>(detail12);
        assert(previews12[0]->getTexture() == 0 && !previews12[0]->isFocusable());
        assert(press(brls::BUTTON_B));
    }

    // 4. 10 tab switches (SC-003) retaining entry #49 & scroll, plus CatalogSuspended resume once.
    auto* entry49 = findCellByDetail(browser, "UgcFood049");
    assert(entry49);
    browser->setContentOffsetY(640.0f, false);
    brls::Application::giveFocus(entry49);
    for (int sw = 0; sw < 5; ++sw) {
        assert(press(brls::BUTTON_RT)); // to Mii
        assert(queue.drainAvailable() == 0);
        assert(press(brls::BUTTON_LT)); // back to Textures
        assert(queue.drainAvailable() == 0);
        assert(browser->getContentOffsetY() == 640.0f);
        assert(brls::Application::getCurrentFocus() == entry49);
    }

    // Suspend while CatalogLoading -> returning issues exactly one reload.
    {
        std::mutex m;
        std::condition_variable cv;
        bool entered = false, released = false;
        beforeRead = [&] {
            std::unique_lock<std::mutex> lock(m);
            entered = true;
            cv.notify_all();
            assert(cv.wait_for(lock, std::chrono::seconds(5), [&] { return released; }));
        };
        assert(pressOn(refreshCell, brls::BUTTON_A));
        {
            std::unique_lock<std::mutex> lock(m);
            assert(cv.wait_for(lock, std::chrono::seconds(5), [&] { return entered; }));
        }
        assert(press(brls::BUTTON_RT)); // Suspends CatalogLoading on leave and cancels session.
        {
            std::lock_guard<std::mutex> lock(m);
            released = true;
            cv.notify_all();
        }
        beforeRead = {};
        assert(findCellByDetail(browser, "UgcFood049") == nullptr);
        assert(press(brls::BUTTON_LT)); // Resumes with single new reload.
        queue.drain();
        assert(queue.drainAvailable() == 0);
        setBrowserScrollableHeight(root);
        assert(browser->getContentOffsetY() == 640.0f);
        entry49 = findCellByDetail(browser, "UgcFood049");
        assert(entry49 && brls::Application::getCurrentFocus() == entry49);
    }

    // Disappearing entry fallback on refresh.
    saves[1].erase("/Ugc/UgcFood049.ugctex.zs");
    saves[1].erase("/Ugc/UgcFood049.canvas.zs");
    assert(pressOn(refreshCell, brls::BUTTON_A));
    queue.drain();
    setBrowserScrollableHeight(root);
    assert(findCellByDetail(browser, "UgcFood049") == nullptr);
    auto* entry50 = findCellByDetail(browser, "UgcFood050");
    assert(entry50 && brls::Application::getCurrentFocus() == entry50);
    saves[1] = savedAccount1;
    assert(pressOn(refreshCell, brls::BUTTON_A));
    queue.drain();
    setBrowserScrollableHeight(root);

    // 5. Touch-only flow: profile header tap, tab tap, refresh tap, entry tap, retry tap, back tap, settings tap.
    auto sidebarItems = findAll<brls::SidebarItem>(root);
    assert(sidebarItems.size() == 5);
    auto* tab0 = sidebarItems[0];
    auto* tab1 = sidebarItems[1];
    pselUid = {{2, 0}};
    tap(headerControl);
    queue.drain();
    assert(findCellByDetail(browser, "UgcFood099") == nullptr);
    pselUid = {{1, 0}};
    tap(headerControl);
    queue.drain();
    setBrowserScrollableHeight(root);
    assert(findCellByDetail(browser, "UgcFood099") != nullptr);

    tap(tab0);
    assert(queue.drainAvailable() == 0);
    tap(tab1);
    assert(queue.drainAvailable() == 0);

    tap(refreshCell);
    queue.drain();
    setBrowserScrollableHeight(root);
    entry49 = findCellByDetail(browser, "UgcFood049");
    assert(entry49);
    browser->setContentOffsetY(640.0f, false);
    tap(entry49);
    auto* detailTouch = topContent();
    assert(detailTouch != root);
    queue.drain();
    auto* retryTouch = findCell(detailTouch, "app/browser_refresh");
    auto* backTouch = findCell(detailTouch, "hints/back");
    assert(retryTouch && backTouch);
    tap(retryTouch);
    queue.drain();
    tap(backTouch);
    assert(topContent() == root);
    assert(browser->getContentOffsetY() == 640.0f);
    assert(brls::Application::getCurrentFocus() == entry49);

    // Alternating input: touch enter settings -> controller dropdown/B -> discard back -> controller Plus exit menu.
    tap(findCell(browser, "app/settings"));
    auto* settingsFromBrowser = topContent();
    assert(settingsFromBrowser != root);
    auto* shapeFromBrowser = findCell(settingsFromBrowser, "preferences/shape");
    assert(shapeFromBrowser);
    assert(pressOn(shapeFromBrowser, brls::BUTTON_A));
    auto* dd = dynamic_cast<brls::Dropdown*>(topContent());
    assert(dd);
    assert(pressOn(dd->optionCells[1], brls::BUTTON_A));
    assert(press(brls::BUTTON_B));
    auto* leaveDlg = dynamic_cast<brls::Dialog*>(topContent());
    assert(leaveDlg && leaveDlg->message == "preferences/leave_confirm");
    assert(pressOn(leaveDlg->buttons[1], brls::BUTTON_A)); // discard
    assert(topContent() == root);
    assert(browser->getContentOffsetY() == 640.0f);

    // 6. Player tab (M1): real read-only Player.sav inspection (US1, US3, US2), controller + touch, isolation, recovery.
    {
        auto* unselectedPlayer = new borealis_app::PlayerBrowserScreen(session, [] {});
        auto labelsUnselected = findAll<brls::Label>(unselectedPlayer);
        bool sawChooseGlobal = false;
        for (auto* lbl : labelsUnselected) {
            if (lbl->getFullText() == "app/player_choose_global")
                sawChooseGlobal = true;
        }
        assert(sawChooseGlobal);
        unselectedPlayer->freeView();
    }

    // Activate tab 3 (Jugador): starts at initial account 1 load for Player.sav.
    assert(press(brls::BUTTON_RT)); // 1->2
    assert(press(brls::BUTTON_RT)); // 2->3 (Jugador)
    queue.drain();
    auto* playerScreen = findPlayerBrowser(root);
    assert(playerScreen);
    setBrowserScrollableHeight(root);

    // No sample entries may remain; all 15 attribute rows + secondary labels exist with Account 1 values.
    assert(findCell(playerScreen, "app/sample_3") == nullptr);
    auto* pNameCell = findCell(playerScreen, "app/player_field_name");
    auto* pIslandCell = findCell(playerScreen, "app/player_field_island");
    auto* pMoneyCell = findCell(playerScreen, "app/player_field_money");
    auto* pCurrencyCell = findCell(playerScreen, "app/player_field_currency");
    auto* pWishesCell = findCell(playerScreen, "app/player_field_wishes");
    auto* pDonationCell = findCell(playerScreen, "app/player_field_donation");
    auto* pRefreshCell = findCell(playerScreen, "app/browser_refresh");
    auto* pTabsCell = findCell(playerScreen, "app/tabs");
    assert(pNameCell && pIslandCell && pMoneyCell && pCurrencyCell && pWishesCell && pDonationCell && pRefreshCell && pTabsCell);
    assert(findAll<brls::DetailCell>(playerScreen).size() == 17); // Refresh + 15 attributes + Tabs
    assert(pNameCell->detailLabel.getFullText() == "Álex 😀");
    assert(pIslandCell->detailLabel.getFullText() == "Ítaca");
    assert(pMoneyCell->detailLabel.getFullText() == "15 000,00 €");
    assert(pCurrencyCell->detailLabel.getFullText() == "€ · app/player_currency_euro");
    assert(pWishesCell->detailLabel.getFullText() == "105");

    // Verify multiline secondary metadata + descriptions exist inline without opening child screens on A/tap.
    bool sawPron = false, sawLibWishes = false, sawDesc = false;
    for (auto* lbl : findAll<brls::Label>(playerScreen)) {
        const auto txt = lbl->getFullText();
        if (txt.find("A-lex") != std::string::npos && txt.find("app/player_lang_eues") != std::string::npos)
            sawPron = true;
        if (txt.find("100 / 268") != std::string::npos)
            sawLibWishes = true;
        if (txt.find("app/player_desc_donation") != std::string::npos)
            sawDesc = true;
    }
    assert(sawPron && sawLibWishes && sawDesc);

    assert(pressOn(pWishesCell, brls::BUTTON_A));
    assert(topContent() == root);
    tap(pDonationCell);
    assert(topContent() == root);
    assert(brls::Application::getCurrentFocus() == pDonationCell);

    // B returns focus to Jugador sidebar item.
    assert(press(brls::BUTTON_B));
    assert(brls::Application::getCurrentFocus() == sidebarItems[3]);

    // Refocus Donation, scroll down, and verify 10 consecutive tab switches (SC-003) + Plus dialog cancel preserve focus & scroll.
    brls::Application::giveFocus(pDonationCell);
    playerScreen->setContentOffsetY(640.0f, false);
    for (int sw = 0; sw < 5; ++sw) {
        assert(press(brls::BUTTON_RT)); // 3->4 (Map)
        assert(queue.drainAvailable() == 0);
        assert(press(brls::BUTTON_LT)); // 4->3 (Jugador)
        assert(queue.drainAvailable() == 0);
        assert(playerScreen->getContentOffsetY() == 640.0f);
        assert(brls::Application::getCurrentFocus() == pDonationCell);
    }
    assert(press(brls::BUTTON_START));
    auto* pExitDlg = dynamic_cast<brls::Dialog*>(topContent());
    assert(pExitDlg && pExitDlg->message == "app/exit_menu_prompt");
    assert(pressOn(pExitDlg->buttons[0], brls::BUTTON_A)); // cancel
    assert(topContent() == root);
    assert(playerScreen->getContentOffsetY() == 640.0f);
    assert(brls::Application::getCurrentFocus() == pDonationCell);

    // Profile switch to Account 2 isolates data completely; cancelled/failed profile selection retains Account 1.
    pselUid = {{0, 0}};
    assert(pressOn(headerControl, brls::BUTTON_A));
    assert(queue.drainAvailable() == 0);
    assert(findCell(playerScreen, "app/player_field_name")->detailLabel.getFullText() == "Álex 😀");

    brls::Application::notifications.clear();
    pselShowRc = 5;
    assert(pressOn(headerControl, brls::BUTTON_A));
    assert(!brls::Application::notifications.empty());
    assert(queue.drainAvailable() == 0);
    assert(findCell(playerScreen, "app/player_field_name")->detailLabel.getFullText() == "Álex 😀");
    pselShowRc = 0;

    pselUid = {{2, 0}};
    assert(pressOn(headerControl, brls::BUTTON_A));
    queue.drain();
    assert(findCell(playerScreen, "app/player_field_name")->detailLabel.getFullText() == "Sofía");
    assert(findCell(playerScreen, "app/player_field_island")->detailLabel.getFullText() == "Avalon");
    assert(findCell(playerScreen, "app/player_field_money")->detailLabel.getFullText() == "8 500,50");
    assert(findCell(playerScreen, "app/player_field_currency")->detailLabel.getFullText().find("0x12345678") != std::string::npos);
    assert(findCell(playerScreen, "app/player_field_tt_penalty")->detailLabel.getFullText() == "app/player_penalty_active");

    bool sawWarningsNotice = false, sawUnavailableWishes = false, sawFutureDonation = false;
    for (auto* lbl : findAll<brls::Label>(playerScreen)) {
        const auto txt = lbl->getFullText();
        if (txt.find("app/player_warnings_notice") != std::string::npos)
            sawWarningsNotice = true;
        if (txt.find("app/player_meta_wishes_unavailable") != std::string::npos)
            sawUnavailableWishes = true;
        if (txt.find("app/player_donation_future") != std::string::npos)
            sawFutureDonation = true;
    }
    assert(sawWarningsNotice && sawUnavailableWishes && sawFutureDonation);

    // Switch back to Account 1 via touch.
    pselUid = {{1, 0}};
    tap(headerControl);
    queue.drain();
    setBrowserScrollableHeight(root);
    assert(findCell(playerScreen, "app/player_field_name")->detailLabel.getFullText() == "Álex 😀");

    // US3: Error injection, double-activation guard, suspend/resume during Loading, and retry recovery on Jugador tab.
    noSave = true;
    assert(pressOn(pRefreshCell, brls::BUTTON_A));
    assert(!pRefreshCell->isFocusable());
    playerScreen->activateBrowser(); // no duplicate request
    queue.drain();
    assert(queue.drainAvailable() == 0);
    assert(findCell(playerScreen, "app/player_field_name") == nullptr);
    noSave = false;

    auto savedPlayer1 = saves[1];
    saves[1]["/Player.sav"] = {0x01, 0x02, 0x03}; // corrupt
    assert(pressOn(pRefreshCell, brls::BUTTON_A));
    queue.drain();
    assert(findCell(playerScreen, "app/player_field_name") == nullptr);

    saves[1]["/Player.sav"] = std::vector<uint8_t>(TextureBrowser::MaxPlayerBytes + 1, 0); // oversized > 8 MiB
    tap(pRefreshCell);
    queue.drain();
    assert(findCell(playerScreen, "app/player_field_name") == nullptr);

    saves[1] = savedPlayer1;
    tap(pRefreshCell); // touch recovery back to valid 15 attributes
    queue.drain();
    setBrowserScrollableHeight(root);
    assert(findCell(playerScreen, "app/player_field_name")->detailLabel.getFullText() == "Álex 😀");

    // Suspend while Loading -> leave to Map -> return issues single reload restoring focus & scroll.
    {
        pWishesCell = findCell(playerScreen, "app/player_field_wishes");
        assert(pWishesCell);
        brls::Application::giveFocus(pWishesCell);
        playerScreen->setContentOffsetY(360.0f, false);

        std::mutex m;
        std::condition_variable cv;
        bool entered = false, released = false;
        beforeRead = [&] {
            std::unique_lock<std::mutex> lock(m);
            entered = true;
            cv.notify_all();
            assert(cv.wait_for(lock, std::chrono::seconds(5), [&] { return released; }));
        };
        assert(pressOn(pRefreshCell, brls::BUTTON_A));
        {
            std::unique_lock<std::mutex> lock(m);
            assert(cv.wait_for(lock, std::chrono::seconds(5), [&] { return entered; }));
        }
        assert(press(brls::BUTTON_RT)); // leave Jugador -> suspend + cancel
        {
            std::lock_guard<std::mutex> lock(m);
            released = true;
            cv.notify_all();
        }
        beforeRead = {};
        assert(press(brls::BUTTON_LT)); // return to Jugador -> single reload
        queue.drain();
        assert(queue.drainAvailable() == 0);
        setBrowserScrollableHeight(root);
        pWishesCell = findCell(playerScreen, "app/player_field_wishes");
        assert(pWishesCell);
        assert(playerScreen->getContentOffsetY() == 360.0f);
        assert(brls::Application::getCurrentFocus() == pWishesCell);
    }

    // Isolated synthetic config tests for TextureSettings (Save/Backup/Conflict/ExitMenu).
    char tmpTemplate[] = "/tmp/tomotool-ui-screens.XXXXXX";
    char* tmpDir = mkdtemp(tmpTemplate);
    assert(tmpDir != nullptr);
    verifySettingsFlow(tmpDir, initialProfile);
    std::remove((std::string(tmpDir) + "/config.ini.borealis-backup.0").c_str());
    std::remove((std::string(tmpDir) + "/config.ini").c_str());
    rmdir(tmpDir);

    // Exit menu from root via Plus and sidebar Exit button, plus double-exit guard.
    assert(topContent() == root);
    assert(press(brls::BUTTON_START));
    auto* exitDlg = dynamic_cast<brls::Dialog*>(topContent());
    assert(exitDlg && exitDlg->message == "app/exit_menu_prompt");
    assert(pressOn(exitDlg->buttons[0], brls::BUTTON_A)); // cancel
    assert(topContent() == root);

    auto buttons = findAll<brls::Button>(root);
    assert(buttons.size() == 1);
    tap(buttons[0]); // sidebar Save/Exit button
    exitDlg = dynamic_cast<brls::Dialog*>(topContent());
    assert(exitDlg);
    tap(exitDlg->buttons[2]); // exit_loader
    assert(exitRequests == 1 && closing);

    assert(press(brls::BUTTON_START));
    exitDlg = dynamic_cast<brls::Dialog*>(topContent());
    assert(exitDlg);
    tap(exitDlg->buttons[2]);
    assert(exitRequests == 2 && closing);

    session->stop();
    for (int i = 0; i < 500 && !session->finished(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(session->finished());
    queue.drainAvailable();

    brls::Application::clearActivities();

    assert(saves == snapshot);
    assert(!filesystemOpen && filesOpen == 0 && dirsOpen == 0);
    for (const auto& call : calls) {
        assert(call.find("write") == std::string::npos);
        assert(call.find("commit") == std::string::npos);
        assert(call.find("create") == std::string::npos);
        assert(call.find("delete") == std::string::npos);
        assert(call.find("rename") == std::string::npos);
    }

    std::cout << "borealis ui screens (controller/touch/lifecycle/settings): OK\n";
    return 0;
}
