#include "mii_roster.h"
#include "support/mii_roster_fixtures.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace {

using namespace MiiRoster;
using namespace MiiRosterFixtures;

class FakeReadSource final : public TextureBrowser::ReadSource {
public:
    TextureBrowser::Status openStatus{};
    std::map<std::string, TextureBrowser::Result<std::vector<uint8_t>>> files;
    int openCalls = 0;
    int closeCalls = 0;
    bool isOpen = false;
    std::vector<std::string> readRefs;

    TextureBrowser::Result<std::vector<TextureBrowser::Account>> accounts(const TextureBrowser::Cancel&) override {
        return {};
    }
    TextureBrowser::Status open(TextureBrowser::AccountKey, const TextureBrowser::Cancel& cancel) override {
        ++openCalls;
        if (cancel && cancel())
            return {TextureBrowser::Error::Cancelled};
        if (openStatus)
            isOpen = true;
        return openStatus;
    }
    TextureBrowser::Result<std::vector<TextureBrowser::File>> enumerate(const TextureBrowser::Cancel&) override {
        return {};
    }
    TextureBrowser::Result<std::vector<uint8_t>> read(
        const std::string& ref, size_t limit, const TextureBrowser::Cancel& cancel) override {
        readRefs.push_back(ref);
        if (cancel && cancel())
            return {{}, {TextureBrowser::Error::Cancelled}};
        auto it = files.find(ref);
        if (it == files.end())
            return {{}, {TextureBrowser::Error::ShortRead}};
        if (it->second && it->second.value.size() > limit)
            return {{}, {TextureBrowser::Error::Limit}};
        return it->second;
    }
    void close() override {
        if (isOpen) {
            isOpen = false;
            ++closeCalls;
        }
    }
};

TextureBrowser::Account makeAccount(uint64_t hi, const std::string& name) {
    TextureBrowser::Account a;
    a.key.words = {hi, 1};
    a.name = name;
    return a;
}

void testLoadFromReadSourceSuccessAndMapFallback() {
    FakeReadSource src;
    src.files["/Mii.sav"] = {makeSyntheticMiiSavA(), {}};
    src.files["/Map.sav"] = {makeSyntheticMapSavA(), {}};

    auto acc = makeAccount(10, "AccountA");
    auto res = LoadFromReadSource(src, acc.key, [] { return false; });
    assert(res);
    assert(res.value.residents.size() == 3);
    assert(res.value.mapNamesAvailable);
    assert(src.openCalls == 1 && src.closeCalls == 1 && !src.isOpen);
    assert(src.readRefs.size() == 2 && src.readRefs[0] == "/Mii.sav" && src.readRefs[1] == "/Map.sav");

    // Missing /Map.sav still succeeds with mapNamesWarning
    src.files.erase("/Map.sav");
    auto resNoMap = LoadFromReadSource(src, acc.key, [] { return false; });
    assert(resNoMap);
    assert(resNoMap.value.residents.size() == 3);
    assert(!resNoMap.value.mapNamesAvailable);
    assert(resNoMap.value.mapNamesWarning);
    assert(src.closeCalls == 2 && !src.isOpen);
}

