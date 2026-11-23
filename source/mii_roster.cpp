// Read-only Mii.sav (+ optional Map.sav) parser, catalog resolver, 7-section
// formatter, and session state machine. Strictly read-only; no save mutation.
#include "mii_roster.h"
#include "belongings_data.h"
#include "habits_data.h"
#include "rel_tier_labels.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>

namespace MiiRoster {
namespace {

enum DataType : uint32_t {
    DT_Bool = 0, DT_BoolArray = 1,
    DT_Int = 2, DT_IntArray = 3,
    DT_Float = 4, DT_FloatArray = 5,
    DT_Enum = 6, DT_EnumArray = 7,
    DT_Vec2 = 8, DT_Vec2Array = 9,
    DT_Vec3 = 10, DT_Vec3Array = 11,
    DT_Str16 = 12, DT_Str16Array = 13,
    DT_Str32 = 14, DT_Str32Array = 15,
    DT_Str64 = 16, DT_Str64Array = 17,
    DT_Bin = 18, DT_BinArray = 19,
    DT_UInt = 20, DT_UIntArray = 21,
    DT_Int64 = 22, DT_Int64Array = 23,
    DT_UInt64 = 24, DT_UInt64Array = 25,
    DT_WStr16 = 26, DT_WStr16Array = 27,
    DT_WStr32 = 28, DT_WStr32Array = 29,
    DT_WStr64 = 30, DT_WStr64Array = 31,
    DT_Bool64Key = 32,
    DT_COUNT = 33
};

template <class F>
struct ScopeGuard {
    F fn;
    ~ScopeGuard() { fn(); }
};

uint32_t rotl32(uint32_t x, int r) {
    return (x << r) | (x >> (32 - r));
}

uint32_t readU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

uint64_t readU64(const uint8_t* p) {
    return static_cast<uint64_t>(readU32(p)) |
           (static_cast<uint64_t>(readU32(p + 4)) << 32);
}

bool isInlineType(uint32_t t) {
    return t == DT_Bool || t == DT_Int || t == DT_Float || t == DT_Enum || t == DT_UInt;
}

size_t fixedHeapSize(uint32_t t) {
    switch (t) {
        case DT_Int64: case DT_UInt64: case DT_Vec2: return 8;
        case DT_Vec3: return 12;
        case DT_Str16: return 16;
        case DT_Str32: case DT_WStr16: return 32;
        case DT_Str64: case DT_WStr32: return 64;
        case DT_WStr64: return 128;
        default: return 0;
    }
}

MiiReadError validateHeapEntry(const std::vector<uint8_t>& bytes, size_t saveDataOff, uint32_t slot, uint32_t type) {
    if (isInlineType(type) || type == DT_Bool64Key)
        return MiiReadError::None;
    const size_t off = static_cast<size_t>(slot);
    if (off < saveDataOff)
        return MiiReadError::CorruptIndex;
    if (off >= bytes.size())
        return MiiReadError::TruncatedPayload;
    const size_t fixed = fixedHeapSize(type);
    if (fixed > 0)
        return fixed <= bytes.size() - off ? MiiReadError::None : MiiReadError::TruncatedPayload;
    if (bytes.size() - off < 4)
        return MiiReadError::TruncatedPayload;
    const uint64_t count = readU32(bytes.data() + off);
    const size_t avail = bytes.size() - off - 4;
    auto checkMul = [&](size_t elemSize) -> MiiReadError {
        if (count > MaxArrayCount)
            return MiiReadError::CorruptIndex;
        return count * elemSize <= avail ? MiiReadError::None : MiiReadError::TruncatedPayload;
    };
    switch (type) {
        case DT_Bin:
            return count <= avail ? MiiReadError::None : MiiReadError::TruncatedPayload;
        case DT_BoolArray: {
            if (count > MaxArrayCount)
                return MiiReadError::CorruptIndex;
            const size_t byteSize = std::max<size_t>(4, static_cast<size_t>((count + 7) / 8));
            const size_t aligned = (byteSize + 3) & ~size_t{3};
            return aligned <= avail ? MiiReadError::None : MiiReadError::TruncatedPayload;
        }
        case DT_IntArray: case DT_UIntArray: case DT_FloatArray: case DT_EnumArray:
            return checkMul(4);
        case DT_Int64Array: case DT_UInt64Array: case DT_Vec2Array:
            return checkMul(8);
        case DT_Vec3Array:
            return checkMul(12);
        case DT_Str16Array:
            return checkMul(16);
        case DT_Str32Array: case DT_WStr16Array:
            return checkMul(32);
        case DT_Str64Array: case DT_WStr32Array:
            return checkMul(64);
        case DT_WStr64Array:
            return checkMul(128);
        case DT_BinArray: {
            if (count > MaxArrayCount)
                return MiiReadError::CorruptIndex;
            size_t p = off + 4;
            for (uint64_t i = 0; i < count; ++i) {
                if (bytes.size() - p < 4)
                    return MiiReadError::TruncatedPayload;
                const uint32_t elemSize = readU32(bytes.data() + p);
                p += 4;
                if (elemSize > bytes.size() - p)
                    return MiiReadError::TruncatedPayload;
                p += elemSize;
            }
            return MiiReadError::None;
        }
        default:
            return MiiReadError::CorruptIndex;
    }
}

void appendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80u) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800u) {
        out += static_cast<char>(0xc0u | (cp >> 6));
        out += static_cast<char>(0x80u | (cp & 0x3fu));
    } else if (cp < 0x10000u) {
        out += static_cast<char>(0xe0u | (cp >> 12));
        out += static_cast<char>(0x80u | ((cp >> 6) & 0x3fu));
        out += static_cast<char>(0x80u | (cp & 0x3fu));
    } else {
        out += static_cast<char>(0xf0u | (cp >> 18));
        out += static_cast<char>(0x80u | ((cp >> 12) & 0x3fu));
        out += static_cast<char>(0x80u | ((cp >> 6) & 0x3fu));
        out += static_cast<char>(0x80u | (cp & 0x3fu));
    }
}

bool decodeUtf16LeFixed(const uint8_t* data, size_t maxUnits, std::string& out) {
    out.clear();
    bool clean = true;
    for (size_t i = 0; i < maxUnits; ++i) {
        uint32_t cp = static_cast<uint32_t>(data[i * 2]) |
                      (static_cast<uint32_t>(data[i * 2 + 1]) << 8);
        if (cp == 0)
            break;
        if (cp >= 0xd800u && cp <= 0xdbffu) {
            if (i + 1 < maxUnits) {
                uint32_t low = static_cast<uint32_t>(data[(i + 1) * 2]) |
                               (static_cast<uint32_t>(data[(i + 1) * 2 + 1]) << 8);
                if (low >= 0xdc00u && low <= 0xdfffu) {
                    cp = 0x10000u + ((cp - 0xd800u) << 10) + (low - 0xdc00u);
                    ++i;
                } else {
                    clean = false;
                    cp = 0xfffdu;
                }
            } else {
                clean = false;
                cp = 0xfffdu;
            }
        } else if (cp >= 0xdc00u && cp <= 0xdfffu) {
            clean = false;
            cp = 0xfffdu;
        }
        appendUtf8(out, cp);
    }
    return clean;
}

struct SavIndexEntry {
    uint32_t type = 0;
    uint32_t slot = 0;
};

struct SavView {
    const std::vector<uint8_t>* bytes = nullptr;
    uint32_t version = 0;
    size_t saveDataOff = 0;
    std::map<uint32_t, SavIndexEntry> entries;

    const SavIndexEntry* find(uint32_t hash) const {
        auto it = entries.find(hash);
        return it == entries.end() ? nullptr : &it->second;
    }

    bool readIntAt(uint32_t hash, size_t idx, int32_t& out) const {
        const auto* e = find(hash);
        if (!e || (e->type != DT_IntArray && e->type != DT_UIntArray && e->type != DT_EnumArray))
            return false;
        const size_t off = e->slot;
        const uint32_t count = readU32(bytes->data() + off);
        if (idx >= count)
            return false;
        out = static_cast<int32_t>(readU32(bytes->data() + off + 4 + idx * 4));
        return true;
    }

