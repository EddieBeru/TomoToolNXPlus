#include "player_profile.h"
#include "support/player_profile_fixtures.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <vector>

namespace {

PlayerProfile::Localizer echoLocalizer() {
    return [](const std::string& key) { return key; };
}

void checkStructuralValidation() {
    // 1. Hashes match TomoToolNX save_editor.cpp / seed_Player.bin.
    assert(PlayerProfile::Hash("Player.Name") == 0xc7fb279bu);
    assert(PlayerProfile::Hash("Player.HowToCallName") == 0xbd8c4da1u);
    assert(PlayerProfile::Hash("Player.NameRegionLanguageID") == 0xb3229c6bu);
    assert(PlayerProfile::Hash("Player.IslandName") == 0xd46df986u);
    assert(PlayerProfile::Hash("Player.HowToCallIslandName") == 0xc6482c14u);
    assert(PlayerProfile::Hash("Player.IslandNameRegionLanguageID") == 0x7bbff932u);
    assert(PlayerProfile::Hash("Player.Money") == 0x365fab1fu);
    assert(PlayerProfile::Hash("Player.Currency") == 0xdc88f139u);
    assert(PlayerProfile::Hash("Player.BootNum") == 0x2f19f777u);
    assert(PlayerProfile::Hash("Player.SkinColorIndex") == 0xafbdf7dcu);
    assert(PlayerProfile::Hash("Liberation.FountainLevel") == 0xa4e65f79u);
    assert(PlayerProfile::Hash("Player.Region") == 0x10631cf8u);
    assert(PlayerProfile::Hash("Player.LastDonationGameTime") == 0xbba9c090u);
    assert(PlayerProfile::Hash("Liberation.WishInfo.WishIdValue") == 0xf91e67beu);
    assert(PlayerProfile::Hash("Liberation.WishInfo.IsLiberated") == 0xe38c7611u);

    // 2. Empty and undersized buffers (< 0x20).
    assert(PlayerProfile::Parse({}).status.error == PlayerProfile::PlayerReadError::InvalidHeader);
    assert(PlayerProfile::Parse(std::vector<uint8_t>(31, 0)).status.error == PlayerProfile::PlayerReadError::InvalidHeader);

    // 3. Oversized buffer (> 8 MiB).
    {
        std::vector<uint8_t> tooBig(TextureBrowser::MaxPlayerBytes + 1, 0);
        assert(PlayerProfile::Parse(tooBig).status.error == PlayerProfile::PlayerReadError::FileTooLarge);
    }

    // 4. Bad magic.
    {
        auto b = PlayerProfileFixtures::makeSyntheticPlayerSavA();
        b[0] = 0x99;
        assert(PlayerProfile::Parse(b).status.error == PlayerProfile::PlayerReadError::InvalidHeader);
    }

    // 5. Invalid saveDataOff (< 0x20, > size, or unaligned % 8 != 0).
    {
        auto b = PlayerProfileFixtures::makeSyntheticPlayerSavA();
        auto smallOff = b;
        PlayerProfileFixtures::writeU32(smallOff, 8, 0x18u);
        assert(PlayerProfile::Parse(smallOff).status.error == PlayerProfile::PlayerReadError::InvalidHeader);

        auto pastEnd = b;
        PlayerProfileFixtures::writeU32(pastEnd, 8, static_cast<uint32_t>(pastEnd.size() + 8));
        assert(PlayerProfile::Parse(pastEnd).status.error == PlayerProfile::PlayerReadError::InvalidHeader);

        auto unaligned = b;
        PlayerProfileFixtures::writeU32(unaligned, 8, 0x24u);
        assert(PlayerProfile::Parse(unaligned).status.error == PlayerProfile::PlayerReadError::CorruptIndex);
    }

    // 6. Entry before any type sentinel.
    {
        auto b = PlayerProfileFixtures::makeSyntheticPlayerSavA();
        PlayerProfileFixtures::writeU32(b, 0x20, 0x12345678u);
        assert(PlayerProfile::Parse(b).status.error == PlayerProfile::PlayerReadError::CorruptIndex);
    }

    // 7. Invalid type sentinel >= 33.
    {
        auto b = PlayerProfileFixtures::makeSyntheticPlayerSavA();
        PlayerProfileFixtures::writeU32(b, 0x20, 0u);
        PlayerProfileFixtures::writeU32(b, 0x24, 33u);
        assert(PlayerProfile::Parse(b).status.error == PlayerProfile::PlayerReadError::CorruptIndex);
    }

    // 8. Truncated heap payload and heap offset < saveDataOff.
    {
        auto b = PlayerProfileFixtures::makeSyntheticPlayerSavA();
        auto truncated = b;
        truncated.pop_back();
        assert(PlayerProfile::Parse(truncated).status.error == PlayerProfile::PlayerReadError::TruncatedPayload);

        uint32_t saveDataOff = static_cast<uint32_t>(b[8]) |
                               (static_cast<uint32_t>(b[9]) << 8) |
                               (static_cast<uint32_t>(b[10]) << 16) |
                               (static_cast<uint32_t>(b[11]) << 24);
        bool patched = false;
        for (size_t pos = 0x20; pos + 8 <= saveDataOff; pos += 8) {
            uint32_t h = static_cast<uint32_t>(b[pos]) |
                         (static_cast<uint32_t>(b[pos + 1]) << 8) |
                         (static_cast<uint32_t>(b[pos + 2]) << 16) |
                         (static_cast<uint32_t>(b[pos + 3]) << 24);
            if (h == 0xc7fb279bu) {
                PlayerProfileFixtures::writeU32(b, pos + 4, 0x20u);
                patched = true;
                break;
            }
        }
        assert(patched);
        assert(PlayerProfile::Parse(b).status.error == PlayerProfile::PlayerReadError::TruncatedPayload);
    }

    // 9. Duplicate hash in tracked entries.
    {
        auto entries = PlayerProfileFixtures::baseEntriesA();
        entries.push_back({0x365fab1fu, 20, 999u, {}});
        auto b = PlayerProfileFixtures::serializeSav(entries);
        assert(PlayerProfile::Parse(b).status.error == PlayerProfile::PlayerReadError::CorruptIndex);
    }

    // 10. Missing required field (e.g. Player.Money removed).
    {
        auto entries = PlayerProfileFixtures::baseEntriesA();
        entries.erase(entries.begin() + 6);
        auto b = PlayerProfileFixtures::serializeSav(entries);
        assert(PlayerProfile::Parse(b).status.error == PlayerProfile::PlayerReadError::MissingRequiredField);
    }

    // 11. Type mismatch on required scalar/string field.
    {
        auto entries = PlayerProfileFixtures::baseEntriesA();
        entries[0].type = 20;
        entries[0].inlineRaw = 42;
        entries[0].payload.clear();
        auto b = PlayerProfileFixtures::serializeSav(entries);
        assert(PlayerProfile::Parse(b).status.error == PlayerProfile::PlayerReadError::TypeMismatch);
    }

    // 12. Input immutability check (byte-for-byte identical after parsing).
    {
        const auto original = PlayerProfileFixtures::makeSyntheticPlayerSavA(true, true);
        std::vector<uint8_t> copy = original;
        const auto parsed = PlayerProfile::Parse(copy);
        assert(bool(parsed));
        assert(copy == original);
    }
}

void checkAll15AttributesAndEdgeCases() {
    constexpr int64_t kRefClock = 1700259200LL; // 3 days (3 * 86400s) after 1700000000
    const auto tr = echoLocalizer();

    // Account A: all 15 attributes, UTF-16LE accents + surrogate emoji, DT_WStr64 pronunciation, 100/268 wishes, 3d donation.
    {
        const auto rawA = PlayerProfileFixtures::makeSyntheticPlayerSavA(true, true);
        const auto parsed = PlayerProfile::Parse(rawA);
        assert(bool(parsed));
        const auto& s = parsed.value;
        assert(!s.hasFieldWarnings);
        assert(s.playerName == "Álex 😀");
        assert(s.howToCallName == "A-lex");
        assert(s.nameLanguage.known && s.nameLanguage.code == "EUes");
        assert(s.islandName == "Ítaca");
        assert(s.howToCallIslandName == "I-ta-ca");
        assert(s.islandLanguage.known && s.islandLanguage.code == "EUes");
        assert(s.moneyCents == 1500000u);
        assert(s.currency.known && s.currency.code == "Euro" && s.currency.symbol == "€");
        assert(s.bootCount == 42u);
        assert(s.skinToneIndex == 2u && s.skinToneValid());
        assert(s.bdayDay == 15 && s.bdayDayValid());
        assert(s.bdayMonth == 8 && s.bdayMonthValid());
        assert(s.bdayYear == 1998);
        assert(s.islandSizeRaw == 3 && s.islandSizeValid());
        assert(s.fountainLevel == 12u);
        assert(s.wishes.countPresent && s.wishes.wishesMade == 105u);
        assert(s.wishes.liberatedAvailable && s.wishes.liberatedCount == 100 && s.wishes.totalCatalogWishes == 268);
        assert(s.region.known && s.region.code == "Europe");
        assert(s.ttPenaltyPresent && s.ttPenaltyRaw == 0ULL);
        assert(s.lastDonationPresent && s.lastDonationSeconds == 1700000000ULL);

        const auto views = PlayerProfile::FormatAttributes(s, tr, kRefClock);
        assert(views.size() == 15);
        assert(views[0].focusKey == "attr_0" && views[0].primaryValue == "Álex 😀");
        assert(views[0].secondaryMetadata.find("A-lex") != std::string::npos);
        assert(views[0].secondaryMetadata.find("player_lang_eues") != std::string::npos);
        assert(views[1].primaryValue == "Ítaca");
        assert(views[1].secondaryMetadata.find("I-ta-ca") != std::string::npos);
        assert(views[2].primaryValue == "15 000,00 €");
        assert(views[2].secondaryMetadata.find("1500000") != std::string::npos);
        assert(views[3].primaryValue == "€ · player_currency_euro");
        assert(views[4].primaryValue == "42");
        assert(views[5].primaryValue == "3 / 6");
        assert(views[5].secondaryMetadata.find("#D2A07A") != std::string::npos);
        assert(views[6].primaryValue == "15");
        assert(views[7].primaryValue == "8");
        assert(views[8].primaryValue == "1998");
        assert(views[9].primaryValue == "90 x 64");
        assert(views[9].secondaryMetadata.find("3 / 4") != std::string::npos);
        assert(views[10].primaryValue == "12");
        assert(views[11].primaryValue == "105");
        assert(views[11].secondaryMetadata.find("100 / 268") != std::string::npos);
        assert(views[12].primaryValue == "player_region_europe");
        assert(views[13].primaryValue == "player_penalty_cleared");
        assert(views[14].primaryValue == PlayerProfile::FormatUtcTimestamp(1700000000ULL));
        assert(views[14].secondaryMetadata.find("3 player_donation_days_ago") != std::string::npos);
        for (size_t i = 0; i < views.size(); ++i)
            assert(!views[i].hasWarning && !views[i].description.empty() && !views[i].title.empty());
    }

    // Account B: empty pronunciation, unknown enums, out-of-range scalars, active TT, future donation, missing wishes table.
    {
        const auto rawB = PlayerProfileFixtures::makeSyntheticPlayerSavB(true);
        const auto parsed = PlayerProfile::Parse(rawB);
        assert(bool(parsed));
        const auto& s = parsed.value;
        assert(s.hasFieldWarnings);
        assert(!s.nameLanguage.known && s.nameLanguage.rawHash == 0xfeedfaceu);
        assert(!s.currency.known && s.currency.rawHash == 0x12345678u);
        assert(!s.region.known && s.region.rawHash == 0xdeadbeefu);
        assert(!s.skinToneValid() && !s.bdayDayValid() && !s.bdayMonthValid() && !s.islandSizeValid());
        assert(s.bdayYearSigned && s.bdayYear == -5);
        assert(!s.wishes.liberatedAvailable && s.wishes.tableWarning);

        const auto views = PlayerProfile::FormatAttributes(s, tr, kRefClock);
        assert(views[0].primaryValue == "Sofía" && views[0].hasWarning);
        assert(views[0].secondaryMetadata.find("0xfeedface") != std::string::npos);
        assert(views[1].secondaryMetadata.find("A-va-lon") != std::string::npos); // DT_WStr32 fallback
        assert(views[2].primaryValue == "8 500,50"); // unknown currency omits symbol
        assert(views[3].primaryValue.find("0x12345678") != std::string::npos && views[3].hasWarning);
        assert(views[5].primaryValue.find("player_value_out_of_range") != std::string::npos && views[5].hasWarning);
        assert(views[6].primaryValue.find("player_value_out_of_range") != std::string::npos && views[6].hasWarning);
        assert(views[7].primaryValue.find("player_value_out_of_range") != std::string::npos && views[7].hasWarning);
        assert(views[8].primaryValue == "-5");
        assert(views[9].primaryValue.find("player_value_non_standard") != std::string::npos && views[9].hasWarning);
        assert(views[11].secondaryMetadata == "player_meta_wishes_unavailable" && views[11].hasWarning);
        assert(views[12].primaryValue.find("0xdeadbeef") != std::string::npos && views[12].hasWarning);
        assert(views[13].primaryValue == "player_penalty_active");
        assert(views[14].secondaryMetadata == "player_donation_future" && views[14].hasWarning);
    }

    // Donation timestamp == 0 ("never collected") and == 1 day ago, plus lone surrogate degradation on name.
    {
        auto entries = PlayerProfileFixtures::baseEntriesA();
        entries[19].payload = PlayerProfileFixtures::makeUInt64Payload(12345ULL); // TTPenalty
        entries[20].payload = PlayerProfileFixtures::makeUInt64Payload(0ULL);     // Donation = never
        // Put lone high surrogate in Player.Name[0..1]
        entries[0].payload[0] = 0x00;
        entries[0].payload[1] = 0xd8;
        entries[0].payload[2] = 'X';
        entries[0].payload[3] = 0x00;
        const auto parsed = PlayerProfile::Parse(PlayerProfileFixtures::serializeSav(entries));
        assert(bool(parsed));
        assert(parsed.value.playerNameTextWarning);
        assert(parsed.value.hasFieldWarnings);
        assert(parsed.value.playerName.find("\xef\xbf\xbd") != std::string::npos);

        const auto viewsZero = PlayerProfile::FormatAttributes(parsed.value, tr, kRefClock);
        assert(viewsZero[14].primaryValue == "player_donation_never");
        assert(viewsZero[14].secondaryMetadata == "player_donation_never_hint");

        entries[20].payload = PlayerProfileFixtures::makeUInt64Payload(static_cast<uint64_t>(kRefClock - 86400));
        const auto parsedOneDay = PlayerProfile::Parse(PlayerProfileFixtures::serializeSav(entries));
        const auto viewsOneDay = PlayerProfile::FormatAttributes(parsedOneDay.value, tr, kRefClock);
        assert(viewsOneDay[14].secondaryMetadata == "1 player_donation_day_ago");
    }

    // All 11 currencies, 9 regions, 11 languages, 6 skin tones, 4 island sizes resolve cleanly.
    const uint32_t currencyHashes[] = {
        0x7e3d1e46u, 0xce8d18e1u, 0x2bef288au, 0x038c20a9u, 0x2f084465u,
        0x78915cbeu, 0xd73beb47u, 0xaca685c9u, 0x4616c424u, 0xa60f72e0u, 0x6362048bu
    };
    const char* regions[] = {
        "Japan", "Europe", "NorthAmerica", "SouthAmericaN", "SouthAmericaS",
        "Australia", "Asia", "OthersN", "OthersS"
    };
    const char* langs[] = {
        "JPja", "USen", "EUen", "EUfr", "EUde", "EUes", "EUit", "EUnl", "CNzh", "KRko", "TWzh"
    };
    for (int i = 0; i < 11; ++i) {
        auto entries = PlayerProfileFixtures::baseEntriesA();
        entries[7].inlineRaw = currencyHashes[i];
        entries[2].inlineRaw = PlayerProfile::Hash(langs[i]);
        if (i < 9)
            entries[18].inlineRaw = PlayerProfile::Hash(regions[i]);
        if (i < 6)
            entries[9].inlineRaw = static_cast<uint32_t>(i);
        if (i >= 1 && i <= 4)
            entries[13].inlineRaw = static_cast<uint32_t>(i);
        const auto p = PlayerProfile::Parse(PlayerProfileFixtures::serializeSav(entries));
        assert(bool(p));
        assert(p.value.currency.known && p.value.nameLanguage.known);
        if (i < 9) assert(p.value.region.known);
        if (i < 6) assert(PlayerProfile::SkinToneRgbHex(p.value.skinToneIndex) != nullptr);
        if (i >= 1 && i <= 4) assert(PlayerProfile::IslandSizeDimensions(p.value.islandSizeRaw) != nullptr);
    }

    // Real sanitized template seed_Player.bin parses cleanly end-to-end.
    {
        std::ifstream in("data/seed_Player.bin", std::ios::binary);
        assert(in.good());
        std::vector<uint8_t> seed((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const auto parsed = PlayerProfile::Parse(seed);
        assert(bool(parsed));
        const auto& s = parsed.value;
        assert(s.playerName == "Chef");
        assert(s.islandName == "Amitus");
        assert(s.moneyCents == 1500000u);
        assert(s.currency.code == "Euro");
        assert(s.region.code == "Europe");
        assert(s.nameLanguage.code == "EUfr");
        assert(s.wishes.liberatedAvailable && s.wishes.liberatedCount == 268);
        assert(s.ttPenaltyPresent && s.ttPenaltyRaw == 0ULL);
        assert(s.lastDonationPresent && s.lastDonationSeconds == 1776876030ULL);
    }
}

} // namespace

int main() {
    checkStructuralValidation();
    checkAll15AttributesAndEdgeCases();
    std::cout << "player profile structural, 15 attributes, catalogs & seed: OK\n";
    return 0;
}