void testLoadErrorsAndCloseGuarantee() {
    auto acc = makeAccount(20, "AccountB");

    // Invalid account key
    {
        FakeReadSource src;
        auto res = LoadFromReadSource(src, {}, [] { return false; });
        assert(!res && res.status.error == MiiReadError::NoAccountSelected);
        assert(src.openCalls == 0);
    }
    // NoSave
    {
        FakeReadSource src;
        src.openStatus = {TextureBrowser::Error::NoSave};
        auto res = LoadFromReadSource(src, acc.key, [] { return false; });
        assert(!res && res.status.error == MiiReadError::NoSave);
        assert(src.closeCalls == 0);
    }
    // Metadata / Open failures
    {
        FakeReadSource src;
        src.openStatus = {TextureBrowser::Error::Metadata, 0x123};
        auto res = LoadFromReadSource(src, acc.key, [] { return false; });
        assert(!res && res.status.error == MiiReadError::AccountMetadataFailed && res.status.platform == 0x123);
    }
    {
        FakeReadSource src;
        src.openStatus = {TextureBrowser::Error::Open, 0x456};
        auto res = LoadFromReadSource(src, acc.key, [] { return false; });
        assert(!res && res.status.error == MiiReadError::ReadOnlyOpenFailed && res.status.platform == 0x456);
    }
    // ShortRead on /Mii.sav guarantees close()
    {
        FakeReadSource src;
        auto res = LoadFromReadSource(src, acc.key, [] { return false; });
        assert(!res && res.status.error == MiiReadError::ShortRead);
        assert(src.openCalls == 1 && src.closeCalls == 1 && !src.isOpen);
    }
    // Oversized /Mii.sav -> FileTooLarge & close()
    {
        FakeReadSource src;
        src.files["/Mii.sav"] = {std::vector<uint8_t>(TextureBrowser::MaxMiiBytes + 1, 0), {}};
        auto res = LoadFromReadSource(src, acc.key, [] { return false; });
        assert(!res && res.status.error == MiiReadError::FileTooLarge);
        assert(src.closeCalls == 1 && !src.isOpen);
    }
    // Cancelled
    {
        FakeReadSource src;
        auto res = LoadFromReadSource(src, acc.key, [] { return true; });
        assert(!res && res.status.error == MiiReadError::Cancelled);
        assert(src.openCalls == 0);
    }
}

void testModelStateMachine() {
    MiiBrowserModel m;
    assert(m.state == MiiBrowserState::Unselected);
    assert(!m.hasAccount);

    // Select Account A -> Loading, generation increments
    m.select(makeAccount(1, "A"));
    assert(m.state == MiiBrowserState::Loading);
    assert(m.hasAccount);
    const uint64_t g1 = m.generation;
    assert(g1 == 1);

    // Suspend while loading -> Suspended, generation increments
    m.suspend();
    assert(m.state == MiiBrowserState::Suspended);
    assert(m.generation == g1 + 1);

    // Resume -> Loading, generation increments
    assert(m.resume());
    assert(m.state == MiiBrowserState::Loading);
    assert(!m.resume()); // already loading

    // Accept empty island -> Empty (not Error)
    auto emptyRes = Parse(makeEmptyIslandMiiSav());
    m.accept(std::move(emptyRes));
    assert(m.state == MiiBrowserState::Empty);
    assert(m.summary.residents.empty());

    // Refresh -> Loading -> Ready
    m.loading();
    assert(m.state == MiiBrowserState::Loading);
    m.accept(Parse(makeSyntheticMiiSavA()));
    assert(m.state == MiiBrowserState::Ready);
    assert(m.summary.residents.size() == 3);

    // Cancelled result does not overwrite Ready state
    Result<MiiRosterSummary> cancelled;
    cancelled.status = {MiiReadError::Cancelled};
    m.accept(std::move(cancelled));
    assert(m.state == MiiBrowserState::Ready);
    assert(m.summary.residents.size() == 3);

    // Switch to Account B clears Account A residents immediately
    m.select(makeAccount(2, "B"));
    assert(m.state == MiiBrowserState::Loading);
    assert(m.summary.residents.empty());

    // Error state -> retry -> Ready
    Result<MiiRosterSummary> fail;
    fail.status = {MiiReadError::NoSave};
    m.accept(std::move(fail));
    assert(m.state == MiiBrowserState::Error);
    assert(m.status.error == MiiReadError::NoSave);

    m.loading();
    m.accept(Parse(makeSyntheticMiiSavB()));
    assert(m.state == MiiBrowserState::Ready);
    assert(m.summary.residents.size() == 1);

    // Clear with invalid account -> Unselected
    m.select({});
    assert(m.state == MiiBrowserState::Unselected);
    assert(!m.hasAccount);
}

} // namespace

int main() {
    testLoadFromReadSourceSuccessAndMapFallback();
    testLoadErrorsAndCloseGuarantee();
    testModelStateMachine();
    std::printf("mii_browser_context_test: OK\n");
    return 0;
}
