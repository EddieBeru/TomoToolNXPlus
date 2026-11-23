#pragma once
// Synthetic binary Mii.sav and Map.sav generators for host verification (no personal save data).
#include "belongings_data.h"
#include "habits_data.h"
#include "player_profile_fixtures.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace MiiRosterFixtures {

using PlayerProfileFixtures::SavEntry;
using PlayerProfileFixtures::makeBoolArrayPayload;
using PlayerProfileFixtures::makeUIntArrayPayload;
using PlayerProfileFixtures::makeUtf16Payload;
using PlayerProfileFixtures::serializeSav;
using PlayerProfileFixtures::writeU32;
using PlayerProfileFixtures::writeU64;

inline uint32_t murmur3(const char* key) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(key);
    const size_t len = std::strlen(key);
    const size_t nblocks = len / 4;
    uint32_t h1 = 0;
    const uint32_t c1 = 0xcc9e2d51u;
    const uint32_t c2 = 0x1b873593u;
    for (size_t i = 0; i < nblocks; ++i) {
        uint32_t k1 = static_cast<uint32_t>(data[i * 4]) |
                      (static_cast<uint32_t>(data[i * 4 + 1]) << 8) |
                      (static_cast<uint32_t>(data[i * 4 + 2]) << 16) |
                      (static_cast<uint32_t>(data[i * 4 + 3]) << 24);
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;
        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19);
        h1 = h1 * 5u + 0xe6546b64u;
    }
    const uint8_t* tail = data + nblocks * 4;
    uint32_t k1 = 0;
    switch (len & 3u) {
        case 3: k1 ^= static_cast<uint32_t>(tail[2]) << 16; // fallthrough
        case 2: k1 ^= static_cast<uint32_t>(tail[1]) << 8;  // fallthrough
        case 1: k1 ^= static_cast<uint32_t>(tail[0]);
                k1 *= c1;
                k1 = (k1 << 15) | (k1 >> 17);
                k1 *= c2;
                h1 ^= k1;
        default: break;
    }
    h1 ^= static_cast<uint32_t>(len);
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6bu;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35u;
    h1 ^= h1 >> 16;
    return h1;
}

inline std::vector<uint8_t> makeIntArrayPayload(const std::vector<int32_t>& values) {
    std::vector<uint8_t> out(4 + values.size() * 4, 0);
    writeU32(out, 0, static_cast<uint32_t>(values.size()));
    for (size_t i = 0; i < values.size(); ++i)
        writeU32(out, 4 + i * 4, static_cast<uint32_t>(values[i]));
    return out;
}

inline std::vector<uint8_t> makeUInt64ArrayPayload(const std::vector<uint64_t>& values) {
    std::vector<uint8_t> out(4 + values.size() * 8, 0);
    writeU32(out, 0, static_cast<uint32_t>(values.size()));
    for (size_t i = 0; i < values.size(); ++i)
        writeU64(out, 4 + i * 8, values[i]);
    return out;
}

inline std::vector<uint8_t> makeWStr32ArrayPayload(const std::vector<std::string>& strings) {
    std::vector<uint8_t> out(4 + strings.size() * 64, 0);
    writeU32(out, 0, static_cast<uint32_t>(strings.size()));
    for (size_t i = 0; i < strings.size(); ++i) {
        if (!strings[i].empty()) {
            auto item = makeUtf16Payload(strings[i], 32);
            std::copy(item.begin(), item.end(), out.begin() + 4 + static_cast<std::ptrdiff_t>(i * 64));
        }
    }
    return out;
}

inline std::vector<uint8_t> makeWStr64ArrayPayload(const std::vector<std::string>& strings) {
    std::vector<uint8_t> out(4 + strings.size() * 128, 0);
    writeU32(out, 0, static_cast<uint32_t>(strings.size()));
    for (size_t i = 0; i < strings.size(); ++i) {
        if (!strings[i].empty()) {
            auto item = makeUtf16Payload(strings[i], 64);
            std::copy(item.begin(), item.end(), out.begin() + 4 + static_cast<std::ptrdiff_t>(i * 128));
        }
    }
    return out;
}

