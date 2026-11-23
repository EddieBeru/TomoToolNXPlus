#include "player_profile.h"
#include "support/player_profile_fixtures.h"
#include "support/texture_browser_fixtures.h"
#include <cassert>
#include <iostream>

int main() {
    Fixtures::Source source;
    const TextureBrowser::AccountKey keyA{{1, 0}};
    const TextureBrowser::AccountKey keyB{{2, 0}};
    const TextureBrowser::AccountKey keyMissingPlayer{{3, 0}};
    const TextureBrowser::AccountKey keyOversized{{4, 0}};
    source.accountFiles[1]["/Player.sav"] = PlayerProfileFixtures::makeSyntheticPlayerSavA();
    source.accountFiles[2]["/Player.sav"] = PlayerProfileFixtures::makeSyntheticPlayerSavB();
    source.accountFiles[3]["/Ugc/UgcFood000.ugctex.zs"] = std::vector<uint8_t>(64, 0); // no /Player.sav -> ShortRead
    source.accountFiles[4]["/Player.sav"] = std::vector<uint8_t>(TextureBrowser::MaxPlayerBytes + 1, 0); // -> Limit / FileTooLarge

    const auto cancelNever = [] { return false; };

    // 1. Successful loads for A and B verify close() on every exit and zero write operations.
    const auto resA = PlayerProfile::LoadFromReadSource(source, keyA, cancelNever);
    assert(bool(resA));
    assert(resA.value.playerName == "Álex 😀");
    assert(resA.value.islandName == "Ítaca");
    assert(resA.value.moneyCents == 1500000u);
    assert(source.operations.back() == "close");

    // 2. Open-stage failure mappings preserve close().
    struct OpenCase { TextureBrowser::Error in; PlayerProfile::PlayerReadError out; };
    const OpenCase openCases[] = {
        {TextureBrowser::Error::NoSave, PlayerProfile::PlayerReadError::NoSave},
        {TextureBrowser::Error::Metadata, PlayerProfile::PlayerReadError::AccountMetadataFailed},
        {TextureBrowser::Error::Open, PlayerProfile::PlayerReadError::ReadOnlyOpenFailed},
        {TextureBrowser::Error::Cancelled, PlayerProfile::PlayerReadError::Cancelled},
    };
    for (const auto& c : openCases) {
        source.failure = {c.in, 77u};
        const auto r = PlayerProfile::LoadFromReadSource(source, keyA, cancelNever);
        assert(!r);
        assert(r.status.error == c.out);
        assert(source.operations.back() == "close");
    }
    source.failure = {};

    // Read-stage failures: ShortRead (missing /Player.sav) and FileTooLarge (> MaxPlayerBytes).
    {
        const auto rShort = PlayerProfile::LoadFromReadSource(source, keyMissingPlayer, cancelNever);
        assert(!rShort && rShort.status.error == PlayerProfile::PlayerReadError::ShortRead);
        assert(source.operations.back() == "close");

        const auto rLimit = PlayerProfile::LoadFromReadSource(source, keyOversized, cancelNever);
        assert(!rLimit && rLimit.status.error == PlayerProfile::PlayerReadError::FileTooLarge);
        assert(source.operations.back() == "close");
    }

    // Invalid AccountKey & cancelled callback.
    {
        const auto rNoAcct = PlayerProfile::LoadFromReadSource(source, {}, cancelNever);
        assert(!rNoAcct && rNoAcct.status.error == PlayerProfile::PlayerReadError::NoAccountSelected);
        assert(source.operations.back() == "close");

        const auto rCancelled = PlayerProfile::LoadFromReadSource(source, keyA, [] { return true; });
        assert(!rCancelled && rCancelled.status.error == PlayerProfile::PlayerReadError::Cancelled);
        assert(source.operations.back() == "close");
    }

    for (const auto& op : source.operations) {
        assert(op == "open-read" || op == "read" || op == "close");
    }

    // 3. PlayerBrowserModel state machine transitions, suspend/resume, generation isolation, error recovery.
    PlayerProfile::PlayerBrowserModel model;
    assert(model.state == PlayerProfile::PlayerBrowserState::Unselected);
    assert(!model.hasAccount);

    TextureBrowser::Account acctA{keyA, "Cuenta A", 1, false};
    TextureBrowser::Account acctB{keyB, "Cuenta B", 2, false};

    model.select(acctA);
    assert(model.hasAccount);
    assert(model.state == PlayerProfile::PlayerBrowserState::Loading);
    const uint64_t gen1 = model.generation;
    assert(gen1 == 1);

    // Suspend on tab switch while Loading, preserving focus/scroll.
    model.context.focusTarget = "attr_11";
    model.context.selectedAttrIndex = 11;
    model.context.scroll = 320.0f;
    model.suspend();
    assert(model.state == PlayerProfile::PlayerBrowserState::Suspended);
    assert(model.generation > gen1);

    // Resume issues one reload with a new generation and preserves context.
    assert(model.resume());
    assert(!model.resume()); // cannot resume twice
    assert(model.state == PlayerProfile::PlayerBrowserState::Loading);
    assert(model.context.focusTarget == "attr_11");
    assert(model.context.scroll == 320.0f);

    model.accept(resA);
    assert(model.state == PlayerProfile::PlayerBrowserState::Ready);
    assert(model.summary.playerName == "Álex 😀");

    // Suspend when already Ready leaves state intact.
    model.suspend();
    assert(model.state == PlayerProfile::PlayerBrowserState::Ready);
    assert(!model.resume());

    // Switching to Account B immediately clears Account A's summary and bumps generation.
    const uint64_t genBeforeSwitch = model.generation;
    model.select(acctB);
    assert(model.state == PlayerProfile::PlayerBrowserState::Loading);
    assert(model.generation > genBeforeSwitch);
    assert(model.summary.playerName.empty());
    assert(!model.summary.moneyPresent);
    assert(model.context.scroll == 0.0f);

    const auto resB = PlayerProfile::LoadFromReadSource(source, keyB, cancelNever);
    assert(bool(resB));
    model.accept(resB);
    assert(model.state == PlayerProfile::PlayerBrowserState::Ready);
    assert(model.summary.playerName == "Sofía");
    assert(model.summary.islandName == "Avalon");

    // Error state transitions and recovery via loading() + accept(ok).
    PlayerProfile::Result<PlayerProfile::PlayerProfileSummary> failRes;
    failRes.status = {PlayerProfile::PlayerReadError::NoSave, 0};
    model.loading();
    assert(model.state == PlayerProfile::PlayerBrowserState::Loading);
    model.accept(failRes);
    assert(model.state == PlayerProfile::PlayerBrowserState::Error);
    assert(model.summary.playerName.empty());
    model.loading();
    assert(model.state == PlayerProfile::PlayerBrowserState::Loading);
    model.accept(resA);
    assert(model.state == PlayerProfile::PlayerBrowserState::Ready);

    std::cout << "player browser context & model: OK\n";
    return 0;
}
