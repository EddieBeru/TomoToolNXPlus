#pragma once
// Synthetic binary Player.sav generators for host verification (no personal save data).
#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace PlayerProfileFixtures {

struct SavEntry {
    uint32_t hash = 0;
    uint32_t type = 0;
    uint32_t inlineRaw = 0;
    std::vector<uint8_t> payload;
};

inline void writeU32(std::vector<uint8_t>& buf, size_t off, uint32_t v) {
    buf[off + 0] = static_cast<uint8_t>(v & 0xffu);
    buf[off + 1] = static_cast<uint8_t>((v >> 8) & 0xffu);
    buf[off + 2] = static_cast<uint8_t>((v >> 16) & 0xffu);
    buf[off + 3] = static_cast<uint8_t>((v >> 24) & 0xffu);
}

inline void writeU64(std::vector<uint8_t>& buf, size_t off, uint64_t v) {
    writeU32(buf, off, static_cast<uint32_t>(v & 0xffffffffull));
    writeU32(buf, off + 4, static_cast<uint32_t>(v >> 32));
}

inline std::vector<uint8_t> makeUInt64Payload(uint64_t v) {
    std::vector<uint8_t> out(8, 0);
    writeU64(out, 0, v);
    return out;
}

// Encodes UTF-8 into a fixed-width UTF-16LE buffer (32 code units = 64B, 64 code units = 128B),
// emitting surrogate pairs for code points above U+FFFF.
inline std::vector<uint8_t> makeUtf16Payload(const std::string& utf8, size_t maxUnits) {
    std::vector<uint8_t> out(maxUnits * 2, 0);
    size_t unit = 0;
    size_t i = 0;
    while (i < utf8.size() && unit + 1 < maxUnits) {
        uint8_t c = static_cast<uint8_t>(utf8[i]);
        uint32_t cp = '?';
        size_t adv = 1;
        if (c < 0x80u) {
            cp = c;
            adv = 1;
        } else if ((c & 0xe0u) == 0xc0u && i + 1 < utf8.size()) {
            cp = ((c & 0x1fu) << 6) | (static_cast<uint8_t>(utf8[i + 1]) & 0x3fu);
            adv = 2;
        } else if ((c & 0xf0u) == 0xe0u && i + 2 < utf8.size()) {
            cp = ((c & 0x0fu) << 12) |
                 ((static_cast<uint8_t>(utf8[i + 1]) & 0x3fu) << 6) |
                 (static_cast<uint8_t>(utf8[i + 2]) & 0x3fu);
            adv = 3;
        } else if ((c & 0xf8u) == 0xf0u && i + 3 < utf8.size()) {
            cp = ((c & 0x07u) << 18) |
                 ((static_cast<uint8_t>(utf8[i + 1]) & 0x3fu) << 12) |
                 ((static_cast<uint8_t>(utf8[i + 2]) & 0x3fu) << 6) |
                 (static_cast<uint8_t>(utf8[i + 3]) & 0x3fu);
            adv = 4;
        }
        i += adv;
        if (cp <= 0xffffu) {
            out[unit * 2 + 0] = static_cast<uint8_t>(cp & 0xffu);
            out[unit * 2 + 1] = static_cast<uint8_t>((cp >> 8) & 0xffu);
            ++unit;
        } else if (cp <= 0x10ffffu && unit + 2 < maxUnits) {
            uint32_t v = cp - 0x10000u;
            uint16_t hi = static_cast<uint16_t>(0xd800u + (v >> 10));
            uint16_t lo = static_cast<uint16_t>(0xdc00u + (v & 0x3ffu));
            out[unit * 2 + 0] = static_cast<uint8_t>(hi & 0xffu);
            out[unit * 2 + 1] = static_cast<uint8_t>((hi >> 8) & 0xffu);
            ++unit;
            out[unit * 2 + 0] = static_cast<uint8_t>(lo & 0xffu);
            out[unit * 2 + 1] = static_cast<uint8_t>((lo >> 8) & 0xffu);
            ++unit;
        }
    }
    return out;
}

inline std::vector<uint8_t> makeUIntArrayPayload(const std::vector<uint32_t>& values) {
    std::vector<uint8_t> out(4 + values.size() * 4, 0);
    writeU32(out, 0, static_cast<uint32_t>(values.size()));
    for (size_t i = 0; i < values.size(); ++i)
        writeU32(out, 4 + i * 4, values[i]);
    return out;
}

