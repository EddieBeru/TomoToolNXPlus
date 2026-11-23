#pragma once
// Read-only Player.sav domain model, bounded binary parser, and session controller.
// Binary format and Murmur3 field hashes match TomoToolNX save_editor.cpp (alexislours/ltd-save-editor).
#include "texture_browser.h"
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace PlayerProfile {

enum class PlayerAttributeId : size_t {
    Name = 0,
    Island,
    Money,
    Currency,
    BootCount,
    SkinTone,
    BdayDay,
    BdayMonth,
    BdayYear,
    IslandSize,
    FountainLv,
    Wishes,
    Region,
    TTPenalty,
    Donation,
};

constexpr size_t AttributeCount = 15;
constexpr size_t MaxArrayCount = 65536;

struct EnumResolution {
    uint32_t rawHash = 0;
    bool present = false;
    bool known = false;
    std::string code;
    std::string labelKey;
    std::string fallbackLabel;
    std::string symbol;
};

struct WishesProgress {
    bool countPresent = false;
    uint32_t wishesMade = 0;
    bool liberatedAvailable = false;
    int liberatedCount = 0;
    int totalCatalogWishes = 268;
    bool tableWarning = false;
};

struct PlayerProfileSummary {
    uint32_t saveVersion = 0;

    std::string playerName;
    bool playerNamePresent = false;
    bool playerNameTextWarning = false;
    std::string howToCallName;
    bool howToCallNamePresent = false;
    bool howToCallNameTextWarning = false;
    EnumResolution nameLanguage;

    std::string islandName;
    bool islandNamePresent = false;
    bool islandNameTextWarning = false;
    std::string howToCallIslandName;
    bool howToCallIslandNamePresent = false;
    bool howToCallIslandNameTextWarning = false;
    EnumResolution islandLanguage;

    uint32_t moneyCents = 0;
    bool moneyPresent = false;
    EnumResolution currency;
    uint32_t bootCount = 0;
    bool bootCountPresent = false;
    uint32_t skinToneIndex = 0;
    bool skinTonePresent = false;

    int32_t bdayDay = 0;
    bool bdayDayPresent = false;
    int32_t bdayMonth = 0;
    bool bdayMonthPresent = false;
    int32_t bdayYear = 0;
    bool bdayYearPresent = false;
    bool bdayYearSigned = false;

    int32_t islandSizeRaw = 0;
    bool islandSizePresent = false;
    uint32_t fountainLevel = 0;
    bool fountainLevelPresent = false;
    WishesProgress wishes;
    EnumResolution region;

    uint64_t ttPenaltyRaw = 0;
    bool ttPenaltyPresent = false;
    uint64_t lastDonationSeconds = 0;
    bool lastDonationPresent = false;

    bool hasFieldWarnings = false;

    bool skinToneValid() const { return skinTonePresent && skinToneIndex <= 5u; }
    bool bdayDayValid() const { return bdayDayPresent && bdayDay >= 1 && bdayDay <= 31; }
    bool bdayMonthValid() const { return bdayMonthPresent && bdayMonth >= 1 && bdayMonth <= 12; }
    bool islandSizeValid() const { return islandSizePresent && islandSizeRaw >= 1 && islandSizeRaw <= 4; }
};

struct PlayerAttributeView {
    PlayerAttributeId id = PlayerAttributeId::Name;
    std::string focusKey;
    std::string title;
    std::string primaryValue;
    std::string secondaryMetadata;
    std::string description;
    bool hasWarning = false;
};

enum class PlayerReadError {
    None = 0,
    NoAccountSelected,
    NoSave,
    AccountMetadataFailed,
    ReadOnlyOpenFailed,
    ShortRead,
    FileTooLarge,
    InvalidHeader,
    CorruptIndex,
    TruncatedPayload,
    MissingRequiredField,
    TypeMismatch,
    Cancelled,
};

struct Status {
    PlayerReadError error = PlayerReadError::None;
    uint32_t platform = 0;
    explicit operator bool() const { return error == PlayerReadError::None; }
};

template <class T>
struct Result {
    T value{};
    Status status{};
    explicit operator bool() const { return bool(status); }
};

using Localizer = std::function<std::string(const std::string&)>;

uint32_t Hash(const char* name);
std::string FormatMoneyCents(uint32_t cents);
std::string FormatUtcTimestamp(uint64_t unixSeconds);
std::string FormatHex32(uint32_t value);
const char* IslandSizeDimensions(int32_t preset);
const char* SkinToneRgbHex(uint32_t index);

Result<PlayerProfileSummary> Parse(const std::vector<uint8_t>& bytes);
Result<PlayerProfileSummary> LoadFromReadSource(
    TextureBrowser::ReadSource& source,
    TextureBrowser::AccountKey account,
    const TextureBrowser::Cancel& cancel);

std::array<PlayerAttributeView, AttributeCount> FormatAttributes(
    const PlayerProfileSummary& summary,
    const Localizer& tr,
    int64_t referenceUnixSeconds = 0);

enum class PlayerBrowserState {
    Unselected,
    Loading,
    Suspended,
    Ready,
    Error,
};

struct PlayerBrowserContext {
    TextureBrowser::Account account;
    std::string focusTarget = "refresh";
    int selectedAttrIndex = -1;
    float scroll = 0.0f;
};

struct PlayerBrowserModel {
    PlayerBrowserState state = PlayerBrowserState::Unselected;
    PlayerBrowserContext context;
    PlayerProfileSummary summary;
    Status status;
    uint64_t generation = 0;
    bool hasAccount = false;

    void select(TextureBrowser::Account account);
    void loading();
    void accept(Result<PlayerProfileSummary> result);
    void suspend();
    bool resume();
};

} // namespace PlayerProfile
