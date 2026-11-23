// Read-only Player.sav parser, catalog resolver, formatter, and session model.
// Binary format, Murmur3 hashes, and catalog options follow TomoToolNX save_editor.cpp
// and alexislours/ltd-save-editor. Strictly read-only; no save mutation or commit paths.
#include "player_profile.h"
#include "wishes_data.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <set>

namespace PlayerProfile {
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

bool isScalarCompatible(uint32_t t) {
    return t == DT_Bool || t == DT_Int || t == DT_UInt || t == DT_Enum;
}

bool isStringCompatible(uint32_t t) {
    return t == DT_WStr32 || t == DT_WStr64;
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

bool validateHeapEntry(const std::vector<uint8_t>& bytes, size_t saveDataOff, uint32_t slot, uint32_t type) {
    if (isInlineType(type) || type == DT_Bool64Key)
        return true;
    const size_t off = static_cast<size_t>(slot);
    if (off < saveDataOff || off >= bytes.size())
        return false;
    const size_t fixed = fixedHeapSize(type);
    if (fixed > 0)
        return fixed <= bytes.size() - off;
    if (bytes.size() - off < 4)
        return false;
    const uint64_t count = readU32(bytes.data() + off);
    const size_t avail = bytes.size() - off - 4;
    auto checkMul = [&](size_t elemSize) {
        if (count > MaxArrayCount)
            return false;
        return count * elemSize <= avail;
    };
    switch (type) {
        case DT_Bin: return count <= avail;
        case DT_BoolArray: {
            if (count > MaxArrayCount)
                return false;
            const size_t byteSize = std::max<size_t>(4, static_cast<size_t>((count + 7) / 8));
            const size_t aligned = (byteSize + 3) & ~size_t{3};
            return aligned <= avail;
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
                return false;
            size_t p = off + 4;
            for (uint64_t i = 0; i < count; ++i) {
                if (bytes.size() - p < 4)
                    return false;
                const uint32_t elemSize = readU32(bytes.data() + p);
                p += 4;
                if (elemSize > bytes.size() - p)
                    return false;
                p += elemSize;
            }
            return true;
        }
        default:
            return false;
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

constexpr uint32_t kHashPlayerName = 0xc7fb279bu;            // Hash("Player.Name")
constexpr uint32_t kHashHowToCallName = 0xbd8c4da1u;         // Hash("Player.HowToCallName")
constexpr uint32_t kHashNameLang = 0xb3229c6bu;              // Hash("Player.NameRegionLanguageID")
constexpr uint32_t kHashIslandName = 0xd46df986u;            // Hash("Player.IslandName")
constexpr uint32_t kHashHowToCallIslandName = 0xc6482c14u;   // Hash("Player.HowToCallIslandName")
constexpr uint32_t kHashIslandLang = 0x7bbff932u;            // Hash("Player.IslandNameRegionLanguageID")
constexpr uint32_t kHashMoney = 0x365fab1fu;                 // Hash("Player.Money")
constexpr uint32_t kHashCurrency = 0xdc88f139u;              // Hash("Player.Currency")
constexpr uint32_t kHashBootNum = 0x2f19f777u;               // Hash("Player.BootNum")
constexpr uint32_t kHashSkinTone = 0xafbdf7dcu;              // Hash("Player.SkinColorIndex")
constexpr uint32_t kHashBdayDay = 0xdb7786bbu;
constexpr uint32_t kHashBdayMonth = 0xc754bef3u;
constexpr uint32_t kHashBdayYear = 0x11996629u;
constexpr uint32_t kHashIslandSize = 0x870a807cu;
constexpr uint32_t kHashFountainLv = 0xa4e65f79u;            // Hash("Liberation.FountainLevel")
constexpr uint32_t kHashWishes = 0xa32f7e47u;
constexpr uint32_t kHashWishIds = 0xf91e67beu;               // Hash("Liberation.WishInfo.WishIdValue")
constexpr uint32_t kHashWishLiberated = 0xe38c7611u;         // Hash("Liberation.WishInfo.IsLiberated")
constexpr uint32_t kHashRegion = 0x10631cf8u;                // Hash("Player.Region")
constexpr uint32_t kHashTTPenalty = 0x71e3ba15u;
constexpr uint32_t kHashLastDonation = 0xbba9c090u;          // Hash("Player.LastDonationGameTime")

bool isTrackedPlayerHash(uint32_t h) {
    switch (h) {
        case kHashPlayerName:
        case kHashHowToCallName:
        case kHashNameLang:
        case kHashIslandName:
        case kHashHowToCallIslandName:
        case kHashIslandLang:
        case kHashMoney:
        case kHashCurrency:
        case kHashBootNum:
        case kHashSkinTone:
        case kHashBdayDay:
        case kHashBdayMonth:
        case kHashBdayYear:
        case kHashIslandSize:
        case kHashFountainLv:
        case kHashWishes:
        case kHashWishIds:
        case kHashWishLiberated:
        case kHashRegion:
        case kHashTTPenalty:
        case kHashLastDonation:
            return true;
        default:
            return false;
    }
}

struct CurrencyEntry {
    uint32_t hash;
    const char* symbol;
    const char* code;
    const char* labelKey;
    const char* fallback;
};

constexpr CurrencyEntry kCurrencyTable[] = {
    {0x7e3d1e46u, "---", "Invalid", "player_currency_invalid", "None (Invalid)"},
    {0xce8d18e1u, "\xc2\xa5", "Yen", "player_currency_yen", "Yen (JPY)"},
    {0x2bef288au, "$", "Dollar", "player_currency_dollar", "Dollar (USD)"},
    {0x038c20a9u, "\xe2\x82\xac", "Euro", "player_currency_euro", "Euro (EUR)"},
    {0x2f084465u, "\xc2\xa3", "Pound", "player_currency_pound", "Pound (GBP)"},
    {0x78915cbeu, "HK$", "AsiaDollar", "player_currency_asia_dollar", "Asian Dollar (HK$)"},
    {0xd73beb47u, "KRW", "Won", "player_currency_won", "Won (KRW)"},
    {0xaca685c9u, "\xc2\xa5", "Yuan", "player_currency_yuan", "Yuan (CNY)"},
    {0x4616c424u, "RUB", "Rouble", "player_currency_rouble", "Rouble (RUB)"},
    {0xa60f72e0u, "PHP", "Peso", "player_currency_peso", "Peso (PHP)"},
    {0x6362048bu, "G$", "GeneralUse", "player_currency_general", "General Currency (G$)"},
};

EnumResolution resolveCurrency(uint32_t hash, bool present) {
    EnumResolution res;
    res.rawHash = hash;
    res.present = present;
    if (!present)
        return res;
    for (const auto& entry : kCurrencyTable) {
        if (entry.hash == hash) {
            res.known = true;
            res.code = entry.code;
            res.labelKey = entry.labelKey;
            res.fallbackLabel = entry.fallback;
            res.symbol = entry.symbol;
            return res;
        }
    }
    res.fallbackLabel = FormatHex32(hash);
    return res;
}

struct NamedEnumEntry {
    const char* name;
    const char* labelKey;
    const char* fallback;
};

constexpr NamedEnumEntry kRegionTable[] = {
    {"Japan", "player_region_japan", "Japan"},
    {"Europe", "player_region_europe", "Europe"},
    {"NorthAmerica", "player_region_north_america", "North America"},
    {"SouthAmericaN", "player_region_south_america_n", "S. America (N)"},
    {"SouthAmericaS", "player_region_south_america_s", "S. America (S)"},
    {"Australia", "player_region_australia", "Australia/NZ"},
    {"Asia", "player_region_asia", "HK/TW/Korea"},
    {"OthersN", "player_region_others_n", "Other (N)"},
    {"OthersS", "player_region_others_s", "Other (S)"},
};

EnumResolution resolveRegion(uint32_t hash, bool present) {
    EnumResolution res;
    res.rawHash = hash;
    res.present = present;
    if (!present)
        return res;
    for (const auto& entry : kRegionTable) {
        if (Hash(entry.name) == hash) {
            res.known = true;
            res.code = entry.name;
            res.labelKey = entry.labelKey;
            res.fallbackLabel = entry.fallback;
            return res;
        }
    }
    res.fallbackLabel = FormatHex32(hash);
    return res;
}

constexpr NamedEnumEntry kLangTable[] = {
    {"JPja", "player_lang_jpja", "Japanese"},
    {"USen", "player_lang_usen", "English (US)"},
    {"EUen", "player_lang_euen", "English (EU)"},
    {"EUfr", "player_lang_eufr", "French (EU)"},
    {"EUde", "player_lang_eude", "German (EU)"},
    {"EUes", "player_lang_eues", "Spanish (EU)"},
    {"EUit", "player_lang_euit", "Italian (EU)"},
    {"EUnl", "player_lang_eunl", "Dutch (EU)"},
    {"CNzh", "player_lang_cnzh", "Chinese (Simplified)"},
    {"KRko", "player_lang_krko", "Korean"},
    {"TWzh", "player_lang_twzh", "Chinese (Traditional)"},
};

EnumResolution resolveLanguage(uint32_t hash, bool present) {
    EnumResolution res;
    res.rawHash = hash;
    res.present = present;
    if (!present)
        return res;
    for (const auto& entry : kLangTable) {
        if (Hash(entry.name) == hash) {
            res.known = true;
            res.code = entry.name;
            res.labelKey = entry.labelKey;
            res.fallbackLabel = entry.fallback;
            return res;
        }
    }
    res.fallbackLabel = FormatHex32(hash);
    return res;
}

std::string resolveKey(const Localizer& tr, const std::string& key) {
    return tr ? tr(key) : key;
}

std::string resolveEnumLabel(const EnumResolution& r, const Localizer& tr) {
    if (!r.present)
        return resolveKey(tr, "player_value_missing");
    if (r.known)
        return resolveKey(tr, r.labelKey);
    return resolveKey(tr, "player_value_unknown") + " (" + FormatHex32(r.rawHash) + ")";
}

} // namespace

uint32_t Hash(const char* name) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(name);
    const int len = static_cast<int>(std::strlen(name));
    const int nblocks = len / 4;
    uint32_t h1 = 0;
    const uint32_t c1 = 0xcc9e2d51u;
    const uint32_t c2 = 0x1b873593u;

    for (int i = 0; i < nblocks; ++i) {
        uint32_t k1 = 0;
        std::memcpy(&k1, data + i * 4, 4);
        k1 *= c1;
        k1 = rotl32(k1, 15);
        k1 *= c2;
        h1 ^= k1;
        h1 = rotl32(h1, 13);
        h1 = h1 * 5u + 0xe6546b64u;
    }

    const uint8_t* tail = data + nblocks * 4;
    uint32_t k1 = 0;
    switch (len & 3) {
        case 3:
            k1 ^= static_cast<uint32_t>(tail[2]) << 16;
            // fallthrough
        case 2:
            k1 ^= static_cast<uint32_t>(tail[1]) << 8;
            // fallthrough
        case 1:
            k1 ^= tail[0];
            k1 *= c1;
            k1 = rotl32(k1, 15);
            k1 *= c2;
            h1 ^= k1;
            break;
        default:
            break;
    }

    h1 ^= static_cast<uint32_t>(len);
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6bu;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35u;
    h1 ^= h1 >> 16;
    return h1;
}

std::string FormatMoneyCents(uint32_t cents) {
    const uint64_t whole = static_cast<uint64_t>(cents) / 100ull;
    const uint32_t frac = static_cast<uint32_t>(cents % 100u);
    const std::string s = std::to_string(whole);
    const size_t n = s.size();
    std::string out;
    if (n <= 3) {
        out = s;
    } else {
        out.reserve(n + n / 3 + 4);
        size_t firstGroup = n % 3;
        if (firstGroup == 0)
            firstGroup = 3;
        out.append(s, 0, firstGroup);
        for (size_t i = firstGroup; i < n; i += 3) {
            out.push_back(' ');
            out.append(s, i, 3);
        }
    }
    char fracBuf[8];
    std::snprintf(fracBuf, sizeof(fracBuf), ",%02u", frac);
    out += fracBuf;
    return out;
}

std::string FormatUtcTimestamp(uint64_t unixSeconds) {
    if (unixSeconds > 0x7fffffffffffffffULL)
        return std::to_string(unixSeconds);
    const time_t t = static_cast<time_t>(unixSeconds);
    struct tm tmv {};
    if (!gmtime_r(&t, &tmv))
        return std::to_string(unixSeconds);
    char buf[32];
    if (!std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M UTC", &tmv))
        return std::to_string(unixSeconds);
    return std::string(buf);
}

std::string FormatHex32(uint32_t value) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%08x", value);
    return std::string(buf);
}

const char* IslandSizeDimensions(int32_t preset) {
    switch (preset) {
        case 1: return "50 x 36";
        case 2: return "70 x 50";
        case 3: return "90 x 64";
        case 4: return "120 x 80";
        default: return nullptr;
    }
}

const char* SkinToneRgbHex(uint32_t index) {
    switch (index) {
        case 0: return "#F6D9BD";
        case 1: return "#ECC19D";
        case 2: return "#D2A07A";
        case 3: return "#B07A54";
        case 4: return "#825533";
        case 5: return "#5A391C";
        default: return nullptr;
    }
}

Result<PlayerProfileSummary> Parse(const std::vector<uint8_t>& bytes) {
    if (bytes.size() > TextureBrowser::MaxPlayerBytes)
        return {{}, {PlayerReadError::FileTooLarge, 0}};
    if (bytes.size() < 0x20u)
        return {{}, {PlayerReadError::InvalidHeader, 0}};
    if (readU32(bytes.data()) != 0x01020304u)
        return {{}, {PlayerReadError::InvalidHeader, 0}};

    const uint32_t version = readU32(bytes.data() + 4);
    const uint32_t saveDataOffRaw = readU32(bytes.data() + 8);
    if (saveDataOffRaw < 0x20u || static_cast<size_t>(saveDataOffRaw) > bytes.size())
        return {{}, {PlayerReadError::InvalidHeader, 0}};
    const size_t saveDataOff = static_cast<size_t>(saveDataOffRaw);
    if ((saveDataOff - 0x20u) % 8u != 0)
        return {{}, {PlayerReadError::CorruptIndex, 0}};

    struct RawEntry {
        uint32_t type = 0;
        uint32_t slot = 0;
    };

    PlayerProfileSummary summary;
    summary.saveVersion = version;

    std::set<uint32_t> seenTracked;
    bool haveType = false;
    uint32_t curType = 0;

    bool hasWishIds = false;
    RawEntry wishIdsEntry{};
    bool hasWishLib = false;
    RawEntry wishLibEntry{};
    uint32_t nameLangHash = 0;
    bool nameLangPresent = false;
    uint32_t islandLangHash = 0;
    bool islandLangPresent = false;
    uint32_t currencyHash = 0;
    bool currencyPresent = false;
    uint32_t regionHash = 0;
    bool regionPresent = false;

    for (size_t pos = 0x20u; pos < saveDataOff; pos += 8u) {
        const uint32_t hash = readU32(bytes.data() + pos);
        const uint32_t slot = readU32(bytes.data() + pos + 4);
        if (hash == 0) {
            if (slot >= DT_COUNT)
                return {{}, {PlayerReadError::CorruptIndex, 0}};
            curType = slot;
            haveType = true;
            continue;
        }
        if (!haveType)
            return {{}, {PlayerReadError::CorruptIndex, 0}};
        if (!validateHeapEntry(bytes, saveDataOff, slot, curType))
            return {{}, {PlayerReadError::TruncatedPayload, 0}};
        if (isTrackedPlayerHash(hash) && !seenTracked.insert(hash).second)
            return {{}, {PlayerReadError::CorruptIndex, 0}};

        const auto decodeStrField = [&](std::string& out, bool& cleanOut) -> bool {
            if (!isStringCompatible(curType))
                return false;
            const size_t maxUnits = (curType == DT_WStr64) ? 64u : 32u;
            cleanOut = decodeUtf16LeFixed(bytes.data() + slot, maxUnits, out);
            return true;
        };

        switch (hash) {
            case kHashPlayerName: {
                bool clean = true;
                if (!decodeStrField(summary.playerName, clean))
                    return {{}, {PlayerReadError::TypeMismatch, 0}};
                summary.playerNamePresent = true;
                summary.playerNameTextWarning = !clean;
                if (!clean)
                    summary.hasFieldWarnings = true;
                break;
            }
            case kHashHowToCallName: {
                bool clean = true;
                if (!decodeStrField(summary.howToCallName, clean)) {
                    summary.hasFieldWarnings = true;
                } else {
                    summary.howToCallNamePresent = true;
                    summary.howToCallNameTextWarning = !clean;
                    if (!clean)
                        summary.hasFieldWarnings = true;
                }
                break;
            }
            case kHashNameLang:
                if (curType != DT_Enum && !isScalarCompatible(curType)) {
                    summary.hasFieldWarnings = true;
                } else {
                    nameLangHash = slot;
                    nameLangPresent = true;
                }
                break;
            case kHashIslandName: {
                bool clean = true;
                if (!decodeStrField(summary.islandName, clean))
                    return {{}, {PlayerReadError::TypeMismatch, 0}};
                summary.islandNamePresent = true;
                summary.islandNameTextWarning = !clean;
                if (!clean)
                    summary.hasFieldWarnings = true;
                break;
            }
            case kHashHowToCallIslandName: {
                bool clean = true;
                if (!decodeStrField(summary.howToCallIslandName, clean)) {
                    summary.hasFieldWarnings = true;
                } else {
                    summary.howToCallIslandNamePresent = true;
                    summary.howToCallIslandNameTextWarning = !clean;
                    if (!clean)
                        summary.hasFieldWarnings = true;
                }
                break;
            }
            case kHashIslandLang:
                if (curType != DT_Enum && !isScalarCompatible(curType)) {
                    summary.hasFieldWarnings = true;
                } else {
                    islandLangHash = slot;
                    islandLangPresent = true;
                }
                break;
            case kHashMoney:
                if (curType != DT_UInt && !isScalarCompatible(curType))
                    return {{}, {PlayerReadError::TypeMismatch, 0}};
                summary.moneyCents = slot;
                summary.moneyPresent = true;
                break;
            case kHashCurrency:
                if (curType != DT_Enum && !isScalarCompatible(curType))
                    return {{}, {PlayerReadError::TypeMismatch, 0}};
                currencyHash = slot;
                currencyPresent = true;
                break;
            case kHashBootNum:
                if (!isScalarCompatible(curType))
                    return {{}, {PlayerReadError::TypeMismatch, 0}};
                summary.bootCount = slot;
                summary.bootCountPresent = true;
                break;
            case kHashSkinTone:
                if (!isScalarCompatible(curType))
                    return {{}, {PlayerReadError::TypeMismatch, 0}};
                summary.skinToneIndex = slot;
                summary.skinTonePresent = true;
                if (slot > 5u)
                    summary.hasFieldWarnings = true;
                break;
            case kHashBdayDay:
                if (!isScalarCompatible(curType))
                    return {{}, {PlayerReadError::TypeMismatch, 0}};
                summary.bdayDay = static_cast<int32_t>(slot);
                summary.bdayDayPresent = true;
                if (summary.bdayDay < 1 || summary.bdayDay > 31)
                    summary.hasFieldWarnings = true;
                break;
            case kHashBdayMonth:
                if (!isScalarCompatible(curType))
                    return {{}, {PlayerReadError::TypeMismatch, 0}};
                summary.bdayMonth = static_cast<int32_t>(slot);
                summary.bdayMonthPresent = true;
                if (summary.bdayMonth < 1 || summary.bdayMonth > 12)
                    summary.hasFieldWarnings = true;
                break;
            case kHashBdayYear:
                if (!isScalarCompatible(curType))
                    return {{}, {PlayerReadError::TypeMismatch, 0}};
                summary.bdayYear = static_cast<int32_t>(slot);
                summary.bdayYearPresent = true;
                summary.bdayYearSigned = (curType == DT_Int);
                break;
            case kHashIslandSize:
                if (!isScalarCompatible(curType))
                    return {{}, {PlayerReadError::TypeMismatch, 0}};
                summary.islandSizeRaw = static_cast<int32_t>(slot);
                summary.islandSizePresent = true;
                if (summary.islandSizeRaw < 1 || summary.islandSizeRaw > 4)
                    summary.hasFieldWarnings = true;
                break;
            case kHashFountainLv:
                if (!isScalarCompatible(curType))
                    return {{}, {PlayerReadError::TypeMismatch, 0}};
                summary.fountainLevel = slot;
                summary.fountainLevelPresent = true;
                break;
            case kHashWishes:
                if (!isScalarCompatible(curType))
                    return {{}, {PlayerReadError::TypeMismatch, 0}};
                summary.wishes.countPresent = true;
                summary.wishes.wishesMade = slot;
                break;
            case kHashWishIds:
                hasWishIds = true;
                wishIdsEntry = {curType, slot};
                break;
            case kHashWishLiberated:
                hasWishLib = true;
                wishLibEntry = {curType, slot};
                break;
            case kHashRegion:
                if (!isScalarCompatible(curType))
                    return {{}, {PlayerReadError::TypeMismatch, 0}};
                regionHash = slot;
                regionPresent = true;
                break;
            case kHashTTPenalty:
                if (curType != DT_UInt64)
                    return {{}, {PlayerReadError::TypeMismatch, 0}};
                summary.ttPenaltyRaw = readU64(bytes.data() + slot);
                summary.ttPenaltyPresent = true;
                break;
            case kHashLastDonation:
                if (curType != DT_UInt64)
                    return {{}, {PlayerReadError::TypeMismatch, 0}};
                summary.lastDonationSeconds = readU64(bytes.data() + slot);
                summary.lastDonationPresent = true;
                break;
            default:
                break;
        }
    }

    if (!summary.playerNamePresent || !summary.islandNamePresent ||
        !summary.moneyPresent || !currencyPresent)
        return {{}, {PlayerReadError::MissingRequiredField, 0}};

    summary.nameLanguage = resolveLanguage(nameLangHash, nameLangPresent);
    summary.islandLanguage = resolveLanguage(islandLangHash, islandLangPresent);
    summary.currency = resolveCurrency(currencyHash, currencyPresent);
    summary.region = resolveRegion(regionHash, regionPresent);

    if ((nameLangPresent && !summary.nameLanguage.known) ||
        (islandLangPresent && !summary.islandLanguage.known) ||
        (currencyPresent && !summary.currency.known) ||
        (regionPresent && !summary.region.known)) {
        summary.hasFieldWarnings = true;
    }

    // Wishes liberation progress: degrades locally if absent or inconsistent without failing valid profile.
    summary.wishes.totalCatalogWishes = WishesData::WISH_COUNT;
    if (hasWishIds && hasWishLib &&
        wishIdsEntry.type == DT_UIntArray && wishLibEntry.type == DT_BoolArray) {
        const uint32_t idCount = readU32(bytes.data() + wishIdsEntry.slot);
        const uint32_t libCount = readU32(bytes.data() + wishLibEntry.slot);
        if (libCount > 0 && idCount > 0) {
            int count = 0;
            for (uint32_t i = 0; i < libCount; ++i) {
                const uint8_t byteVal = bytes[wishLibEntry.slot + 4 + (i >> 3)];
                if (((byteVal >> (i & 7)) & 1u) != 0u)
                    ++count;
            }
            summary.wishes.liberatedAvailable = true;
            summary.wishes.liberatedCount = count;
            if (libCount != idCount) {
                summary.wishes.tableWarning = true;
                summary.hasFieldWarnings = true;
            }
        } else {
            summary.wishes.tableWarning = true;
            summary.hasFieldWarnings = true;
        }
    } else if (hasWishIds || hasWishLib || summary.wishes.countPresent) {
        summary.wishes.tableWarning = true;
        summary.hasFieldWarnings = true;
    }

    return {std::move(summary), {PlayerReadError::None, 0}};
}

Result<PlayerProfileSummary> LoadFromReadSource(
    TextureBrowser::ReadSource& source,
    TextureBrowser::AccountKey account,
    const TextureBrowser::Cancel& cancel) {
    ScopeGuard<std::function<void()>> ensureClose{[&] { source.close(); }};
    if (cancel && cancel())
        return {{}, {PlayerReadError::Cancelled, 0}};
    if (!account.valid())
        return {{}, {PlayerReadError::NoAccountSelected, 0}};

    const auto openStatus = source.open(account, cancel);
    if (!openStatus) {
        PlayerReadError err = PlayerReadError::ReadOnlyOpenFailed;
        switch (openStatus.error) {
            case TextureBrowser::Error::NoSave: err = PlayerReadError::NoSave; break;
            case TextureBrowser::Error::Metadata: err = PlayerReadError::AccountMetadataFailed; break;
            case TextureBrowser::Error::Cancelled: err = PlayerReadError::Cancelled; break;
            case TextureBrowser::Error::InvalidData: err = PlayerReadError::NoAccountSelected; break;
            default: err = PlayerReadError::ReadOnlyOpenFailed; break;
        }
        return {{}, {err, openStatus.platform}};
    }

    auto readResult = source.read("/Player.sav", TextureBrowser::MaxPlayerBytes, cancel);
    if (!readResult) {
        PlayerReadError err = PlayerReadError::ShortRead;
        switch (readResult.status.error) {
            case TextureBrowser::Error::Limit: err = PlayerReadError::FileTooLarge; break;
            case TextureBrowser::Error::Cancelled: err = PlayerReadError::Cancelled; break;
            case TextureBrowser::Error::InvalidData: err = PlayerReadError::InvalidHeader; break;
            default: err = PlayerReadError::ShortRead; break;
        }
        return {{}, {err, readResult.status.platform}};
    }

    if (cancel && cancel())
        return {{}, {PlayerReadError::Cancelled, 0}};
    return Parse(readResult.value);
}

std::array<PlayerAttributeView, AttributeCount> FormatAttributes(
    const PlayerProfileSummary& summary,
    const Localizer& tr,
    int64_t referenceUnixSeconds) {
    const int64_t refNow = (referenceUnixSeconds > 0)
                               ? referenceUnixSeconds
                               : static_cast<int64_t>(std::time(nullptr));

    auto T = [&](const std::string& key) { return resolveKey(tr, key); };
    auto makeFocusKey = [](size_t idx) { return "attr_" + std::to_string(idx); };

    std::array<PlayerAttributeView, AttributeCount> rows{};

    // 0: Name
    {
        auto& r = rows[0];
        r.id = PlayerAttributeId::Name;
        r.focusKey = makeFocusKey(0);
        r.title = T("player_field_name");
        r.primaryValue = summary.playerName.empty() ? T("empty") : summary.playerName;
        const std::string pron = summary.howToCallName.empty()
                                     ? (summary.playerName.empty() ? T("empty") : summary.playerName + " (" + T("empty") + ")")
                                     : summary.howToCallName;
        r.secondaryMetadata = T("player_meta_pronunciation") + ": " + pron +
                              "\n" + T("player_meta_language") + ": " + resolveEnumLabel(summary.nameLanguage, tr);
        r.description = T("player_desc_name");
        r.hasWarning = summary.playerNameTextWarning || summary.howToCallNameTextWarning ||
                       !summary.nameLanguage.present || !summary.nameLanguage.known;
    }

    // 1: Island
    {
        auto& r = rows[1];
        r.id = PlayerAttributeId::Island;
        r.focusKey = makeFocusKey(1);
        r.title = T("player_field_island");
        r.primaryValue = summary.islandName.empty() ? T("empty") : summary.islandName;
        const std::string pron = summary.howToCallIslandName.empty()
                                     ? (summary.islandName.empty() ? T("empty") : summary.islandName + " (" + T("empty") + ")")
                                     : summary.howToCallIslandName;
        r.secondaryMetadata = T("player_meta_pronunciation") + ": " + pron +
                              "\n" + T("player_meta_language") + ": " + resolveEnumLabel(summary.islandLanguage, tr);
        r.description = T("player_desc_island");
        r.hasWarning = summary.islandNameTextWarning || summary.howToCallIslandNameTextWarning ||
                       !summary.islandLanguage.present || !summary.islandLanguage.known;
    }

    // 2: Money
    {
        auto& r = rows[2];
        r.id = PlayerAttributeId::Money;
        r.focusKey = makeFocusKey(2);
        r.title = T("player_field_money");
        if (!summary.moneyPresent) {
            r.primaryValue = T("player_value_missing");
            r.hasWarning = true;
        } else {
            r.primaryValue = FormatMoneyCents(summary.moneyCents);
            if (summary.currency.present && summary.currency.known &&
                summary.currency.symbol != "---") {
                r.primaryValue += " " + summary.currency.symbol;
            }
        }
        r.secondaryMetadata = T("player_meta_raw_value") + ": " + std::to_string(summary.moneyCents);
        r.description = T("player_desc_money");
    }

    // 3: Currency
    {
        auto& r = rows[3];
        r.id = PlayerAttributeId::Currency;
        r.focusKey = makeFocusKey(3);
        r.title = T("player_field_currency");
        if (!summary.currency.present) {
            r.primaryValue = T("player_value_missing");
            r.hasWarning = true;
        } else if (summary.currency.known) {
            r.primaryValue = summary.currency.symbol + " · " + T(summary.currency.labelKey);
        } else {
            r.primaryValue = T("player_value_unknown") + " (" + FormatHex32(summary.currency.rawHash) + ")";
            r.hasWarning = true;
        }
        r.secondaryMetadata = T("player_meta_currency") + ": " +
                              (summary.currency.known ? (summary.currency.code + " · " + FormatHex32(summary.currency.rawHash))
                                                      : FormatHex32(summary.currency.rawHash));
        r.description = T("player_desc_currency");
    }

    // 4: Boot Count
    {
        auto& r = rows[4];
        r.id = PlayerAttributeId::BootCount;
        r.focusKey = makeFocusKey(4);
        r.title = T("player_field_boot_count");
        r.primaryValue = summary.bootCountPresent ? std::to_string(summary.bootCount) : T("player_value_missing");
        r.secondaryMetadata = T("player_meta_raw_value") + ": " + std::to_string(summary.bootCount);
        r.description = T("player_desc_boot_count");
        r.hasWarning = !summary.bootCountPresent;
    }

    // 5: Skin Tone
    {
        auto& r = rows[5];
        r.id = PlayerAttributeId::SkinTone;
        r.focusKey = makeFocusKey(5);
        r.title = T("player_field_skin_tone");
        if (!summary.skinTonePresent) {
            r.primaryValue = T("player_value_missing");
            r.secondaryMetadata = T("player_value_missing");
            r.hasWarning = true;
        } else if (summary.skinToneValid()) {
            r.primaryValue = std::to_string(summary.skinToneIndex + 1u) + " / 6";
            r.secondaryMetadata = T("player_meta_skin_index") + ": " + std::to_string(summary.skinToneIndex) +
                                  " (" + std::string(SkinToneRgbHex(summary.skinToneIndex)) + ")";
        } else {
            r.primaryValue = std::to_string(summary.skinToneIndex) + " (" + T("player_value_out_of_range") + ")";
            r.secondaryMetadata = T("player_meta_skin_index") + ": " + std::to_string(summary.skinToneIndex) +
                                  " · " + T("player_value_out_of_range") + " (0–5)";
            r.hasWarning = true;
        }
        r.description = T("player_desc_skin_tone");
    }

    // 6: Bday Day
    {
        auto& r = rows[6];
        r.id = PlayerAttributeId::BdayDay;
        r.focusKey = makeFocusKey(6);
        r.title = T("player_field_bday_day");
        if (!summary.bdayDayPresent) {
            r.primaryValue = T("player_value_missing");
            r.secondaryMetadata = T("player_value_missing");
            r.hasWarning = true;
        } else if (summary.bdayDayValid()) {
            r.primaryValue = std::to_string(summary.bdayDay);
            r.secondaryMetadata = T("player_meta_range_valid") + ": 1–31";
        } else {
            r.primaryValue = std::to_string(summary.bdayDay) + " (" + T("player_value_out_of_range") + ")";
            r.secondaryMetadata = T("player_value_out_of_range") + " (1–31)";
            r.hasWarning = true;
        }
        r.description = T("player_desc_bday_day");
    }

    // 7: Bday Month
    {
        auto& r = rows[7];
        r.id = PlayerAttributeId::BdayMonth;
        r.focusKey = makeFocusKey(7);
        r.title = T("player_field_bday_month");
        if (!summary.bdayMonthPresent) {
            r.primaryValue = T("player_value_missing");
            r.secondaryMetadata = T("player_value_missing");
            r.hasWarning = true;
        } else if (summary.bdayMonthValid()) {
            r.primaryValue = std::to_string(summary.bdayMonth);
            r.secondaryMetadata = T("player_meta_range_valid") + ": 1–12";
        } else {
            r.primaryValue = std::to_string(summary.bdayMonth) + " (" + T("player_value_out_of_range") + ")";
            r.secondaryMetadata = T("player_value_out_of_range") + " (1–12)";
            r.hasWarning = true;
        }
        r.description = T("player_desc_bday_month");
    }

    // 8: Bday Year
    {
        auto& r = rows[8];
        r.id = PlayerAttributeId::BdayYear;
        r.focusKey = makeFocusKey(8);
        r.title = T("player_field_bday_year");
        if (!summary.bdayYearPresent) {
            r.primaryValue = T("player_value_missing");
            r.secondaryMetadata = T("player_value_missing");
            r.hasWarning = true;
        } else {
            const std::string yr = summary.bdayYearSigned
                                       ? std::to_string(summary.bdayYear)
                                       : std::to_string(static_cast<uint32_t>(summary.bdayYear));
            r.primaryValue = yr;
            r.secondaryMetadata = T("player_meta_raw_value") + ": " + yr;
        }
        r.description = T("player_desc_bday_year");
    }

    // 9: Island Size
    {
        auto& r = rows[9];
        r.id = PlayerAttributeId::IslandSize;
        r.focusKey = makeFocusKey(9);
        r.title = T("player_field_island_size");
        const char* dims = summary.islandSizePresent ? IslandSizeDimensions(summary.islandSizeRaw) : nullptr;
        if (!summary.islandSizePresent) {
            r.primaryValue = T("player_value_missing");
            r.secondaryMetadata = T("player_value_missing");
            r.hasWarning = true;
        } else if (dims) {
            r.primaryValue = dims;
            r.secondaryMetadata = T("player_meta_preset") + ": " + std::to_string(summary.islandSizeRaw) +
                                  " / 4 (" + dims + ")";
        } else {
            r.primaryValue = std::to_string(summary.islandSizeRaw) + " (" + T("player_value_non_standard") + ")";
            r.secondaryMetadata = T("player_value_non_standard") + " · " + T("player_meta_raw_value") +
                                  ": " + std::to_string(summary.islandSizeRaw);
            r.hasWarning = true;
        }
        r.description = T("player_desc_island_size");
    }

    // 10: Fountain Lv
    {
        auto& r = rows[10];
        r.id = PlayerAttributeId::FountainLv;
        r.focusKey = makeFocusKey(10);
        r.title = T("player_field_fountain_lv");
        r.primaryValue = summary.fountainLevelPresent
                             ? std::to_string(summary.fountainLevel)
                             : T("player_value_missing");
        r.secondaryMetadata = T("player_meta_raw_value") + ": " + std::to_string(summary.fountainLevel);
        r.description = T("player_desc_fountain_lv");
        r.hasWarning = !summary.fountainLevelPresent;
    }

    // 11: Wishes
    {
        auto& r = rows[11];
        r.id = PlayerAttributeId::Wishes;
        r.focusKey = makeFocusKey(11);
        r.title = T("player_field_wishes");
        r.primaryValue = summary.wishes.countPresent
                             ? std::to_string(summary.wishes.wishesMade)
                             : T("player_value_missing");
        if (summary.wishes.liberatedAvailable) {
            r.secondaryMetadata = T("player_meta_wishes_liberated") + ": " +
                                  std::to_string(summary.wishes.liberatedCount) + " / " +
                                  std::to_string(summary.wishes.totalCatalogWishes);
        } else {
            r.secondaryMetadata = T("player_meta_wishes_unavailable");
        }
        r.description = T("player_desc_wishes");
        r.hasWarning = !summary.wishes.countPresent || !summary.wishes.liberatedAvailable || summary.wishes.tableWarning;
    }

    // 12: Region
    {
        auto& r = rows[12];
        r.id = PlayerAttributeId::Region;
        r.focusKey = makeFocusKey(12);
        r.title = T("player_field_region");
        r.primaryValue = resolveEnumLabel(summary.region, tr);
        r.secondaryMetadata = T("player_meta_region_code") + ": " +
                              (summary.region.known ? summary.region.code : FormatHex32(summary.region.rawHash));
        r.description = T("player_desc_region");
        r.hasWarning = !summary.region.present || !summary.region.known;
    }

    // 13: TT Penalty
    {
        auto& r = rows[13];
        r.id = PlayerAttributeId::TTPenalty;
        r.focusKey = makeFocusKey(13);
        r.title = T("player_field_tt_penalty");
        if (!summary.ttPenaltyPresent) {
            r.primaryValue = T("player_value_missing");
            r.secondaryMetadata = T("player_value_missing");
            r.hasWarning = true;
        } else if (summary.ttPenaltyRaw != 0) {
            r.primaryValue = T("player_penalty_active");
            r.secondaryMetadata = T("player_penalty_detail_active") + " · " +
                                  T("player_meta_timestamp") + ": " + std::to_string(summary.ttPenaltyRaw);
        } else {
            r.primaryValue = T("player_penalty_cleared");
            r.secondaryMetadata = T("player_penalty_detail_cleared") + " · " +
                                  T("player_meta_timestamp") + ": 0";
        }
        r.description = T("player_desc_tt_penalty");
    }

    // 14: Donation
    {
        auto& r = rows[14];
        r.id = PlayerAttributeId::Donation;
        r.focusKey = makeFocusKey(14);
        r.title = T("player_field_donation");
        if (!summary.lastDonationPresent) {
            r.primaryValue = T("player_value_missing");
            r.secondaryMetadata = T("player_value_missing");
            r.hasWarning = true;
        } else if (summary.lastDonationSeconds == 0) {
            r.primaryValue = T("player_donation_never");
            r.secondaryMetadata = T("player_donation_never_hint");
        } else {
            r.primaryValue = FormatUtcTimestamp(summary.lastDonationSeconds);
            constexpr int64_t kSecsPerDay = 24LL * 60LL * 60LL;
            const int64_t stamp = static_cast<int64_t>(summary.lastDonationSeconds);
            if (summary.lastDonationSeconds > static_cast<uint64_t>(std::max<int64_t>(0, refNow)) ||
                stamp > refNow) {
                r.secondaryMetadata = T("player_donation_future");
                r.hasWarning = true;
            } else {
                const int64_t days = (refNow - stamp) / kSecsPerDay;
                if (days == 0) {
                    r.secondaryMetadata = T("player_donation_today");
                } else if (days == 1) {
                    r.secondaryMetadata = "1 " + T("player_donation_day_ago");
                } else {
                    r.secondaryMetadata = std::to_string(days) + " " + T("player_donation_days_ago");
                }
            }
        }
        r.description = T("player_desc_donation");
    }

    return rows;
}

void PlayerBrowserModel::select(TextureBrowser::Account account) {
    context = {};
    context.account = std::move(account);
    hasAccount = context.account.key.valid();
    summary = {};
    if (hasAccount) {
        loading();
    } else {
        ++generation;
        state = PlayerBrowserState::Unselected;
        status = {PlayerReadError::NoAccountSelected, 0};
    }
}

void PlayerBrowserModel::loading() {
    ++generation;
    state = PlayerBrowserState::Loading;
    status = {};
}

void PlayerBrowserModel::accept(Result<PlayerProfileSummary> result) {
    status = result.status;
    if (!result) {
        summary = {};
        state = PlayerBrowserState::Error;
        return;
    }
    summary = std::move(result.value);
    state = PlayerBrowserState::Ready;
}

void PlayerBrowserModel::suspend() {
    if (state == PlayerBrowserState::Loading) {
        ++generation;
        state = PlayerBrowserState::Suspended;
    }
}

bool PlayerBrowserModel::resume() {
    if (state != PlayerBrowserState::Suspended)
        return false;
    loading();
    return true;
}

} // namespace PlayerProfile