inline std::vector<uint8_t> makeBoolArrayPayload(const std::vector<bool>& values) {
    const size_t n = values.size();
    const size_t rawBytes = std::max<size_t>(4, (n + 7) / 8);
    const size_t aligned = (rawBytes + 3) & ~size_t{3};
    std::vector<uint8_t> out(4 + aligned, 0);
    writeU32(out, 0, static_cast<uint32_t>(n));
    for (size_t i = 0; i < n; ++i) {
        if (values[i])
            out[4 + (i >> 3)] |= static_cast<uint8_t>(1u << (i & 7));
    }
    return out;
}

inline std::vector<uint8_t> makeWStr64ArrayPayload(size_t count, const std::string& prefix, bool badSurrogateAt2) {
    std::vector<uint8_t> out(4 + count * 128, 0);
    writeU32(out, 0, static_cast<uint32_t>(count));
    for (size_t i = 0; i < count; ++i) {
        auto item = makeUtf16Payload(prefix + " " + std::to_string(i), 64);
        std::copy(item.begin(), item.end(), out.begin() + 4 + i * 128);
    }
    if (badSurrogateAt2 && count > 2) {
        out[4 + 2 * 128 + 0] = 0x00;
        out[4 + 2 * 128 + 1] = 0xd8; // lone high surrogate
    }
    return out;
}

inline bool isInlineType(uint32_t t) {
    return t == 0u || t == 2u || t == 4u || t == 6u || t == 20u;
}

inline std::vector<uint8_t> serializeSav(const std::vector<SavEntry>& entries, uint32_t version = 1) {
    constexpr uint32_t kTypeCount = 33;
    std::vector<std::vector<size_t>> byType(kTypeCount);
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].type < kTypeCount)
            byType[entries[i].type].push_back(i);
    }
    size_t rowCount = kTypeCount + entries.size();
    const uint32_t saveDataOff = static_cast<uint32_t>(0x20u + rowCount * 8u);
    size_t totalHeap = 0;
    for (const auto& e : entries) {
        if (!isInlineType(e.type))
            totalHeap += e.payload.size();
    }
    std::vector<uint8_t> out(saveDataOff + totalHeap, 0);
    writeU32(out, 0, 0x01020304u);
    writeU32(out, 4, version);
    writeU32(out, 8, saveDataOff);

    size_t indexPos = 0x20u;
    size_t heapPos = saveDataOff;
    for (uint32_t t = 0; t < kTypeCount; ++t) {
        writeU32(out, indexPos, 0u);
        writeU32(out, indexPos + 4, t);
        indexPos += 8;
        for (size_t idx : byType[t]) {
            const auto& e = entries[idx];
            writeU32(out, indexPos, e.hash);
            if (isInlineType(t)) {
                writeU32(out, indexPos + 4, e.inlineRaw);
            } else {
                writeU32(out, indexPos + 4, static_cast<uint32_t>(heapPos));
                std::copy(e.payload.begin(), e.payload.end(), out.begin() + static_cast<std::ptrdiff_t>(heapPos));
                heapPos += e.payload.size();
            }
            indexPos += 8;
        }
    }
    return out;
}