inline std::vector<uint8_t> makeCharInfoExPayload(const std::vector<bool>& occupied) {
    constexpr size_t kElemSize = 16;
    std::vector<uint8_t> out(4 + occupied.size() * (4 + kElemSize), 0);
    writeU32(out, 0, static_cast<uint32_t>(occupied.size()));
    size_t pos = 4;
    for (size_t i = 0; i < occupied.size(); ++i) {
        writeU32(out, pos, static_cast<uint32_t>(kElemSize));
        pos += 4;
        if (occupied[i]) {
            out[pos] = 0x5au;
            out[pos + 1] = static_cast<uint8_t>((i + 1) & 0xffu);
        }
        pos += kElemSize;
    }
    return out;
}

struct SyntheticMiiSpec {
    size_t slotIndex = 0; // 0..69
    std::string name;
    bool charInfoOccupied = true;
    int32_t levelRaw = 0;
    int32_t levelMeter = 50;
    uint32_t money = 1000;
    int32_t bdayDay = 15;
    int32_t bdayMonth = 6;
    int32_t bdayYear = 1995;
    int32_t directAge = -1;
    int32_t activeness = 5;
    int32_t audacity = 6;
    int32_t commonsense = 7;
    int32_t gaiety = 8;
    int32_t sociability = 9;
    int32_t bondMeter = 75;
    uint32_t moodHash = 0x62189afbu; // Good
    int32_t fullness = 60;
    int32_t houseMapId = -1;
    int32_t roomIndex = 0;
};

struct SyntheticWordSpec {
    size_t slotIndex = 0;
    size_t wordIndex = 0; // 0..11
    uint32_t kindHash = 0x7e3d1e46u; // Invalid
    std::string text;
    std::string howToCall;
};

struct SyntheticRelSpec {
    size_t pairIndex = 0;
    int32_t idA = -1;
    int32_t idB = -1;
    uint32_t typeAB = 0x7e3d1e46u;
    uint32_t typeBA = 0x7e3d1e46u;
    int32_t meterAB = 0;
    int32_t meterBA = 0;
    bool isFight = false;
    uint64_t sinceTs = 0;
};

struct SyntheticMiiSavBuilder {
    static constexpr size_t kSlots = 70;
    static constexpr size_t kWords = 70 * 12;
    static constexpr size_t kPairs = 2415;

    std::vector<std::string> names = std::vector<std::string>(kSlots);
    std::vector<bool> charOcc = std::vector<bool>(kSlots, false);
    std::vector<int32_t> level = std::vector<int32_t>(kSlots, 0);
    std::vector<int32_t> levelMeter = std::vector<int32_t>(kSlots, 0);
    std::vector<uint32_t> money = std::vector<uint32_t>(kSlots, 0);
    std::vector<int32_t> bdayDay = std::vector<int32_t>(kSlots, 1);
    std::vector<int32_t> bdayMonth = std::vector<int32_t>(kSlots, 1);
    std::vector<int32_t> bdayYear = std::vector<int32_t>(kSlots, 2000);
    std::vector<int32_t> directAge = std::vector<int32_t>(kSlots, -1);
    std::vector<int32_t> activeness = std::vector<int32_t>(kSlots, 5);
    std::vector<int32_t> audacity = std::vector<int32_t>(kSlots, 5);
    std::vector<int32_t> commonsense = std::vector<int32_t>(kSlots, 5);
    std::vector<int32_t> gaiety = std::vector<int32_t>(kSlots, 5);
    std::vector<int32_t> sociability = std::vector<int32_t>(kSlots, 5);
    std::vector<int32_t> bondMeter = std::vector<int32_t>(kSlots, 0);
    std::vector<uint32_t> mood = std::vector<uint32_t>(kSlots, 0x390b012du); // Normal
    std::vector<int32_t> fullness = std::vector<int32_t>(kSlots, 0);
    std::vector<int32_t> houseMapId = std::vector<int32_t>(kSlots, -1);
    std::vector<int32_t> roomIndex = std::vector<int32_t>(kSlots, 0);

