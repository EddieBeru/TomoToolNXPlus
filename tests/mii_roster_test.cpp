#include "mii_roster.h"
#include "support/mii_roster_fixtures.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <vector>

namespace {

using namespace MiiRoster;
using namespace MiiRosterFixtures;

std::vector<uint8_t> readFile(const char* path) {
    std::ifstream f(path, std::ios::binary);
    assert(f.good());
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

Localizer identityTr() {
    return [](const std::string& k) { return k; };
}

void testStructuralValidation() {
    // Empty & too small
    {
        std::vector<uint8_t> empty;
        auto r = Parse(empty);
        assert(!r && r.status.error == MiiReadError::InvalidHeader);
    }
    {
        std::vector<uint8_t> small(0x1f, 0);
        auto r = Parse(small);
        assert(!r && r.status.error == MiiReadError::InvalidHeader);
    }
    // Oversized (> MaxMiiBytes)
    {
        std::vector<uint8_t> huge(TextureBrowser::MaxMiiBytes + 1, 0);
        auto r = Parse(huge);
        assert(!r && r.status.error == MiiReadError::FileTooLarge);
    }
    // Bad magic
    {
        auto bytes = makeSyntheticMiiSavA();
        bytes[0] = 0xff;
        auto r = Parse(bytes);
        assert(!r && r.status.error == MiiReadError::InvalidHeader);
    }
    // Unaligned saveDataOff
    {
        auto bytes = makeSyntheticMiiSavA();
        writeU32(bytes, 8, 0x24u);
        auto r = Parse(bytes);
        assert(!r && r.status.error == MiiReadError::InvalidHeader);
    }
    // Sentinel type >= 33
    {
        auto bytes = makeSyntheticMiiSavA();
        writeU32(bytes, 0x20u, 0u);
        writeU32(bytes, 0x24u, 33u);
        auto r = Parse(bytes);
        assert(!r && r.status.error == MiiReadError::CorruptIndex);
    }
    // Entry before first sentinel
    {
        auto bytes = makeSyntheticMiiSavA();
        writeU32(bytes, 0x20u, 0x12345678u);
        writeU32(bytes, 0x24u, 0u);
        auto r = Parse(bytes);
        assert(!r && r.status.error == MiiReadError::CorruptIndex);
    }
    // Truncated payload
    {
        auto bytes = makeSyntheticMiiSavA();
        bytes.resize(bytes.size() - 64);
        auto r = Parse(bytes);
        assert(!r && r.status.error == MiiReadError::TruncatedPayload);
    }
    // Missing required Mii.Name.Name
    {
        std::vector<SavEntry> e = {{0x9999b7d9u, 3, 0, makeIntArrayPayload(std::vector<int32_t>(70, 0))}};
        auto r = Parse(serializeSav(e));
        assert(!r && r.status.error == MiiReadError::MissingRequiredField);
    }
    // TypeMismatch on Mii.Name.Name
    {
        std::vector<SavEntry> e = {{0x2499bfdau, 3, 0, makeIntArrayPayload(std::vector<int32_t>(70, 0))}};
        auto r = Parse(serializeSav(e));
        assert(!r && r.status.error == MiiReadError::TypeMismatch);
    }
    // Corrupt BinArray (CharInfoEx element length exceeds buffer)
    {
        std::vector<uint8_t> badBin(12, 0);
        writeU32(badBin, 0, 1u);
        writeU32(badBin, 4, 9999u);
        std::vector<SavEntry> e = {
            {0x2499bfdau, 29, 0, makeWStr32ArrayPayload(std::vector<std::string>(70))},
            {0x881ca27au, 19, 0, badBin},
        };
        auto r = Parse(serializeSav(e));
        assert(!r && r.status.error == MiiReadError::TruncatedPayload);
    }
}

void testAccountAAndAll7Sections() {
    const auto miiBytes = makeSyntheticMiiSavA();
    const auto mapBytes = makeSyntheticMapSavA();
    const auto miiSnapshot = miiBytes;
    const auto mapSnapshot = mapBytes;

    auto res = Parse(miiBytes, &mapBytes);
    assert(res);
    // Byte-for-byte immutability
    assert(miiBytes == miiSnapshot);
    assert(mapBytes == mapSnapshot);

    assert(res.value.residents.size() == 3);
    assert(res.value.mapNamesAvailable);
    assert(!res.value.mapNamesWarning);

    // Sparse slot ordering: 1, 14, 70 (deleted slot 6 & ghost slot 7 excluded)
    assert(res.value.residents[0].slotNumber == 1);
    assert(res.value.residents[1].slotNumber == 14);
    assert(res.value.residents[2].slotNumber == 70);

    const auto& lucia = res.value.residents[0];
    // 1. Stats (16 fields)
    assert(lucia.stats.name == "Lucía 🌟");
    assert(!lucia.stats.nameTextWarning);
    assert(lucia.stats.displayLevel() == 5);
    assert(lucia.stats.levelMeter == 82);
    assert(lucia.stats.money == 12500u);
    assert(lucia.stats.bdayDay == 14 && lucia.stats.bdayMonth == 3 && lucia.stats.bdayYear == 1998);
    assert(lucia.stats.directAge == -1);
    assert(lucia.stats.activeness == 8 && lucia.stats.audacity == 6 && lucia.stats.commonsense == 7);
    assert(lucia.stats.gaiety == 9 && lucia.stats.sociability == 10);
    assert(lucia.stats.bondMeter == 91);
    assert(lucia.stats.moodKnown && lucia.stats.moodKey == "good");
    assert(lucia.stats.fullness == 115);

    auto statsRows = FormatSectionRows(lucia, MiiSectionId::Stats, identityTr());
    assert(statsRows.size() == 16);

    // 2. Words
    assert(!lucia.words[0].isEmpty && lucia.words[0].kindKey == "greeting");
    assert(lucia.words[0].text == "¡Hola isla! 🌴");
    assert(lucia.words[0].howToCall == "O-la is-la");
    assert(lucia.words[3].isEmpty);
    auto wordRows = FormatSectionRows(lucia, MiiSectionId::Words, identityTr());
    assert(wordRows.size() == 12);

    // 3. Relations (Couple fighting with Kenji, Friend with Mia, Parent with orphan slot 21)
    assert(lucia.relations.size() == 3);
    assert(lucia.relations[0].otherSlotNumber == 14 && lucia.relations[0].isFight);
    assert(lucia.relations[0].tierLabel == "Still super in love?");
    assert(lucia.relations[0].sinceUnixSeconds == 1700000000ULL);
    assert(lucia.relations[1].otherSlotNumber == 21 && !lucia.relations[1].otherSlotOccupied);
    assert(lucia.relations[2].otherSlotNumber == 70 && lucia.relations[2].myTypeKey == "friend");
    auto relRows = FormatSectionRows(lucia, MiiSectionId::Relations, identityTr());
    assert(relRows.size() == 3);

    // 4. Belongings
    assert(!lucia.belongings.wornSlots[2].isEmpty && lucia.belongings.wornSlots[2].known);
    assert(lucia.belongings.wornSlots[2].label == "ABC sweatshirt");
    assert(!lucia.belongings.pocket[0].isEmpty && lucia.belongings.pocket[0].known);
    assert(!res.value.residents[1].belongings.coordinate.isEmpty);
    assert(res.value.residents[1].belongings.coordinate.known);
    auto blRows = FormatSectionRows(lucia, MiiSectionId::Belongings, identityTr());
    assert(blRows.size() == 1 + WornClothSlots + PocketSlotsPerMii);

    // 5. Habits (9 categories, 74 total, 9 owned + active)
    int totalHabits = 0, totalOwned = 0, totalActive = 0;
    for (const auto& c : lucia.habitCategories) {
        totalHabits += c.totalCount;
        totalOwned += c.ownedCount;
        totalActive += c.activeCount;
    }
    assert(totalHabits == 74);
    assert(totalOwned == 9);
    assert(totalActive == 9);
    auto habitRows = FormatSectionRows(lucia, MiiSectionId::Habits, identityTr());
    assert(habitRows.size() == 9);

    // 6. Housing (Casa Sol from Map.sav, room conflict with Kenji, Mia unhoused)
    assert(lucia.housing.housed && lucia.housing.houseNameFromMap);
    assert(lucia.housing.houseName == "Casa Sol");
    assert(lucia.housing.hasRoomConflict);
    assert(lucia.housing.housemates.size() == 1 && lucia.housing.housemates[0].slotNumber == 14);
    assert(!res.value.residents[2].housing.housed);
    auto housingRows = FormatSectionRows(lucia, MiiSectionId::Housing, identityTr());
    assert(housingRows.size() >= 3);

    // 7. Social Summary
    assert(lucia.social.totalConnections == 3);
    assert(lucia.social.romance == 1 && lucia.social.friends == 1 && lucia.social.family == 1);
    assert(lucia.social.activeFights == 1);
    auto socialRows = FormatSectionRows(lucia, MiiSectionId::Social, identityTr());
    assert(socialRows.size() == 7 + 3);

    auto overviews = FormatSectionOverviews(lucia, identityTr());
    assert(overviews.size() == SectionCount);
    assert(!FormatResidentSummary(lucia, identityTr()).empty());
}

void testAccountBAndDegradations() {
    const auto miiB = makeSyntheticMiiSavB();
    // Corrupt Map.sav must NOT invalidate Mii.sav
    std::vector<uint8_t> corruptMap = {0xde, 0xad, 0xbe, 0xef};
    auto res = Parse(miiB, &corruptMap);
    assert(res);
    assert(!res.value.mapNamesAvailable);
    assert(res.value.mapNamesWarning);
    assert(res.value.hasFieldWarnings);
    assert(res.value.residents.size() == 1);

    const auto& sofia = res.value.residents[0];
    assert(sofia.slotNumber == 3);
    assert(sofia.stats.name == "Sofía");
    assert(!sofia.stats.moodKnown);
    assert(!sofia.stats.bdayDayValid());
    assert(!sofia.stats.bdayMonthValid());
    assert(!sofia.words[0].kindKnown);
    assert(!sofia.relations[0].myTypeKnown);
    assert(!sofia.belongings.wornSlots[2].known);
    assert(!sofia.belongings.pocket[0].known);
    assert(sofia.habitCategories[0].entries[0].state == HabitState::Unknown);
    assert(sofia.housing.housed && !sofia.housing.houseNameFromMap);
    assert(sofia.hasWarnings);

    auto housingRows = FormatSectionRows(sofia, MiiSectionId::Housing, identityTr());
    assert(housingRows[0].hasWarning);
    assert(housingRows[0].primaryValue.find("#4") != std::string::npos);
}

void testEmptyAndFullIsland() {
    auto emptyRes = Parse(makeEmptyIslandMiiSav());
    assert(emptyRes);
    assert(emptyRes.value.residents.empty());

    auto fullRes = Parse(makeFullIslandMiiSav());
    assert(fullRes);
    assert(fullRes.value.residents.size() == 70);
    assert(fullRes.value.residents.front().slotNumber == 1);
    assert(fullRes.value.residents.back().slotNumber == 70);
}

void testRepositorySeeds() {
    auto seedMii = readFile("data/seed_Mii.bin");
    auto seedMap = readFile("data/seed_Map.bin");
    auto res = Parse(seedMii, &seedMap);
    assert(res);
    assert(res.value.saveVersion == 1u);
    assert(res.value.mapNamesAvailable);
    assert(res.value.residents.size() <= MaxSlots);
}

} // namespace

int main() {
    testStructuralValidation();
    testAccountAAndAll7Sections();
    testAccountBAndDegradations();
    testEmptyAndFullIsland();
    testRepositorySeeds();
    std::printf("mii_roster_test: OK\n");
    return 0;
}