inline std::vector<SavEntry> baseEntriesA(bool includeUgcFoodNames = false, bool badUgcFoodEntry = false) {
    std::vector<uint32_t> wishIds(268, 0);
    std::vector<bool> liberated(268, false);
    for (size_t i = 0; i < 268; ++i) {
        wishIds[i] = static_cast<uint32_t>(0x1000u + i);
        liberated[i] = (i < 100);
    }
    std::vector<SavEntry> e = {
        {0xc7fb279bu, 28, 0, makeUtf16Payload("Álex 😀", 32)},          // Player.Name (accent + emoji surrogate)
        {0xbd8c4da1u, 30, 0, makeUtf16Payload("A-lex", 64)},            // Player.HowToCallName (DT_WStr64)
        {0xb3229c6bu,  6, 0xbdd674dcu, {}},                             // Player.NameRegionLanguageID = EUes
        {0xd46df986u, 28, 0, makeUtf16Payload("Ítaca", 32)},            // Player.IslandName
        {0xc6482c14u, 30, 0, makeUtf16Payload("I-ta-ca", 64)},          // Player.HowToCallIslandName (DT_WStr64)
        {0x7bbff932u,  6, 0xbdd674dcu, {}},                             // Player.IslandNameRegionLanguageID = EUes
        {0x365fab1fu, 20, 1500000u, {}},                                // Player.Money -> 15 000,00
        {0xdc88f139u,  6, 0x038c20a9u, {}},                             // Player.Currency = Euro (€)
        {0x2f19f777u, 20, 42u, {}},                                     // Player.BootNum
        {0xafbdf7dcu, 20, 2u, {}},                                      // Player.SkinColorIndex (2 -> tone 3)
        {0xdb7786bbu, 20, 15u, {}},                                     // BdayDay
        {0xc754bef3u, 20, 8u, {}},                                      // BdayMonth
        {0x11996629u, 20, 1998u, {}},                                   // BdayYear
        {0x870a807cu,  2, 3u, {}},                                      // IslandSize = 3 (90 x 64)
        {0xa4e65f79u, 20, 12u, {}},                                     // Liberation.FountainLevel
        {0xa32f7e47u, 20, 105u, {}},                                    // Wishes count
        {0xf91e67beu, 21, 0, makeUIntArrayPayload(wishIds)},            // Liberation.WishInfo.WishIdValue
        {0xe38c7611u,  1, 0, makeBoolArrayPayload(liberated)},          // Liberation.WishInfo.IsLiberated (100/268)
        {0x10631cf8u,  6, 0x88fa1449u, {}},                             // Player.Region = Europe
        {0x71e3ba15u, 24, 0, makeUInt64Payload(0ULL)},                  // TTPenalty = cleared
        {0xbba9c090u, 24, 0, makeUInt64Payload(1700000000ULL)},         // Player.LastDonationGameTime (3 days ago @ 1700259200)
    };
    if (includeUgcFoodNames)
        e.push_back({0x408494f5u, 31, 0, makeWStr64ArrayPayload(99, "IslandA", badUgcFoodEntry)});
    return e;
}

inline std::vector<uint8_t> makeSyntheticPlayerSavA(bool includeUgcFoodNames = false, bool badUgcFoodEntry = false) {
    return serializeSav(baseEntriesA(includeUgcFoodNames, badUgcFoodEntry));
}

inline std::vector<SavEntry> baseEntriesB(bool includeUgcFoodNames = false) {
    // Distinct values + edge cases (empty pronunciation, unknown enums, out-of-range scalars, active TT, future donation, no wishes table).
    std::vector<SavEntry> e = {
        {0xc7fb279bu, 28, 0, makeUtf16Payload("Sofía", 32)},
        {0xbd8c4da1u, 30, 0, makeUtf16Payload("", 64)},                 // empty pronunciation
        {0xb3229c6bu,  6, 0xfeedfaceu, {}},                             // unknown language hash
        {0xd46df986u, 28, 0, makeUtf16Payload("Avalon", 32)},
        {0xc6482c14u, 28, 0, makeUtf16Payload("A-va-lon", 32)},         // DT_WStr32 variant
        {0x7bbff932u,  6, 0xeb419930u, {}},                             // USen
        {0x365fab1fu, 20, 850050u, {}},                                 // 8 500,50
        {0xdc88f139u,  6, 0x12345678u, {}},                             // unknown currency hash
        {0x2f19f777u, 20, 7u, {}},                                      // BootNum
        {0xafbdf7dcu, 20, 9u, {}},                                      // out-of-range skin tone (> 5)
        {0xdb7786bbu, 20, 99u, {}},                                     // out-of-range bday day
        {0xc754bef3u, 20, 0u, {}},                                      // out-of-range bday month
        {0x11996629u,  2, static_cast<uint32_t>(-5), {}},               // signed int variant
        {0x870a807cu,  2, 7u, {}},                                      // non-standard island size
        {0xa4e65f79u, 20, 3u, {}},                                      // FountainLv
        {0xa32f7e47u, 20, 19u, {}},                                     // Wishes without Liberation.WishInfo arrays
        {0x10631cf8u,  6, 0xdeadbeefu, {}},                             // unknown region hash
        {0x71e3ba15u, 24, 0, makeUInt64Payload(1700100000ULL)},         // active TT penalty
        {0xbba9c090u, 24, 0, makeUInt64Payload(4102444800ULL)},         // future donation stamp (2100-01-01 UTC)
    };
    if (includeUgcFoodNames)
        e.push_back({0x408494f5u, 31, 0, makeWStr64ArrayPayload(50, "IslandB", false)});
    return e;
}

inline std::vector<uint8_t> makeSyntheticPlayerSavB(bool includeUgcFoodNames = false) {
    return serializeSav(baseEntriesB(includeUgcFoodNames));
}

} // namespace PlayerProfileFixtures