    std::vector<uint32_t> wordKind = std::vector<uint32_t>(kWords, 0x7e3d1e46u); // Invalid
    std::vector<std::string> wordText = std::vector<std::string>(kWords);
    std::vector<std::string> wordHow = std::vector<std::string>(kWords);

    std::vector<int32_t> relIdA = std::vector<int32_t>(kPairs, -1);
    std::vector<int32_t> relIdB = std::vector<int32_t>(kPairs, -1);
    std::vector<uint32_t> relType = std::vector<uint32_t>(kPairs * 2, 0x7e3d1e46u);
    std::vector<int32_t> relMeter = std::vector<int32_t>(kPairs * 2, 0);
    std::vector<bool> relFight = std::vector<bool>(kPairs, false);
    std::vector<uint64_t> relSince = std::vector<uint64_t>(kPairs, 0);

    std::array<std::vector<uint32_t>, 8> wornHash;
    std::array<std::vector<int32_t>, 8> wornColor;
    std::vector<uint32_t> coordHash = std::vector<uint32_t>(kSlots, 0x7e3d1e46u);
    std::vector<int32_t> coordColor = std::vector<int32_t>(kSlots, 0);
    std::vector<uint32_t> pocket = std::vector<uint32_t>(kWords, 0u);

    std::vector<std::vector<bool>> habitOwn;
    std::vector<std::vector<bool>> habitChecked;
    std::vector<std::vector<uint32_t>> habitState;

    SyntheticMiiSavBuilder() {
        for (size_t s = 0; s < 8; ++s) {
            wornHash[s].assign(kSlots, 0x7e3d1e46u);
            wornColor[s].assign(kSlots, 0);
        }
        habitOwn.assign(static_cast<size_t>(HABIT_COUNT), std::vector<bool>(kSlots, false));
        habitChecked.assign(static_cast<size_t>(HABIT_COUNT), std::vector<bool>(kSlots, false));
        habitState.assign(static_cast<size_t>(HABIT_COUNT), std::vector<uint32_t>(kSlots, murmur3("NeverOwned")));
    }

    void addMii(const SyntheticMiiSpec& m) {
        const size_t i = m.slotIndex;
        names[i] = m.name;
        charOcc[i] = m.charInfoOccupied;
        level[i] = m.levelRaw;
        levelMeter[i] = m.levelMeter;
        money[i] = m.money;
        bdayDay[i] = m.bdayDay;
        bdayMonth[i] = m.bdayMonth;
        bdayYear[i] = m.bdayYear;
        directAge[i] = m.directAge;
        activeness[i] = m.activeness;
        audacity[i] = m.audacity;
        commonsense[i] = m.commonsense;
        gaiety[i] = m.gaiety;
        sociability[i] = m.sociability;
        bondMeter[i] = m.bondMeter;
        mood[i] = m.moodHash;
        fullness[i] = m.fullness;
        houseMapId[i] = m.houseMapId;
        roomIndex[i] = m.roomIndex;
    }

    void setWord(const SyntheticWordSpec& w) {
        const size_t ai = w.slotIndex * 12 + w.wordIndex;
        wordKind[ai] = w.kindHash;
        wordText[ai] = w.text;
        wordHow[ai] = w.howToCall;
    }

    void setRelation(const SyntheticRelSpec& r) {
        relIdA[r.pairIndex] = r.idA;
        relIdB[r.pairIndex] = r.idB;
        relType[r.pairIndex * 2 + 0] = r.typeAB;
        relType[r.pairIndex * 2 + 1] = r.typeBA;
        relMeter[r.pairIndex * 2 + 0] = r.meterAB;
        relMeter[r.pairIndex * 2 + 1] = r.meterBA;
        relFight[r.pairIndex] = r.isFight;
        relSince[r.pairIndex] = r.sinceTs;
    }