    bool readUIntAt(uint32_t hash, size_t idx, uint32_t& out) const {
        const auto* e = find(hash);
        if (!e || (e->type != DT_UIntArray && e->type != DT_IntArray && e->type != DT_EnumArray))
            return false;
        const size_t off = e->slot;
        const uint32_t count = readU32(bytes->data() + off);
        if (idx >= count)
            return false;
        out = readU32(bytes->data() + off + 4 + idx * 4);
        return true;
    }

    bool readUInt64At(uint32_t hash, size_t idx, uint64_t& out) const {
        const auto* e = find(hash);
        if (!e || (e->type != DT_UInt64Array && e->type != DT_Int64Array))
            return false;
        const size_t off = e->slot;
        const uint32_t count = readU32(bytes->data() + off);
        if (idx >= count)
            return false;
        out = readU64(bytes->data() + off + 4 + idx * 8);
        return true;
    }

    bool readBoolAt(uint32_t hash, size_t idx, bool& out) const {
        const auto* e = find(hash);
        if (!e || e->type != DT_BoolArray)
            return false;
        const size_t off = e->slot;
        const uint32_t count = readU32(bytes->data() + off);
        if (idx >= count)
            return false;
        out = ((bytes->data()[off + 4 + (idx >> 3)] >> (idx & 7)) & 1u) != 0;
        return true;
    }

    bool readWStrAt(uint32_t hash, size_t idx, std::string& out, bool& clean) const {
        const auto* e = find(hash);
        if (!e || (e->type != DT_WStr32Array && e->type != DT_WStr64Array))
            return false;
        const size_t off = e->slot;
        const uint32_t count = readU32(bytes->data() + off);
        if (idx >= count)
            return false;
        const size_t units = (e->type == DT_WStr32Array) ? 32u : 64u;
        const size_t elemBytes = units * 2u;
        clean = decodeUtf16LeFixed(bytes->data() + off + 4 + idx * elemBytes, units, out);
        return true;
    }

    uint32_t arrayCount(uint32_t hash) const {
        const auto* e = find(hash);
        if (!e || isInlineType(e->type) || e->type == DT_Bool64Key || fixedHeapSize(e->type) > 0)
            return 0;
        return readU32(bytes->data() + e->slot);
    }
};

MiiReadError parseSavIndex(const std::vector<uint8_t>& bytes, size_t maxBytes, SavView& out) {
    if (bytes.size() > maxBytes)
        return MiiReadError::FileTooLarge;
    if (bytes.size() < 0x20u)
        return MiiReadError::InvalidHeader;
    if (readU32(bytes.data()) != 0x01020304u)
        return MiiReadError::InvalidHeader;
    const uint32_t ver = readU32(bytes.data() + 4);
    const uint32_t saveDataOff = readU32(bytes.data() + 8);
    if (saveDataOff < 0x20u || saveDataOff > bytes.size() || ((saveDataOff - 0x20u) % 8u) != 0u)
        return MiiReadError::InvalidHeader;

    out.bytes = &bytes;
    out.version = ver;
    out.saveDataOff = saveDataOff;
    out.entries.clear();

    bool seenSentinel = false;
    uint32_t currentType = 0;
    for (size_t pos = 0x20u; pos < saveDataOff; pos += 8u) {
        const uint32_t key = readU32(bytes.data() + pos);
        const uint32_t val = readU32(bytes.data() + pos + 4u);
        if (key == 0u) {
            if (val >= DT_COUNT)
                return MiiReadError::CorruptIndex;
            currentType = val;
            seenSentinel = true;
            continue;
        }
        if (!seenSentinel)
            return MiiReadError::CorruptIndex;
        const MiiReadError heapErr = validateHeapEntry(bytes, saveDataOff, val, currentType);
        if (heapErr != MiiReadError::None)
            return heapErr;
        auto it = out.entries.find(key);
        if (it != out.entries.end()) {
            if (it->second.type != currentType || it->second.slot != val)
                return MiiReadError::CorruptIndex;
        } else {
            out.entries.emplace(key, SavIndexEntry{currentType, val});
        }
    }
    return MiiReadError::None;
}

// Verified Murmur3 x86_32 (seed 0) hashes from data/seed_Mii.bin and data/seed_Map.bin.
constexpr uint32_t kHashInvalidEnum  = 0x7e3d1e46u; // Hash("Invalid")
constexpr uint32_t kHashMiiName      = 0x2499bfdau; // Mii.Name.Name
constexpr uint32_t kHashCharInfoEx   = 0x881ca27au; // Mii.CharInfoEx
constexpr uint32_t kHashLevel        = 0x9999b7d9u; // Mii.MiiMisc.SatisfyInfo.Level
constexpr uint32_t kHashLevelMeter   = 0x197237e7u; // Mii.MiiMisc.SatisfyInfo.Meter
constexpr uint32_t kHashMoney        = 0xf334fd2eu; // Mii.Belongings.Money
constexpr uint32_t kHashBdayDay      = 0xab8ae08bu; // Mii.MiiMisc.BirthdayInfo.Day
constexpr uint32_t kHashBdayMonth    = 0x6cf484f4u; // Mii.MiiMisc.BirthdayInfo.Month
constexpr uint32_t kHashBdayYear     = 0x5d7d3f45u; // Mii.MiiMisc.BirthdayInfo.Year
constexpr uint32_t kHashDirectAge    = 0x2545e583u; // Mii.MiiMisc.BirthdayInfo.DirectAge
constexpr uint32_t kHashActiveness   = 0x25b48224u; // Mii.CharacterParam.Activeness
constexpr uint32_t kHashAudacity     = 0xcd8dbaf8u; // Mii.CharacterParam.Audaciousness
constexpr uint32_t kHashCommonsense  = 0x607ba160u; // Mii.CharacterParam.Commonsense
constexpr uint32_t kHashGaiety       = 0x68e1134eu; // Mii.CharacterParam.Gaiety
constexpr uint32_t kHashSociability  = 0x43cd364fu; // Mii.CharacterParam.Sociability
constexpr uint32_t kHashBondMeter    = 0xd0882636u; // Mii.MiiMisc.BondInfo.Meter
constexpr uint32_t kHashFeelingType  = 0x176c29ebu; // Mii.Feeling.Type
constexpr uint32_t kHashFullness     = 0xef25b456u; // Mii.MiiMisc.EatInfo.EatFullness
constexpr uint32_t kHashHouseMapId   = 0xb8ab3cacu; // Mii.Location.HouseMapId
constexpr uint32_t kHashRoomIndex    = 0xfc296601u; // Mii.Location.RoomIndex
constexpr uint32_t kHashWordKind     = 0x5f7bfcc8u; // Mii.MiiMisc.WordInfo.WordArray.WordKind
constexpr uint32_t kHashWordText     = 0x09ef8656u; // Mii.MiiMisc.WordInfo.WordArray.WordText
constexpr uint32_t kHashWordHow      = 0x0b2a261eu; // Mii.MiiMisc.WordInfo.WordArray.WordHowToCall
constexpr uint32_t kHashRelIdA       = 0xf7420afbu; // RelationId.Id_a
constexpr uint32_t kHashRelIdB       = 0x4071f71cu; // RelationId.Id_b
constexpr uint32_t kHashRelBaseType  = 0x8b41897eu; // Relation.DirectionalInfo.BaseRelationType
constexpr uint32_t kHashRelMeter     = 0x42c2fc2fu; // Relation.DirectionalInfo.Meter
constexpr uint32_t kHashRelIsFight   = 0xc2db3a72u; // Relation.Info.IsFight
constexpr uint32_t kHashRelSince     = 0x1a892e50u; // Relation.SinceTimestamp
constexpr uint32_t kHashCoordKey     = 0xd5ee521au;
constexpr uint32_t kHashCoordColor   = 0x763c97ccu;
constexpr uint32_t kHashPocketGoods  = 0xe3d6005eu;
constexpr uint32_t kHashMapHouseId   = 0x0b5276e9u; // House.MapId (Map.sav)
constexpr uint32_t kHashMapGroupName = 0x0d96409eu; // House.RoommateGroupName (Map.sav)

struct WornSlotDef {
    const char* key;
    uint32_t itemHashKey;
    uint32_t colorHashKey;
};

