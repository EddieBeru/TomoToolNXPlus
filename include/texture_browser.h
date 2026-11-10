#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace TextureBrowser {
constexpr size_t MaxRecords=8192, MaxEntries=2048, MaxPlayerBytes=8*1024*1024;
constexpr size_t MaxImageBytes=4*1024*1024, ReadChunk=64*1024;
struct AccountKey {
    std::array<uint64_t,2> words{};
    bool valid() const { return words[0] || words[1]; }
    bool operator==(const AccountKey& rhs) const { return words==rhs.words; }
    bool operator!=(const AccountKey& rhs) const { return !(*this==rhs); }
};
struct Account { AccountKey key; std::string name; size_t ordinal=0; bool profileWarning=false; };
enum class Error { None, AccountService, NoAccounts, NoSave, Metadata, Open,
    UgcMissing, Enumeration, ShortRead, InvalidData, Unsupported, Limit, Preview, Cancelled };
struct Status {
    Error error=Error::None;
    uint32_t platform=0;
    explicit operator bool() const { return error==Error::None; }
};
template<class T> struct Result { T value{}; Status status{}; explicit operator bool() const {return bool(status);} };
using Cancel = std::function<bool()>;
struct File { std::string name; uint64_t size=0; };
struct Id {
    AccountKey account; std::string stem;
    bool operator==(const Id& rhs) const { return account==rhs.account && stem==rhs.stem; }
};
struct Entry {
    Id id; std::string name, mainRef, canvasRef, thumbRef;
    bool associationWarning=false;
};
struct Catalog { std::vector<Entry> entries; bool namesWarning=false; };
class ReadSource {
public:
    virtual ~ReadSource()=default;
    virtual Result<std::vector<Account>> accounts(const Cancel& cancel)=0;
    virtual Status open(AccountKey account, const Cancel& cancel)=0;
    virtual Result<std::vector<File>> enumerate(const Cancel& cancel)=0;
    virtual Result<std::vector<uint8_t>> read(const std::string& ref,size_t limit,const Cancel& cancel)=0;
    virtual void close()=0;
};
enum class State { Unselected, AccountsLoading, AccountChooser, NoAccounts, CatalogLoading,
    CatalogSuspended, Ready, Empty, CatalogError };
struct Context {
    Account account;
    std::string selectedStem, focusTarget="choose";
    size_t ordinal=0;
    float scroll=0;
};
struct Model {
    State state=State::Unselected;
    Context context;
    Catalog catalog;
    std::vector<Account> accounts;
    Status status;
    uint64_t generation=0;
    bool hasAccount=false;
    State beforeChooser=State::Unselected;
    void chooseAccounts();
    void cancelChooser();
    void select(Account account);
    void loading();
    void accept(Result<Catalog> result);
    void suspend();
    bool resume();
};
struct Preview {
    int width=0,height=0;
    std::vector<uint8_t> rgba;
    bool canvas=false, fallback=false;
};
Result<Preview> loadPreview(ReadSource& source,const Entry& entry,const Cancel& cancel);
Result<Catalog> loadCatalog(ReadSource& source, AccountKey account, const Cancel& cancel);
}