    void setWorn(size_t slotIndex, size_t clothSlot, uint32_t itemHash, int32_t colorIdx) {
        wornHash[clothSlot][slotIndex] = itemHash;
        wornColor[clothSlot][slotIndex] = colorIdx;
    }

    void setCoord(size_t slotIndex, uint32_t itemHash, int32_t colorIdx) {
        coordHash[slotIndex] = itemHash;
        coordColor[slotIndex] = colorIdx;
    }

    void setPocket(size_t slotIndex, size_t pocketIdx, uint32_t goodsHash) {
        pocket[slotIndex * 12 + pocketIdx] = goodsHash;
    }

    void setHabit(size_t slotIndex, size_t habitIdx, bool own, bool checked, uint32_t stateHash) {
        habitOwn[habitIdx][slotIndex] = own;
        habitChecked[habitIdx][slotIndex] = checked;
        habitState[habitIdx][slotIndex] = stateHash;
    }

    std::vector<uint8_t> build(bool includeHabits = true) const {
        static constexpr uint32_t kWornKH[8] = {
            0xb20fecedu, 0x35256740u, 0xe81ee9c4u, 0x15aea81du,
            0x7a1809d1u, 0x98cc9cccu, 0x0a903d03u, 0xc1094d9fu,
        };
        static constexpr uint32_t kWornCH[8] = {
            0xee37f256u, 0x1124095du, 0x77cf0ff7u, 0xec5010fbu,
            0x536dbec2u, 0xc499cda5u, 0xc0529e53u, 0x64af701au,
        };

        std::vector<SavEntry> e = {
            {0x2499bfdau, 29, 0, makeWStr32ArrayPayload(names)},
            {0x881ca27au, 19, 0, makeCharInfoExPayload(charOcc)},
            {0x9999b7d9u,  3, 0, makeIntArrayPayload(level)},
            {0x197237e7u,  3, 0, makeIntArrayPayload(levelMeter)},
            {0xf334fd2eu, 21, 0, makeUIntArrayPayload(money)},
            {0xab8ae08bu,  3, 0, makeIntArrayPayload(bdayDay)},
            {0x6cf484f4u,  3, 0, makeIntArrayPayload(bdayMonth)},
            {0x5d7d3f45u,  3, 0, makeIntArrayPayload(bdayYear)},
            {0x2545e583u,  3, 0, makeIntArrayPayload(directAge)},
            {0x25b48224u,  3, 0, makeIntArrayPayload(activeness)},
            {0xcd8dbaf8u,  3, 0, makeIntArrayPayload(audacity)},
            {0x607ba160u,  3, 0, makeIntArrayPayload(commonsense)},
            {0x68e1134eu,  3, 0, makeIntArrayPayload(gaiety)},
            {0x43cd364fu,  3, 0, makeIntArrayPayload(sociability)},
            {0xd0882636u,  3, 0, makeIntArrayPayload(bondMeter)},
            {0x176c29ebu,  7, 0, makeUIntArrayPayload(mood)},
            {0xef25b456u,  3, 0, makeIntArrayPayload(fullness)},
            {0xb8ab3cacu,  3, 0, makeIntArrayPayload(houseMapId)},
            {0xfc296601u,  3, 0, makeIntArrayPayload(roomIndex)},
            {0x5f7bfcc8u,  7, 0, makeUIntArrayPayload(wordKind)},
            {0x09ef8656u, 31, 0, makeWStr64ArrayPayload(wordText)},
            {0x0b2a261eu, 31, 0, makeWStr64ArrayPayload(wordHow)},
            {0xf7420afbu,  3, 0, makeIntArrayPayload(relIdA)},
            {0x4071f71cu,  3, 0, makeIntArrayPayload(relIdB)},
            {0x8b41897eu,  7, 0, makeUIntArrayPayload(relType)},
            {0x42c2fc2fu,  3, 0, makeIntArrayPayload(relMeter)},
            {0xc2db3a72u,  1, 0, makeBoolArrayPayload(relFight)},
            {0x1a892e50u, 25, 0, makeUInt64ArrayPayload(relSince)},
            {0xd5ee521au, 21, 0, makeUIntArrayPayload(coordHash)},
            {0x763c97ccu,  3, 0, makeIntArrayPayload(coordColor)},
            {0xe3d6005eu, 21, 0, makeUIntArrayPayload(pocket)},
        };
        for (size_t s = 0; s < 8; ++s) {
            e.push_back({kWornKH[s], 21, 0, makeUIntArrayPayload(wornHash[s])});
            e.push_back({kWornCH[s],  3, 0, makeIntArrayPayload(wornColor[s])});
        }
        if (includeHabits) {
            for (int h = 0; h < HABIT_COUNT; ++h) {
                std::string base = std::string("Mii.Belongings.HabitOwnInfo.") + HABITS[h].name;
                e.push_back({murmur3((base + ".IsOwn").c_str()),     1, 0, makeBoolArrayPayload(habitOwn[h])});
                e.push_back({murmur3((base + ".IsChecked").c_str()), 1, 0, makeBoolArrayPayload(habitChecked[h])});
                e.push_back({murmur3((base + ".State").c_str()),     7, 0, makeUIntArrayPayload(habitState[h])});
            }
        }
        return serializeSav(e);
    }
};