constexpr WornSlotDef kWornDefs[WornClothSlots] = {
    {"All",       0xb20fecedu, 0xee37f256u},
    {"Topslong",  0x35256740u, 0x1124095du},
    {"Tops",      0xe81ee9c4u, 0x77cf0ff7u},
    {"BottomsA",  0x15aea81du, 0xec5010fbu},
    {"BottomsB",  0x7a1809d1u, 0x536dbec2u},
    {"Headwear",  0x98cc9cccu, 0xc499cda5u},
    {"Shoes",     0x0a903d03u, 0xc0529e53u},
    {"Accessory", 0xc1094d9fu, 0x64af701au},
};

struct HashKeyPair {
    uint32_t hash;
    const char* key;
};

constexpr HashKeyPair kMoodTable[] = {
    {0xb6eede09u, "none"},
    {0x390b012du, "normal"},
    {0x62189afbu, "good"},
    {0xab185a53u, "irritate"},
    {0xabc36d9fu, "deject"},
    {0x86e2036fu, "careless"},
    {0xf3046534u, "angry"},
    {0xf54cf52eu, "depress"},
    {0x651b7eddu, "worry"},
};

struct WordKindEntry {
    const char* rawName;
    const char* key;
};

constexpr WordKindEntry kWordKindTable[] = {
    {"Invalid",     "invalid"},
    {"TalkStart",   "talk_start"},
    {"TalkEnd",     "talk_end"},
    {"Phrase",      "phrase"},
    {"Happy",       "happy"},
    {"Sad",         "sad"},
    {"Angry",       "angry"},
    {"Greeting",    "greeting"},
    {"TalkInSleep", "sleep_talk"},
    {"ShoutToSea",  "shout_sea"},
    {"BeforeEat",   "before_eat"},
};

struct RelTypeEntry {
    uint32_t hash;
    const char* key;
    RelationGroup group;
};

constexpr RelTypeEntry kRelTypeTable[] = {
    {0x0784a8dcu, "unknown",       RelationGroup::Other},
    {0x7e3d1e46u, "invalid",       RelationGroup::Other},
    {0x354a0515u, "know",          RelationGroup::Know},
    {0xba939a42u, "friend",        RelationGroup::Friend},
    {0x535e93a8u, "ex_friend",     RelationGroup::Former},
    {0xc2d067a7u, "couple",        RelationGroup::Romance},
    {0xb7ce0c18u, "lover",         RelationGroup::Romance},
    {0x4cfcf0b9u, "oneside_love",  RelationGroup::Romance},
    {0x68ef8aabu, "oneside_love",  RelationGroup::Romance},
    {0x7783d4c3u, "ex_lover",      RelationGroup::Former},
    {0xfe59f825u, "divorce",       RelationGroup::Former},
    {0xdcfc7603u, "parent",        RelationGroup::Family},
    {0xe193c5a2u, "child",         RelationGroup::Family},
    {0x1918f808u, "sibling_old",   RelationGroup::Family},
    {0x3b1d200au, "sibling_yng",   RelationGroup::Family},
    {0x2fd9785bu, "grandparent",   RelationGroup::Family},
    {0x7e3cd550u, "grandchild",    RelationGroup::Family},
    {0x804172f3u, "relative",      RelationGroup::Family},
};

constexpr const char* kHabitCatKeys[HabitCategoryCount] = {
    "walking", "standing", "eating", "greeting", "anger",
    "expression", "voice", "stomach", "other",
};

const char* resolveMoodKey(uint32_t h) {
    for (const auto& entry : kMoodTable)
        if (entry.hash == h)
            return entry.key;
    return nullptr;
}

const char* resolveWordKindKey(uint32_t h) {
    for (const auto& entry : kWordKindTable)
        if (Hash(entry.rawName) == h)
            return entry.key;
    return nullptr;
}

const RelTypeEntry* resolveRelType(uint32_t h) {
    for (const auto& entry : kRelTypeTable)
        if (entry.hash == h)
            return &entry;
    return nullptr;
}

int relTierIndex(int32_t meter) {
    constexpr int32_t T[6] = {11, 41, 81, 120, 160, 190};
    for (int i = 0; i < 6; ++i)
        if (meter < T[i])
            return i;
    return 6;
}

std::string resolveRelTierLabel(uint32_t typeHash, int32_t meter, bool isFight) {
    const int t = relTierIndex(meter);
    switch (typeHash) {
        case 0xc2d067a7u: return isFight ? REL_TIER_COUPLE_FIGHT[t] : REL_TIER_COUPLE[t];
        case 0xba939a42u: return isFight ? REL_TIER_FRIEND_FIGHT[t] : REL_TIER_FRIEND[t];
        case 0xb7ce0c18u: return isFight ? REL_TIER_LOVER_FIGHT[t] : REL_TIER_LOVER[t];
        case 0x4cfcf0b9u:
        case 0x68ef8aabu: return isFight ? REL_TIER_ONESIDELOVE_FIGHT[t] : REL_TIER_ONESIDELOVE[t];
        case 0x354a0515u: return REL_TIER_KNOW[t];
        case 0x535e93a8u: return REL_TIER_EXFRIEND[t];
        case 0x7783d4c3u: return REL_TIER_EXLOVER[t];
        case 0xfe59f825u: return REL_TIER_DIVORCE[t];
        case 0xdcfc7603u: case 0xe193c5a2u:
        case 0x1918f808u: case 0x3b1d200au:
        case 0x2fd9785bu: case 0x7e3cd550u:
        case 0x804172f3u: return REL_TIER_FAMILY[t];
        default: return {};
    }
}

const BlClothItem* findClothItem(uint32_t h) {
    for (int i = 0; i < BL_CLOTH_COUNT; ++i)
        if (BL_CLOTH_ITEMS[i].nameHash == h)
            return &BL_CLOTH_ITEMS[i];
    return nullptr;
}

const BlCoordItem* findCoordItem(uint32_t h) {
    for (int i = 0; i < BL_COORD_COUNT; ++i)
        if (BL_COORD_ITEMS[i].keyHash == h)
            return &BL_COORD_ITEMS[i];
    return nullptr;
}

const BlGoodsItem* findGoodsItem(uint32_t h) {
    for (int i = 0; i < BL_GOODS_COUNT; ++i)
        if (BL_GOODS_ITEMS[i].nameHash == h)
            return &BL_GOODS_ITEMS[i];
    return nullptr;
}

std::vector<bool> computeCharInfoOccupied(const SavView& sav, size_t maxSlots) {
    std::vector<bool> occ(maxSlots, true);
    const auto* e = sav.find(kHashCharInfoEx);
    if (!e || e->type != DT_BinArray)
        return occ;
    const size_t off = e->slot;
    const uint32_t count = readU32(sav.bytes->data() + off);
    size_t p = off + 4u;
    for (size_t i = 0; i < count && i < maxSlots; ++i) {
        const uint32_t elemSize = readU32(sav.bytes->data() + p);
        p += 4u;
        bool anyNonZero = false;
        for (uint32_t j = 0; j < elemSize; ++j) {
            if ((*sav.bytes)[p + j] != 0u) {
                anyNonZero = true;
                break;
            }
        }
        occ[i] = anyNonZero;
        p += elemSize;
    }
    for (size_t i = count; i < maxSlots; ++i)
        occ[i] = false;
    return occ;
}

MiiReadError mapSourceError(TextureBrowser::Error e) {
    switch (e) {
        case TextureBrowser::Error::None: return MiiReadError::None;
        case TextureBrowser::Error::NoSave: return MiiReadError::NoSave;
        case TextureBrowser::Error::Metadata: return MiiReadError::AccountMetadataFailed;
        case TextureBrowser::Error::Open: return MiiReadError::ReadOnlyOpenFailed;
        case TextureBrowser::Error::ShortRead: return MiiReadError::ShortRead;
        case TextureBrowser::Error::Limit: return MiiReadError::FileTooLarge;
        case TextureBrowser::Error::Cancelled: return MiiReadError::Cancelled;
        default: return MiiReadError::ShortRead;
    }
}

std::string trOr(const Localizer& tr, const std::string& key, const std::string& fallback) {
    if (!tr)
        return fallback;
    std::string val = tr(key);
    if (val.empty())
        return fallback;
    return val;
}

