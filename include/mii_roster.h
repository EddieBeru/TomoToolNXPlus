#pragma once
// Read-only Mii.sav (+ optional Map.sav) domain model, bounded binary parser,
// formatters for all 7 resident sections, and session state machine.
// Binary format, Murmur3 hashes, and catalogs match TomoToolNX save_editor.cpp
// and alexislours/ltd-save-editor. Strictly read-only; no write/commit APIs.
#include "texture_browser.h"
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace MiiRoster {

constexpr size_t MaxSlots = 70;
constexpr size_t WordsPerMii = 12;
constexpr size_t PocketSlotsPerMii = 12;
constexpr size_t WornClothSlots = 8;
constexpr size_t HabitCategoryCount = 9;
constexpr size_t MaxArrayCount = 262144;

enum class MiiSectionId : size_t {
    Stats = 0,
    Words,
    Relations,
    Belongings,
    Habits,
    Housing,
    Social,
};
constexpr size_t SectionCount = 7;

enum class RelationGroup : uint8_t {
    Friend = 0,
    Romance,
    Family,
    Know,
    Former,
    Other,
};

enum class HabitState : uint8_t {
    Owned = 0,
    Unowned,
    NeverOwned,
    Unknown,
};

struct MiiStats {
    std::string name;
    bool nameTextWarning = false;

    int32_t levelRaw = 0;
    bool levelPresent = false;
    int32_t levelMeter = 0;
    bool levelMeterPresent = false;
    uint32_t money = 0;
    bool moneyPresent = false;

    int32_t bdayDay = 0;
    bool bdayDayPresent = false;
    int32_t bdayMonth = 0;
    bool bdayMonthPresent = false;
    int32_t bdayYear = 0;
    bool bdayYearPresent = false;
    int32_t directAge = -1;
    bool directAgePresent = false;

    int32_t activeness = 0;
    bool activenessPresent = false;
    int32_t audacity = 0;
    bool audacityPresent = false;
    int32_t commonsense = 0;
    bool commonsensePresent = false;
    int32_t gaiety = 0;
    bool gaietyPresent = false;
    int32_t sociability = 0;
    bool sociabilityPresent = false;

    int32_t bondMeter = 0;
    bool bondMeterPresent = false;

    uint32_t moodHash = 0;
    bool moodPresent = false;
    bool moodKnown = false;
    std::string moodKey;

    int32_t fullness = 0;
    bool fullnessPresent = false;

    bool bdayDayValid() const { return bdayDayPresent && bdayDay >= 1 && bdayDay <= 31; }
    bool bdayMonthValid() const { return bdayMonthPresent && bdayMonth >= 1 && bdayMonth <= 12; }
    int32_t displayLevel() const { return levelRaw + 1; }
};

struct MiiWordSlot {
    int slotNumber = 0; // 1..12
    uint32_t kindHash = 0;
    bool isEmpty = true;
    bool kindKnown = false;
    std::string kindKey;
    std::string text;
    std::string howToCall;
    bool textWarning = false;
};

struct MiiRelationEntry {
    int pairIndex = -1;
    int otherSlotIndex = -1;
    int otherSlotNumber = 0; // 1..70
    std::string otherName;
    bool otherSlotOccupied = false;

    uint32_t myTypeHash = 0;
    uint32_t otherTypeHash = 0;
    bool myTypeKnown = false;
    std::string myTypeKey;
    RelationGroup categoryGroup = RelationGroup::Other;

    int32_t meter = 0;
    bool meterFixed = false;
    std::string tierLabel;
    bool isFight = false;
    uint64_t sinceUnixSeconds = 0;
};

struct MiiWornClothSlot {
    std::string slotKey;
    uint32_t itemHash = 0;
    int32_t colorIndex = 0;
    int colorCount = 0;
    bool isEmpty = true;
    bool known = false;
    std::string label;
};

struct MiiPocketSlot {
    int slotNumber = 0; // 1..12
    uint32_t itemHash = 0;
    bool isEmpty = true;
    bool known = false;
    std::string label;
};

struct MiiBelongings {
    std::array<MiiWornClothSlot, WornClothSlots> wornSlots{};
    MiiWornClothSlot coordinate;
    std::array<MiiPocketSlot, PocketSlotsPerMii> pocket{};
};

struct MiiHabitEntry {
    int habitIndex = 0;
    int category = 0;
    const char* label = "";
    bool isOwn = false;
    bool isChecked = false;
    uint32_t stateHash = 0;
    HabitState state = HabitState::NeverOwned;
};

struct MiiHabitCategorySummary {
    int category = 0;
    std::string categoryKey;
    int ownedCount = 0;
    int activeCount = 0;
    int totalCount = 0;
    std::vector<MiiHabitEntry> entries;
};

struct MiiHousemateRef {
    int slotNumber = 0; // 1..70
    std::string name;
    int32_t roomIndex = 0;
    bool sameRoomConflict = false;
};

struct MiiHousingInfo {
    bool housed = false;
    int32_t houseMapId = -1;
    int32_t roomIndex = 0;
    std::string houseName;
    bool houseNameFromMap = false;
    std::vector<MiiHousemateRef> housemates;
    bool hasRoomConflict = false;
};

struct MiiSocialSummary {
    size_t totalConnections = 0;
    size_t friends = 0;
    size_t romance = 0;
    size_t family = 0;
    size_t know = 0;
    size_t former = 0;
    size_t other = 0;
    size_t activeFights = 0;
    std::vector<MiiRelationEntry> groupedRelations;
};

struct MiiResident {
    int slotIndex = 0;  // 0..69
    int slotNumber = 1; // 1..70
    MiiStats stats;
    std::array<MiiWordSlot, WordsPerMii> words{};
    std::vector<MiiRelationEntry> relations;
    MiiBelongings belongings;
    std::array<MiiHabitCategorySummary, HabitCategoryCount> habitCategories{};
    MiiHousingInfo housing;
    MiiSocialSummary social;
    bool hasWarnings = false;
};

struct MiiRosterSummary {
    uint32_t saveVersion = 0;
    std::vector<MiiResident> residents; // 0..70 ascending by slotNumber
    bool mapNamesAvailable = false;
    bool mapNamesWarning = false;
    bool hasFieldWarnings = false;
};

struct MiiSectionOverview {
    MiiSectionId id = MiiSectionId::Stats;
    std::string focusKey;
    std::string title;
    std::string summaryText;
};

struct MiiRowView {
    std::string title;
    std::string primaryValue;
    std::string secondaryText;
    bool hasWarning = false;
};

enum class MiiReadError {
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
    MiiReadError error = MiiReadError::None;
    uint32_t platform = 0;
    explicit operator bool() const { return error == MiiReadError::None; }
};

template <class T>
struct Result {
    T value{};
    Status status{};
    explicit operator bool() const { return bool(status); }
};

using Localizer = std::function<std::string(const std::string&)>;

uint32_t Hash(const char* name);
std::string FormatHex32(uint32_t value);
std::string FormatUtcTimestamp(uint64_t unixSeconds);

Result<MiiRosterSummary> Parse(
    const std::vector<uint8_t>& miiBytes,
    const std::vector<uint8_t>* mapBytes = nullptr);

Result<MiiRosterSummary> LoadFromReadSource(
    TextureBrowser::ReadSource& source,
    TextureBrowser::AccountKey account,
    const TextureBrowser::Cancel& cancel);

std::string FormatResidentSummary(const MiiResident& resident, const Localizer& tr);
std::array<MiiSectionOverview, SectionCount> FormatSectionOverviews(
    const MiiResident& resident,
    const Localizer& tr);
std::vector<MiiRowView> FormatSectionRows(
    const MiiResident& resident,
    MiiSectionId section,
    const Localizer& tr);

enum class MiiBrowserState {
    Unselected,
    Loading,
    Suspended,
    Empty,
    Ready,
    Error,
};

struct MiiBrowserContext {
    TextureBrowser::Account account;
    int selectedSlotNumber = -1;
    std::string focusTarget = "refresh";
    float scroll = 0.0f;
};

struct MiiBrowserModel {
    MiiBrowserState state = MiiBrowserState::Unselected;
    MiiBrowserContext context;
    MiiRosterSummary summary;
    Status status;
    uint64_t generation = 0;
    bool hasAccount = false;

    void select(TextureBrowser::Account account);
    void loading();
    void accept(Result<MiiRosterSummary> result);
    void suspend();
    bool resume();
};

} // namespace MiiRoster