inline std::vector<uint8_t> makeSyntheticMapSavA() {
    std::vector<int32_t> mapIds(70, -1);
    std::vector<std::string> groupNames(70);
    mapIds[0] = 1;
    groupNames[0] = "Casa Sol";
    mapIds[1] = 2;
    groupNames[1] = "Villa Mar";
    std::vector<SavEntry> e = {
        {0x0b5276e9u,  3, 0, makeIntArrayPayload(mapIds)},
        {0x0d96409eu, 29, 0, makeWStr32ArrayPayload(groupNames)},
    };
    return serializeSav(e);
}

inline std::vector<uint8_t> makeSyntheticMiiSavA() {
    SyntheticMiiSavBuilder b;
    // Sparse slots: 1 (idx 0), 14 (idx 13), 70 (idx 69)
    b.addMii({0, "Lucía 🌟", true, 4, 82, 12500, 14, 3, 1998, -1, 8, 6, 7, 9, 10, 91, 0x62189afbu, 115, 1, 0});
    b.addMii({13, "Kenji", true, 9, 40, 500, 22, 11, 2001, 24, 4, 7, 9, 5, 6, 65, 0xf3046534u, 45, 1, 0});
    b.addMii({69, "Mía", true, 0, 10, 0, 1, 1, 2005, -1, 3, 4, 5, 6, 7, 20, 0x390b012du, 0, -1, 0});
    // Deleted slot (idx 5): name empty, CharInfoEx still non-zero -> must NOT be counted as occupied
    b.charOcc[5] = true;
    // Ghost slot (idx 6): name non-empty, CharInfoEx all-zero -> must NOT be counted as occupied
    b.names[6] = "Ghost";
    b.charOcc[6] = false;

    // Words for slot 1 (idx 0)
    b.setWord({0, 0, murmur3("Greeting"), "¡Hola isla! 🌴", "O-la is-la"});
    b.setWord({0, 1, murmur3("Happy"), "Qué alegría", ""});
    b.setWord({0, 2, murmur3("Phrase"), "Siempre adelante", "Siem-pre"});

    // Relations for slot 1 (idx 0):
    // Pair 0: idx 0 <-> idx 13 (Couple, fighting, meter 165)
    b.setRelation({0, 0, 13, 0xc2d067a7u, 0xc2d067a7u, 165, 150, true, 1700000000ULL});
    // Pair 1: idx 0 <-> idx 69 (Friend, meter 125)
    b.setRelation({1, 0, 69, 0xba939a42u, 0xba939a42u, 125, 130, false, 1700100000ULL});
    // Pair 2: idx 0 <-> empty slot 20 (Parent -> orphan slot)
    b.setRelation({2, 0, 20, 0xdcfc7603u, 0xe193c5a2u, 90, 95, false, 0ULL});
    // Pair 3: idx 13 <-> idx 69 (Know, meter 50)
    b.setRelation({3, 13, 69, 0x354a0515u, 0x354a0515u, 50, 45, false, 1700200000ULL});

    // Belongings: slot 1 wears Tops + Headwear + 2 pocket items; slot 14 wears Coordinate
    b.setWorn(0, 2, BL_CLOTH_ITEMS[3].nameHash, 2); // ABC sweatshirt (Tops)
    b.setWorn(0, 5, BL_CLOTH_ITEMS[1].nameHash, 1); // 5-panel cap (Headwear)
    b.setPocket(0, 0, BL_GOODS_ITEMS[0].nameHash);
    b.setPocket(0, 1, BL_GOODS_ITEMS[1].nameHash);
    b.setCoord(13, BL_COORD_ITEMS[0].keyHash, 0);

    // Habits for slot 1: one owned+active in each of the 9 categories
    const uint32_t hOwn = murmur3("Own");
    const uint32_t hUnown = murmur3("Unown");
    for (int cat = 0; cat < HABIT_CAT_COUNT; ++cat) {
        for (int h = 0; h < HABIT_COUNT; ++h) {
            if (HABITS[h].category == cat) {
                b.setHabit(0, static_cast<size_t>(h), true, true, hOwn);
                if (h + 1 < HABIT_COUNT && HABITS[h + 1].category == cat)
                    b.setHabit(0, static_cast<size_t>(h + 1), false, false, hUnown);
                break;
            }
        }
    }
    return b.build(true);
}