std::string moodDisplay(const MiiStats& s, const Localizer& tr) {
    if (!s.moodPresent)
        return trOr(tr, "mii_value_unavailable", "Unavailable");
    if (s.moodKnown)
        return trOr(tr, "mii_mood_" + s.moodKey, s.moodKey);
    return trOr(tr, "mii_unknown_code", "Unknown") + " (" + FormatHex32(s.moodHash) + ")";
}

std::string relTypeDisplay(const MiiRelationEntry& r, const Localizer& tr) {
    if (r.myTypeKnown)
        return trOr(tr, "mii_rel_" + r.myTypeKey, r.myTypeKey);
    return trOr(tr, "mii_unknown_code", "Unknown") + " (" + FormatHex32(r.myTypeHash) + ")";
}

std::string groupDisplay(RelationGroup g, const Localizer& tr) {
    switch (g) {
        case RelationGroup::Friend:  return trOr(tr, "mii_social_group_friend", "Friends");
        case RelationGroup::Romance: return trOr(tr, "mii_social_group_romance", "Romance");
        case RelationGroup::Family:  return trOr(tr, "mii_social_group_family", "Family");
        case RelationGroup::Know:    return trOr(tr, "mii_social_group_know", "Acquaintances");
        case RelationGroup::Former:  return trOr(tr, "mii_social_group_former", "Former");
        default:                     return trOr(tr, "mii_social_group_other", "Other");
    }
}

} // namespace

uint32_t Hash(const char* name) {
    if (!name)
        return 0;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(name);
    const size_t len = std::strlen(name);
    const size_t nblocks = len / 4u;
    uint32_t h1 = 0u;
    constexpr uint32_t c1 = 0xcc9e2d51u;
    constexpr uint32_t c2 = 0x1b873593u;
    for (size_t i = 0; i < nblocks; ++i) {
        uint32_t k1 = readU32(data + i * 4u);
        k1 *= c1;
        k1 = rotl32(k1, 15);
        k1 *= c2;
        h1 ^= k1;
        h1 = rotl32(h1, 13);
        h1 = h1 * 5u + 0xe6546b64u;
    }
    const uint8_t* tail = data + nblocks * 4u;
    uint32_t k1 = 0u;
    switch (len & 3u) {
        case 3: k1 ^= static_cast<uint32_t>(tail[2]) << 16; // fallthrough
        case 2: k1 ^= static_cast<uint32_t>(tail[1]) << 8;  // fallthrough
        case 1: k1 ^= static_cast<uint32_t>(tail[0]);
                k1 *= c1;
                k1 = rotl32(k1, 15);
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

std::string FormatHex32(uint32_t value) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%08X", value);
    return buf;
}

std::string FormatUtcTimestamp(uint64_t unixSeconds) {
    if (unixSeconds == 0)
        return {};
    std::time_t raw = static_cast<std::time_t>(unixSeconds);
    std::tm utc{};
    if (!gmtime_r(&raw, &utc))
        return std::to_string(unixSeconds);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d UTC",
                  utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                  utc.tm_hour, utc.tm_min);
    return buf;
}

Result<MiiRosterSummary> Parse(
    const std::vector<uint8_t>& miiBytes,
    const std::vector<uint8_t>* mapBytes) {
    Result<MiiRosterSummary> out;
    SavView sav;
    const MiiReadError idxErr = parseSavIndex(miiBytes, TextureBrowser::MaxMiiBytes, sav);
    if (idxErr != MiiReadError::None) {
        out.status = {idxErr};
        return out;
    }

    const auto* nameEntry = sav.find(kHashMiiName);
    if (!nameEntry) {
        out.status = {MiiReadError::MissingRequiredField};
        return out;
    }
    if (nameEntry->type != DT_WStr32Array) {
        out.status = {MiiReadError::TypeMismatch};
        return out;
    }

    out.value.saveVersion = sav.version;

    // Optional Map.sav parsing for custom house names (best-effort; never invalidates Mii.sav).
    std::map<int32_t, std::string> houseNameByMapId;
    if (mapBytes != nullptr) {
        SavView mapSav;
        if (parseSavIndex(*mapBytes, TextureBrowser::MaxMapBytes, mapSav) == MiiReadError::None) {
            const uint32_t idCount = mapSav.arrayCount(kHashMapHouseId);
            const uint32_t nameCount = mapSav.arrayCount(kHashMapGroupName);
            const uint32_t n = std::min(idCount, nameCount);
            if (n > 0) {
                out.value.mapNamesAvailable = true;
                for (uint32_t i = 0; i < n; ++i) {
                    int32_t mapId = -1;
                    std::string groupName;
                    bool clean = true;
                    if (mapSav.readIntAt(kHashMapHouseId, i, mapId) && mapId >= 0 &&
                        mapSav.readWStrAt(kHashMapGroupName, i, groupName, clean)) {
                        if (!groupName.empty())
                            houseNameByMapId[mapId] = groupName;
                        if (!clean)
                            out.value.mapNamesWarning = true;
                    }
                }
            } else {
                out.value.mapNamesWarning = true;
            }
        } else {
            out.value.mapNamesWarning = true;
        }
    }

    const size_t nameCount = std::min<size_t>(MaxSlots, sav.arrayCount(kHashMiiName));
    const std::vector<bool> charInfoOcc = computeCharInfoOccupied(sav, nameCount);

    // Precompute habit hashes once per call (74 habits * 3 = 222 hashes, negligible).
    const uint32_t hHabitOwn = Hash("Own");
    const uint32_t hHabitUnown = Hash("Unown");
    const uint32_t hHabitNeverOwned = Hash("NeverOwned");
    struct HabitHashes { uint32_t isOwn, isChecked, state; };
    std::vector<HabitHashes> habitHashes(static_cast<size_t>(HABIT_COUNT));
    for (int h = 0; h < HABIT_COUNT; ++h) {
        std::string base = std::string("Mii.Belongings.HabitOwnInfo.") + HABITS[h].name;
        habitHashes[h].isOwn = Hash((base + ".IsOwn").c_str());
        habitHashes[h].isChecked = Hash((base + ".IsChecked").c_str());
        habitHashes[h].state = Hash((base + ".State").c_str());
    }
    (void)HABIT_CAT_NAME;
    (void)HABIT_CAT_LABEL;

    std::array<int, MaxSlots> slotToResidentIdx;
    slotToResidentIdx.fill(-1);

    for (size_t idx = 0; idx < nameCount; ++idx) {
        std::string name;
        bool clean = true;
        if (!sav.readWStrAt(kHashMiiName, idx, name, clean))
            continue;
        if (name.empty() || !charInfoOcc[idx])
            continue;

        MiiResident r;
        r.slotIndex = static_cast<int>(idx);
        r.slotNumber = static_cast<int>(idx + 1);

        // 1. Stats (16 fields)
        r.stats.name = std::move(name);
        r.stats.nameTextWarning = !clean;
        if (!clean) r.hasWarnings = true;

        r.stats.levelPresent = sav.readIntAt(kHashLevel, idx, r.stats.levelRaw);
        r.stats.levelMeterPresent = sav.readIntAt(kHashLevelMeter, idx, r.stats.levelMeter);
        r.stats.moneyPresent = sav.readUIntAt(kHashMoney, idx, r.stats.money);
        r.stats.bdayDayPresent = sav.readIntAt(kHashBdayDay, idx, r.stats.bdayDay);
        r.stats.bdayMonthPresent = sav.readIntAt(kHashBdayMonth, idx, r.stats.bdayMonth);
        r.stats.bdayYearPresent = sav.readIntAt(kHashBdayYear, idx, r.stats.bdayYear);
        r.stats.directAgePresent = sav.readIntAt(kHashDirectAge, idx, r.stats.directAge);
        r.stats.activenessPresent = sav.readIntAt(kHashActiveness, idx, r.stats.activeness);
        r.stats.audacityPresent = sav.readIntAt(kHashAudacity, idx, r.stats.audacity);
        r.stats.commonsensePresent = sav.readIntAt(kHashCommonsense, idx, r.stats.commonsense);
        r.stats.gaietyPresent = sav.readIntAt(kHashGaiety, idx, r.stats.gaiety);
        r.stats.sociabilityPresent = sav.readIntAt(kHashSociability, idx, r.stats.sociability);
        r.stats.bondMeterPresent = sav.readIntAt(kHashBondMeter, idx, r.stats.bondMeter);
        r.stats.fullnessPresent = sav.readIntAt(kHashFullness, idx, r.stats.fullness);

        if (sav.readUIntAt(kHashFeelingType, idx, r.stats.moodHash)) {
            r.stats.moodPresent = true;
            if (const char* mk = resolveMoodKey(r.stats.moodHash)) {
                r.stats.moodKnown = true;
                r.stats.moodKey = mk;
            } else {
                r.hasWarnings = true;
            }
        }
        if ((r.stats.bdayDayPresent && !r.stats.bdayDayValid()) ||
            (r.stats.bdayMonthPresent && !r.stats.bdayMonthValid())) {
            r.hasWarnings = true;
        }

        // 2. Words (12 slots per Mii)
        for (size_t w = 0; w < WordsPerMii; ++w) {
            MiiWordSlot& ws = r.words[w];
            ws.slotNumber = static_cast<int>(w + 1);
            const size_t ai = idx * WordsPerMii + w;
            uint32_t kh = kHashInvalidEnum;
            if (sav.readUIntAt(kHashWordKind, ai, kh)) {
                ws.kindHash = kh;
                ws.isEmpty = (kh == 0u || kh == kHashInvalidEnum);
                if (const char* kk = resolveWordKindKey(kh)) {
                    ws.kindKnown = true;
                    ws.kindKey = kk;
                } else if (!ws.isEmpty) {
                    r.hasWarnings = true;
                }
            }
            bool tClean = true, hClean = true;
            sav.readWStrAt(kHashWordText, ai, ws.text, tClean);
            sav.readWStrAt(kHashWordHow, ai, ws.howToCall, hClean);
            ws.textWarning = !tClean || !hClean;
            if (ws.textWarning)
                r.hasWarnings = true;
        }

        // 4. Belongings (8 worn cloth slots + Coordinate + 12 pocket slots)
        for (size_t s = 0; s < WornClothSlots; ++s) {
            MiiWornClothSlot& slot = r.belongings.wornSlots[s];
            slot.slotKey = kWornDefs[s].key;
            uint32_t ih = kHashInvalidEnum;
            int32_t col = 0;
            sav.readUIntAt(kWornDefs[s].itemHashKey, idx, ih);
            sav.readIntAt(kWornDefs[s].colorHashKey, idx, col);
            slot.itemHash = ih;
            slot.colorIndex = col;
            slot.isEmpty = (ih == 0u || ih == kHashInvalidEnum);
            if (!slot.isEmpty) {
                if (const auto* item = findClothItem(ih)) {
                    slot.known = true;
                    slot.label = item->label;
                    slot.colorCount = item->colorCount;
                } else {
                    r.hasWarnings = true;
                }
            }
        }
        {
            MiiWornClothSlot& coord = r.belongings.coordinate;
            coord.slotKey = "Coordinate";
            uint32_t ch = kHashInvalidEnum;
            int32_t col = 0;
            sav.readUIntAt(kHashCoordKey, idx, ch);
            sav.readIntAt(kHashCoordColor, idx, col);
            coord.itemHash = ch;
            coord.colorIndex = col;
            coord.isEmpty = (ch == 0u || ch == kHashInvalidEnum);
            if (!coord.isEmpty) {
                if (const auto* item = findCoordItem(ch)) {
                    coord.known = true;
                    coord.label = item->label;
                    coord.colorCount = item->colorCount;
                } else {
                    r.hasWarnings = true;
                }
            }
        }
        for (size_t p = 0; p < PocketSlotsPerMii; ++p) {
            MiiPocketSlot& ps = r.belongings.pocket[p];
            ps.slotNumber = static_cast<int>(p + 1);
            uint32_t gh = 0u;
            sav.readUIntAt(kHashPocketGoods, idx * PocketSlotsPerMii + p, gh);
            ps.itemHash = gh;
            ps.isEmpty = (gh == 0u || gh == kHashInvalidEnum);
            if (!ps.isEmpty) {
                if (const auto* item = findGoodsItem(gh)) {
                    ps.known = true;
                    ps.label = item->label;
                } else {
                    r.hasWarnings = true;
                }
            }
        }

        // 5. Habits (74 habits across 9 categories)
        for (size_t c = 0; c < HabitCategoryCount; ++c) {
            r.habitCategories[c].category = static_cast<int>(c);
            r.habitCategories[c].categoryKey = kHabitCatKeys[c];
        }
        for (int h = 0; h < HABIT_COUNT; ++h) {
            const int cat = HABITS[h].category;
            if (cat < 0 || cat >= static_cast<int>(HabitCategoryCount))
                continue;
            MiiHabitEntry he;
            he.habitIndex = h;
            he.category = cat;
            he.label = HABITS[h].label;
            sav.readBoolAt(habitHashes[h].isOwn, idx, he.isOwn);
            sav.readBoolAt(habitHashes[h].isChecked, idx, he.isChecked);
            uint32_t st = hHabitNeverOwned;
            if (sav.readUIntAt(habitHashes[h].state, idx, st)) {
                he.stateHash = st;
                if (st == hHabitOwn)
                    he.state = HabitState::Owned;
                else if (st == hHabitUnown)
                    he.state = HabitState::Unowned;
                else if (st == hHabitNeverOwned)
                    he.state = HabitState::NeverOwned;
                else {
                    he.state = HabitState::Unknown;
                    r.hasWarnings = true;
                }
            }
            auto& bucket = r.habitCategories[static_cast<size_t>(cat)];
            bucket.totalCount++;
            if (he.isOwn) bucket.ownedCount++;
            if (he.isOwn && he.isChecked) bucket.activeCount++;
            bucket.entries.push_back(std::move(he));
        }

        // 6. Housing base fields (housemates resolved in second pass)
        int32_t mapId = -1;
        int32_t roomIdx = 0;
        if (sav.readIntAt(kHashHouseMapId, idx, mapId))
            r.housing.houseMapId = mapId;
        if (sav.readIntAt(kHashRoomIndex, idx, roomIdx))
            r.housing.roomIndex = roomIdx;
        r.housing.housed = (r.housing.houseMapId >= 0);
        if (r.housing.housed) {
            auto it = houseNameByMapId.find(r.housing.houseMapId);
            if (it != houseNameByMapId.end()) {
                r.housing.houseName = it->second;
                r.housing.houseNameFromMap = true;
            }
        }

        slotToResidentIdx[idx] = static_cast<int>(out.value.residents.size());
        out.value.residents.push_back(std::move(r));
    }

    // Second pass: Housing housemates & room conflicts.
    for (size_t i = 0; i < out.value.residents.size(); ++i) {
        auto& r = out.value.residents[i];
        if (!r.housing.housed)
            continue;
        for (size_t j = 0; j < out.value.residents.size(); ++j) {
            if (i == j)
                continue;
            const auto& other = out.value.residents[j];
            if (other.housing.housed && other.housing.houseMapId == r.housing.houseMapId) {
                const bool conflict = (other.housing.roomIndex == r.housing.roomIndex);
                if (conflict)
                    r.housing.hasRoomConflict = true;
                r.housing.housemates.push_back({
                    other.slotNumber,
                    other.stats.name,
                    other.housing.roomIndex,
                    conflict,
                });
            }
        }
    }

    // Third pass: Relations & Social summary across all pairs.
    const uint32_t pairCount = std::min(sav.arrayCount(kHashRelIdA), sav.arrayCount(kHashRelIdB));
    for (uint32_t p = 0; p < pairCount; ++p) {
        int32_t idA = -1, idB = -1;
        if (!sav.readIntAt(kHashRelIdA, p, idA) || !sav.readIntAt(kHashRelIdB, p, idB))
            continue;
        if (idA < 0 || idB < 0 || idA == idB)
            continue;
        uint32_t typeAB = kHashInvalidEnum, typeBA = kHashInvalidEnum;
        int32_t meterAB = 0, meterBA = 0;
        bool isFight = false;
        uint64_t sinceTs = 0;
        sav.readUIntAt(kHashRelBaseType, static_cast<size_t>(p) * 2u, typeAB);
        sav.readUIntAt(kHashRelBaseType, static_cast<size_t>(p) * 2u + 1u, typeBA);
        sav.readIntAt(kHashRelMeter, static_cast<size_t>(p) * 2u, meterAB);
        sav.readIntAt(kHashRelMeter, static_cast<size_t>(p) * 2u + 1u, meterBA);
        sav.readBoolAt(kHashRelIsFight, p, isFight);
        sav.readUInt64At(kHashRelSince, p, sinceTs);

        auto attachDir = [&](int selfSlot, int otherSlot, uint32_t myType, uint32_t otherType, int32_t myMeter) {
            if (selfSlot < 0 || selfSlot >= static_cast<int>(MaxSlots))
                return;
            const int resIdx = slotToResidentIdx[static_cast<size_t>(selfSlot)];
            if (resIdx < 0)
                return;
            if (myType == 0u || myType == kHashInvalidEnum)
                return;

            auto& res = out.value.residents[static_cast<size_t>(resIdx)];
            MiiRelationEntry entry;
            entry.pairIndex = static_cast<int>(p);
            entry.otherSlotIndex = otherSlot;
            entry.otherSlotNumber = otherSlot + 1;
            if (otherSlot >= 0 && otherSlot < static_cast<int>(MaxSlots) &&
                slotToResidentIdx[static_cast<size_t>(otherSlot)] >= 0) {
                entry.otherSlotOccupied = true;
                entry.otherName = out.value.residents[static_cast<size_t>(slotToResidentIdx[static_cast<size_t>(otherSlot)])].stats.name;
            } else {
                entry.otherSlotOccupied = false;
                res.hasWarnings = true;
            }
            entry.myTypeHash = myType;
            entry.otherTypeHash = otherType;
            if (const auto* rt = resolveRelType(myType)) {
                entry.myTypeKnown = true;
                entry.myTypeKey = rt->key;
                entry.categoryGroup = rt->group;
            } else {
                entry.myTypeKnown = false;
                entry.categoryGroup = RelationGroup::Other;
                res.hasWarnings = true;
            }
            entry.meter = myMeter;
            entry.meterFixed = (myType == 0x0784a8dcu || myType == kHashInvalidEnum);
            entry.isFight = isFight;
            entry.sinceUnixSeconds = sinceTs;
            entry.tierLabel = resolveRelTierLabel(myType, myMeter, isFight);
            res.relations.push_back(std::move(entry));
        };

        attachDir(idA, idB, typeAB, typeBA, meterAB);
        attachDir(idB, idA, typeBA, typeAB, meterBA);
    }

    // Build SocialSummary for each resident.
    for (auto& r : out.value.residents) {
        std::sort(r.relations.begin(), r.relations.end(), [](const MiiRelationEntry& a, const MiiRelationEntry& b) {
            if (a.otherSlotNumber != b.otherSlotNumber)
                return a.otherSlotNumber < b.otherSlotNumber;
            return a.pairIndex < b.pairIndex;
        });
        r.social.totalConnections = r.relations.size();
        for (const auto& rel : r.relations) {
            switch (rel.categoryGroup) {
                case RelationGroup::Friend:  r.social.friends++; break;
                case RelationGroup::Romance: r.social.romance++; break;
                case RelationGroup::Family:  r.social.family++; break;
                case RelationGroup::Know:    r.social.know++; break;
                case RelationGroup::Former:  r.social.former++; break;
                default:                     r.social.other++; break;
            }
            if (rel.isFight)
                r.social.activeFights++;
        }
        r.social.groupedRelations = r.relations;
        std::sort(r.social.groupedRelations.begin(), r.social.groupedRelations.end(),
                  [](const MiiRelationEntry& a, const MiiRelationEntry& b) {
                      if (a.categoryGroup != b.categoryGroup)
                          return static_cast<uint8_t>(a.categoryGroup) < static_cast<uint8_t>(b.categoryGroup);
                      if (a.meter != b.meter)
                          return a.meter > b.meter;
                      return a.otherSlotNumber < b.otherSlotNumber;
                  });
        if (r.hasWarnings)
            out.value.hasFieldWarnings = true;
    }
    if (out.value.mapNamesWarning)
        out.value.hasFieldWarnings = true;

    return out;
}