inline std::vector<uint8_t> makeSyntheticMiiSavB() {
    SyntheticMiiSavBuilder b;
    // Account B: 1 resident at slot 3 (idx 2), unknown enums, out-of-range birthday, house without Map.sav
    b.addMii({2, "Sofía", true, 2, 100, 777, 99, 0, 2010, -1, 1, 2, 3, 4, 5, 50, 0xdeadbeefu, 20, 4, 2});
    b.setWord({2, 0, 0xfeedfaceu, "Misterio", ""});
    b.setRelation({0, 2, 10, 0x12345678u, 0x12345678u, 70, 70, false, 0ULL});
    b.setWorn(2, 2, 0xbadc0ffeu, 0);
    b.setPocket(2, 0, 0xabcdef01u);
    b.setHabit(2, 0, true, false, 0x99887766u);
    return b.build(true);
}

inline std::vector<uint8_t> makeEmptyIslandMiiSav() {
    SyntheticMiiSavBuilder b;
    return b.build(false);
}

inline std::vector<uint8_t> makeFullIslandMiiSav() {
    SyntheticMiiSavBuilder b;
    for (size_t i = 0; i < 70; ++i) {
        b.addMii({i, "Resident " + std::to_string(i + 1), true,
                  static_cast<int32_t>(i % 20), static_cast<int32_t>((i * 7) % 101),
                  static_cast<uint32_t>(i * 100), 1 + static_cast<int32_t>(i % 28),
                  1 + static_cast<int32_t>(i % 12), 1990 + static_cast<int32_t>(i % 20),
                  -1, 5, 5, 5, 5, 5, 50, 0x390b012du, 50, static_cast<int32_t>(i / 4), static_cast<int32_t>(i % 4)});
    }
    return b.build(false);
}

} // namespace MiiRosterFixtures