Result<MiiRosterSummary> LoadFromReadSource(
    TextureBrowser::ReadSource& source,
    TextureBrowser::AccountKey account,
    const TextureBrowser::Cancel& cancel) {
    Result<MiiRosterSummary> out;
    if (!account.valid()) {
        out.status = {MiiReadError::NoAccountSelected};
        return out;
    }
    if (cancel && cancel()) {
        out.status = {MiiReadError::Cancelled};
        return out;
    }

    const auto openStatus = source.open(account, cancel ? cancel : TextureBrowser::Cancel{[] { return false; }});
    if (!openStatus) {
        out.status = {mapSourceError(openStatus.error), openStatus.platform};
        return out;
    }
    ScopeGuard<std::function<void()>> closeGuard{[&] { source.close(); }};

    auto miiData = source.read("/Mii.sav", TextureBrowser::MaxMiiBytes,
                               cancel ? cancel : TextureBrowser::Cancel{[] { return false; }});
    if (!miiData) {
        out.status = {mapSourceError(miiData.status.error), miiData.status.platform};
        return out;
    }

    // Best-effort optional read of /Map.sav for custom house names.
    bool mapReadOk = false;
    std::vector<uint8_t> mapBytes;
    if (!cancel || !cancel()) {
        auto mapData = source.read("/Map.sav", TextureBrowser::MaxMapBytes,
                                   cancel ? cancel : TextureBrowser::Cancel{[] { return false; }});
        if (mapData) {
            mapBytes = std::move(mapData.value);
            mapReadOk = true;
        }
    }

    auto parsed = Parse(miiData.value, mapReadOk ? &mapBytes : nullptr);
    if (parsed && !mapReadOk) {
        parsed.value.mapNamesAvailable = false;
        parsed.value.mapNamesWarning = true;
        parsed.value.hasFieldWarnings = true;
    }
    return parsed;
}

std::string FormatResidentSummary(const MiiResident& resident, const Localizer& tr) {
    const std::string slotTag = trOr(tr, "mii_slot_short", "Slot") + " #" + std::to_string(resident.slotNumber);
    const std::string lvTag = resident.stats.levelPresent
        ? (trOr(tr, "mii_level_short", "Lv.") + " " + std::to_string(resident.stats.displayLevel()))
        : trOr(tr, "mii_value_unavailable", "Unavailable");
    return slotTag + " · " + lvTag + " · " + moodDisplay(resident.stats, tr);
}

std::array<MiiSectionOverview, SectionCount> FormatSectionOverviews(
    const MiiResident& resident,
    const Localizer& tr) {
    size_t filledWords = 0;
    for (const auto& w : resident.words)
        if (!w.isEmpty) ++filledWords;

    size_t equippedWorn = 0;
    for (const auto& s : resident.belongings.wornSlots)
        if (!s.isEmpty) ++equippedWorn;
    size_t pocketUsed = 0;
    for (const auto& p : resident.belongings.pocket)
        if (!p.isEmpty) ++pocketUsed;

    int totalOwnedHabits = 0, totalHabits = 0;
    for (const auto& c : resident.habitCategories) {
        totalOwnedHabits += c.ownedCount;
        totalHabits += c.totalCount;
    }

    std::string housingSummary;
    if (!resident.housing.housed) {
        housingSummary = trOr(tr, "mii_housing_unhoused", "Not housed");
    } else {
        const std::string houseLabel = resident.housing.houseNameFromMap && !resident.housing.houseName.empty()
            ? resident.housing.houseName
            : (trOr(tr, "mii_house_fallback", "House") + " #" + std::to_string(resident.housing.houseMapId));
        housingSummary = houseLabel + " · " + trOr(tr, "mii_room_short", "Room") + " " +
                         std::to_string(resident.housing.roomIndex + 1);
    }

    const std::string statsSummary = resident.stats.levelPresent
        ? (trOr(tr, "mii_level_short", "Lv.") + " " + std::to_string(resident.stats.displayLevel()) +
           " · " + moodDisplay(resident.stats, tr))
        : moodDisplay(resident.stats, tr);

    const std::string belongingsSummary = !resident.belongings.coordinate.isEmpty
        ? (trOr(tr, "mii_belongings_coord", "Coordinate") + " · " +
           std::to_string(pocketUsed) + "/" + std::to_string(PocketSlotsPerMii))
        : (std::to_string(equippedWorn) + "/" + std::to_string(WornClothSlots) + " · " +
           std::to_string(pocketUsed) + "/" + std::to_string(PocketSlotsPerMii));

    return {{
        {MiiSectionId::Stats,      "sec_stats",      trOr(tr, "mii_sec_stats", "Stats"),            statsSummary},
        {MiiSectionId::Words,      "sec_words",      trOr(tr, "mii_sec_words", "Words"),            std::to_string(filledWords) + " / " + std::to_string(WordsPerMii)},
        {MiiSectionId::Relations,  "sec_relations",  trOr(tr, "mii_sec_relations", "Relations"),    std::to_string(resident.relations.size())},
        {MiiSectionId::Belongings, "sec_belongings", trOr(tr, "mii_sec_belongings", "Belongings"),  belongingsSummary},
        {MiiSectionId::Habits,     "sec_habits",     trOr(tr, "mii_sec_habits", "Habits"),          std::to_string(totalOwnedHabits) + " / " + std::to_string(totalHabits)},
        {MiiSectionId::Housing,    "sec_housing",    trOr(tr, "mii_sec_housing", "Housing"),        housingSummary},
        {MiiSectionId::Social,     "sec_social",     trOr(tr, "mii_sec_social", "Social Summary"),  std::to_string(resident.social.totalConnections)},
    }};
}

std::vector<MiiRowView> FormatSectionRows(
    const MiiResident& resident,
    MiiSectionId section,
    const Localizer& tr) {
    std::vector<MiiRowView> rows;
    const std::string unavail = trOr(tr, "mii_value_unavailable", "Unavailable");
    const std::string emptyLbl = trOr(tr, "mii_value_empty", "(empty)");

    switch (section) {
        case MiiSectionId::Stats: {
            const auto& s = resident.stats;
            rows.push_back({trOr(tr, "mii_stat_name", "Name"), s.name,
                            s.nameTextWarning ? trOr(tr, "mii_text_warning", "Invalid UTF-16 sequence replaced") : "",
                            s.nameTextWarning});
            rows.push_back({trOr(tr, "mii_stat_level", "Level"),
                            s.levelPresent ? std::to_string(s.displayLevel()) : unavail, "", false});
            rows.push_back({trOr(tr, "mii_stat_level_meter", "Level progress"),
                            s.levelMeterPresent ? (std::to_string(s.levelMeter) + " / 100") : unavail, "", false});
            rows.push_back({trOr(tr, "mii_stat_money", "Money"),
                            s.moneyPresent ? std::to_string(s.money) : unavail, "", false});
            rows.push_back({trOr(tr, "mii_stat_bday_day", "Birthday day"),
                            s.bdayDayPresent ? std::to_string(s.bdayDay) : unavail,
                            (s.bdayDayPresent && !s.bdayDayValid()) ? trOr(tr, "mii_out_of_range", "Out of expected range") : "",
                            s.bdayDayPresent && !s.bdayDayValid()});
            rows.push_back({trOr(tr, "mii_stat_bday_month", "Birthday month"),
                            s.bdayMonthPresent ? std::to_string(s.bdayMonth) : unavail,
                            (s.bdayMonthPresent && !s.bdayMonthValid()) ? trOr(tr, "mii_out_of_range", "Out of expected range") : "",
                            s.bdayMonthPresent && !s.bdayMonthValid()});
            rows.push_back({trOr(tr, "mii_stat_bday_year", "Birthday year"),
                            s.bdayYearPresent ? std::to_string(s.bdayYear) : unavail, "", false});
            rows.push_back({trOr(tr, "mii_stat_direct_age", "Direct age"),
                            !s.directAgePresent ? unavail : (s.directAge == -1 ? trOr(tr, "mii_age_auto", "Automatic (-1)") : std::to_string(s.directAge)),
                            "", false});
            rows.push_back({trOr(tr, "mii_stat_activeness", "Activeness"),
                            s.activenessPresent ? (std::to_string(s.activeness) + " / 10") : unavail, "", false});
            rows.push_back({trOr(tr, "mii_stat_audacity", "Audacity"),
                            s.audacityPresent ? (std::to_string(s.audacity) + " / 10") : unavail, "", false});
            rows.push_back({trOr(tr, "mii_stat_commonsense", "Common sense"),
                            s.commonsensePresent ? (std::to_string(s.commonsense) + " / 10") : unavail, "", false});
            rows.push_back({trOr(tr, "mii_stat_gaiety", "Gaiety"),
                            s.gaietyPresent ? (std::to_string(s.gaiety) + " / 10") : unavail, "", false});
            rows.push_back({trOr(tr, "mii_stat_sociability", "Sociability"),
                            s.sociabilityPresent ? (std::to_string(s.sociability) + " / 10") : unavail, "", false});
            rows.push_back({trOr(tr, "mii_stat_bond_meter", "Bond meter"),
                            s.bondMeterPresent ? (std::to_string(s.bondMeter) + " / 100") : unavail, "", false});
            rows.push_back({trOr(tr, "mii_stat_mood", "Mood"),
                            moodDisplay(s, tr), "", s.moodPresent && !s.moodKnown});
            rows.push_back({trOr(tr, "mii_stat_fullness", "Fullness"),
                            s.fullnessPresent ? std::to_string(s.fullness) : unavail, "", false});
            break;
        }
        case MiiSectionId::Words: {
            for (const auto& w : resident.words) {
                const std::string slotTitle = trOr(tr, "mii_word_slot", "Phrase") + " #" + std::to_string(w.slotNumber);
                if (w.isEmpty) {
                    rows.push_back({slotTitle, emptyLbl, "", false});
                    continue;
                }
                const std::string kindLbl = w.kindKnown
                    ? trOr(tr, "mii_word_kind_" + w.kindKey, w.kindKey)
                    : (trOr(tr, "mii_unknown_code", "Unknown") + " (" + FormatHex32(w.kindHash) + ")");
                std::string secondary;
                if (!w.text.empty())
                    secondary = w.text;
                if (!w.howToCall.empty()) {
                    if (!secondary.empty()) secondary += "\n";
                    secondary += trOr(tr, "mii_word_pronunciation", "Pronunciation") + ": " + w.howToCall;
                }
                if (w.textWarning) {
                    if (!secondary.empty()) secondary += "\n";
                    secondary += trOr(tr, "mii_text_warning", "Invalid UTF-16 sequence replaced");
                }
                rows.push_back({slotTitle, kindLbl, secondary, !w.kindKnown || w.textWarning});
            }
            break;
        }
        case MiiSectionId::Relations: {
            if (resident.relations.empty()) {
                rows.push_back({trOr(tr, "mii_relations_empty", "No relationships"), "", "", false});
                break;
            }
            for (const auto& rel : resident.relations) {
                const std::string target = rel.otherSlotOccupied
                    ? (rel.otherName + " (#" + std::to_string(rel.otherSlotNumber) + ")")
                    : (trOr(tr, "mii_slot_empty_ref", "Empty slot") + " #" + std::to_string(rel.otherSlotNumber));
                const std::string primary = relTypeDisplay(rel, tr) +
                    (rel.meterFixed ? "" : (" · " + std::to_string(rel.meter)));
                std::string secondary;
                if (!rel.tierLabel.empty())
                    secondary = rel.tierLabel;
                if (rel.isFight) {
                    if (!secondary.empty()) secondary += " · ";
                    secondary += trOr(tr, "mii_rel_fighting", "Fighting");
                }
                if (rel.sinceUnixSeconds > 0) {
                    if (!secondary.empty()) secondary += "\n";
                    secondary += trOr(tr, "mii_rel_since", "Since") + ": " + FormatUtcTimestamp(rel.sinceUnixSeconds);
                }
                rows.push_back({target, primary, secondary, !rel.otherSlotOccupied || !rel.myTypeKnown});
            }
            break;
        }
        case MiiSectionId::Belongings: {
            const auto& coord = resident.belongings.coordinate;
            if (!coord.isEmpty) {
                const std::string coordVal = coord.known
                    ? (coord.label + (coord.colorCount > 1 ? (" (#" + std::to_string(coord.colorIndex + 1) + ")") : ""))
                    : (trOr(tr, "mii_unknown_code", "Unknown") + " (" + FormatHex32(coord.itemHash) + ")");
                rows.push_back({trOr(tr, "mii_belongings_coord", "Coordinate"), coordVal,
                                trOr(tr, "mii_belongings_coord_note", "Full outfit overrides individual clothes"),
                                !coord.known});
            } else {
                rows.push_back({trOr(tr, "mii_belongings_coord", "Coordinate"), emptyLbl, "", false});
            }
            for (const auto& s : resident.belongings.wornSlots) {
                const std::string title = trOr(tr, "mii_cloth_" + s.slotKey, s.slotKey);
                if (s.isEmpty) {
                    rows.push_back({title, emptyLbl, "", false});
                } else {
                    const std::string val = s.known
                        ? (s.label + (s.colorCount > 1 ? (" (#" + std::to_string(s.colorIndex + 1) + ")") : ""))
                        : (trOr(tr, "mii_unknown_code", "Unknown") + " (" + FormatHex32(s.itemHash) + ")");
                    rows.push_back({title, val, "", !s.known});
                }
            }
            for (const auto& p : resident.belongings.pocket) {
                const std::string title = trOr(tr, "mii_pocket_slot", "Pocket") + " #" + std::to_string(p.slotNumber);
                if (p.isEmpty) {
                    rows.push_back({title, emptyLbl, "", false});
                } else {
                    const std::string val = p.known
                        ? p.label
                        : (trOr(tr, "mii_unknown_code", "Unknown") + " (" + FormatHex32(p.itemHash) + ")");
                    rows.push_back({title, val, "", !p.known});
                }
            }
            break;
        }
        case MiiSectionId::Habits: {
            for (const auto& cat : resident.habitCategories) {
                const std::string catTitle = trOr(tr, "mii_habit_cat_" + cat.categoryKey, cat.categoryKey);
                const std::string counts = std::to_string(cat.ownedCount) + "/" + std::to_string(cat.totalCount) +
                    " (" + trOr(tr, "mii_habit_active", "active") + ": " + std::to_string(cat.activeCount) + ")";
                std::string secondary;
                bool catWarn = false;
                for (const auto& e : cat.entries) {
                    if (e.state == HabitState::Unknown)
                        catWarn = true;
                    if (e.isOwn || e.state == HabitState::Unknown) {
                        if (!secondary.empty()) secondary += "\n";
                        secondary += (e.isOwn && e.isChecked) ? "[*] " : "[o] ";
                        secondary += e.label;
                        if (e.state == HabitState::Unknown)
                            secondary += " (" + FormatHex32(e.stateHash) + ")";
                    }
                }
                if (secondary.empty())
                    secondary = trOr(tr, "mii_habit_none_owned", "No habits owned in this category");
                rows.push_back({catTitle, counts, secondary, catWarn});
            }
            break;
        }
        case MiiSectionId::Housing: {
            const auto& h = resident.housing;
            if (!h.housed) {
                rows.push_back({trOr(tr, "mii_housing_status", "Housing"),
                                trOr(tr, "mii_housing_unhoused", "Not housed"), "", false});
                break;
            }
            const std::string houseVal = h.houseNameFromMap && !h.houseName.empty()
                ? (h.houseName + " (#" + std::to_string(h.houseMapId) + ")")
                : (trOr(tr, "mii_house_fallback", "House") + " #" + std::to_string(h.houseMapId));
            rows.push_back({trOr(tr, "mii_housing_house", "House"), houseVal,
                            h.houseNameFromMap ? "" : trOr(tr, "mii_housing_map_fallback", "Map.sav house name unavailable; showing house number"),
                            !h.houseNameFromMap});
            rows.push_back({trOr(tr, "mii_housing_room", "Room"),
                            std::to_string(h.roomIndex + 1),
                            h.hasRoomConflict ? trOr(tr, "mii_housing_conflict", "Room conflict with another resident") : "",
                            h.hasRoomConflict});
            if (h.housemates.empty()) {
                rows.push_back({trOr(tr, "mii_housing_mates", "Housemates"),
                                trOr(tr, "mii_housing_alone", "Lives alone"), "", false});
            } else {
                for (const auto& mate : h.housemates) {
                    std::string detail = trOr(tr, "mii_room_short", "Room") + " " + std::to_string(mate.roomIndex + 1);
                    if (mate.sameRoomConflict)
                        detail += " · " + trOr(tr, "mii_housing_conflict_short", "Conflict");
                    rows.push_back({mate.name + " (#" + std::to_string(mate.slotNumber) + ")",
                                    detail, "", mate.sameRoomConflict});
                }
            }
            break;
        }
        case MiiSectionId::Social: {
            const auto& soc = resident.social;
            rows.push_back({trOr(tr, "mii_social_total", "Total connections"), std::to_string(soc.totalConnections), "", false});
            rows.push_back({trOr(tr, "mii_social_group_friend", "Friends"), std::to_string(soc.friends), "", false});
            rows.push_back({trOr(tr, "mii_social_group_romance", "Romance"), std::to_string(soc.romance), "", false});
            rows.push_back({trOr(tr, "mii_social_group_family", "Family"), std::to_string(soc.family), "", false});
            rows.push_back({trOr(tr, "mii_social_group_know", "Acquaintances"), std::to_string(soc.know), "", false});
            rows.push_back({trOr(tr, "mii_social_group_former", "Former"), std::to_string(soc.former), "", false});
            rows.push_back({trOr(tr, "mii_social_fights", "Active fights"), std::to_string(soc.activeFights), "", soc.activeFights > 0});
            for (const auto& rel : soc.groupedRelations) {
                const std::string target = rel.otherSlotOccupied
                    ? (rel.otherName + " (#" + std::to_string(rel.otherSlotNumber) + ")")
                    : (trOr(tr, "mii_slot_empty_ref", "Empty slot") + " #" + std::to_string(rel.otherSlotNumber));
                const std::string primary = groupDisplay(rel.categoryGroup, tr) + " · " + relTypeDisplay(rel, tr);
                std::string secondary = rel.meterFixed ? "" : (trOr(tr, "mii_rel_meter", "Meter") + ": " + std::to_string(rel.meter));
                if (!rel.tierLabel.empty()) {
                    if (!secondary.empty()) secondary += " · ";
                    secondary += rel.tierLabel;
                }
                if (rel.isFight) {
                    if (!secondary.empty()) secondary += " · ";
                    secondary += trOr(tr, "mii_rel_fighting", "Fighting");
                }
                rows.push_back({target, primary, secondary, !rel.otherSlotOccupied || !rel.myTypeKnown});
            }
            break;
        }
    }
    return rows;
}

void MiiBrowserModel::select(TextureBrowser::Account account) {
    if (!account.key.valid()) {
        hasAccount = false;
        context = {};
        summary = {};
        status = {MiiReadError::NoAccountSelected};
        state = MiiBrowserState::Unselected;
        ++generation;
        return;
    }
    hasAccount = true;
    context = {};
    context.account = std::move(account);
    summary = {};
    status = {};
    state = MiiBrowserState::Loading;
    ++generation;
}

void MiiBrowserModel::loading() {
    if (!hasAccount) {
        status = {MiiReadError::NoAccountSelected};
        state = MiiBrowserState::Unselected;
        return;
    }
    status = {};
    state = MiiBrowserState::Loading;
    ++generation;
}

void MiiBrowserModel::accept(Result<MiiRosterSummary> result) {
    if (result.status.error == MiiReadError::Cancelled)
        return;
    status = result.status;
    if (result) {
        summary = std::move(result.value);
        state = summary.residents.empty() ? MiiBrowserState::Empty : MiiBrowserState::Ready;
    } else {
        summary = {};
        state = MiiBrowserState::Error;
    }
}

void MiiBrowserModel::suspend() {
    if (state == MiiBrowserState::Loading) {
        state = MiiBrowserState::Suspended;
        ++generation;
    }
}

bool MiiBrowserModel::resume() {
    if (state == MiiBrowserState::Suspended && hasAccount) {
        state = MiiBrowserState::Loading;
        ++generation;
        return true;
    }
    return false;
}

} // namespace MiiRoster
