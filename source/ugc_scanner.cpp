// ugc_scanner.cpp – no std::filesystem (incompatible with -fno-exceptions)
#include "ugc_scanner.h"
#include <algorithm>
#include <map>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>

static std::string ToLower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}
static bool EndsWith(const std::string& s, const std::string& suf) {
    return s.size() >= suf.size() &&
           s.compare(s.size()-suf.size(), suf.size(), suf) == 0;
}
static std::string Filename(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

namespace UgcScanner {

std::vector<UgcTextureEntry> Scan(const std::string& folderPath) {
    std::vector<UgcTextureEntry> entries;

    DIR* dir = opendir(folderPath.c_str());
    if (!dir) return entries;

    const std::string ugctexSuffix = ".ugctex.zs";
    const std::string thumbSuffix  = "_thumb_ugctex.zs"; // UgcX_Thumb_ugctex.zs
    const std::string thumbSuffix2 = "_thumb.ugctex.zs"; // UgcX_Thumb.ugctex.zs

    std::map<std::string, std::string> byName;
    std::vector<std::string> mainFiles;

    struct dirent* de;
    while ((de = readdir(dir)) != nullptr) {
        if (de->d_type != DT_REG && de->d_type != DT_UNKNOWN) continue;
        std::string name  = de->d_name;
        std::string lower = ToLower(name);
        if (!EndsWith(lower, ".zs")) continue;

        std::string fullPath = folderPath + "/" + name;
        if (de->d_type == DT_UNKNOWN) {
            struct stat st;
            if (stat(fullPath.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
        }

        byName[lower] = fullPath;
        if (EndsWith(lower, ugctexSuffix) && !EndsWith(lower, thumbSuffix) && !EndsWith(lower, thumbSuffix2))
            mainFiles.push_back(fullPath);
    }
    closedir(dir);

    std::sort(mainFiles.begin(), mainFiles.end(), [](const std::string& a, const std::string& b){
        return ToLower(Filename(a)) < ToLower(Filename(b));
    });

    for (auto& mainPath : mainFiles) {
        std::string filename = Filename(mainPath);
        std::string stem     = filename.substr(0, filename.size() - ugctexSuffix.size());

        UgcTextureEntry e;
        e.stem       = stem;
        e.ugctexPath = mainPath;

        std::string thumbName1 = ToLower(stem + "_Thumb_ugctex.zs");  // _Thumb_ugctex.zs
        std::string thumbName2 = ToLower(stem + "_Thumb.ugctex.zs");  // _Thumb.ugctex.zs
        std::string canvasName = ToLower(stem + ".canvas.zs");

        if      (byName.count(thumbName1)) e.thumbPath = byName[thumbName1];
        else if (byName.count(thumbName2)) e.thumbPath = byName[thumbName2];
        if (byName.count(canvasName)) e.canvasPath = byName[canvasName];

        entries.push_back(std::move(e));
    }
    return entries;
}

} // namespace UgcScanner

namespace UgcScanner {
Association Associate(const std::vector<std::string>& filenames) {
    auto lowerAscii=[](std::string value) {
        for(auto& c:value)if(c>='A' && c<='Z')c=char(c+('a'-'A'));
        return value;
    };
    Association result;
    if(filenames.size()>8192) {result.ok=false;result.limit=true;return result;}
    std::map<std::string,std::vector<std::string>> byName;
    std::vector<std::string> mains;
    for(const auto& name:filenames) {
        if(name.empty() || name=="." || name==".." || name.find_first_of("/\\")!=std::string::npos) {
            result.ok=false; return result;
        }
        auto lower=lowerAscii(name);
        byName[lower].push_back(name);
        if(EndsWith(lower,".ugctex.zs") && !EndsWith(lower,"_thumb.ugctex.zs")
           && !EndsWith(lower,"_thumb_ugctex.zs")) {
            if(name.size()==10){result.ok=false;return result;}
            mains.push_back(name);
        }
    }
    if(mains.size()>2048) {result.ok=false;result.limit=true;return result;}
    std::sort(mains.begin(),mains.end(),[&](const auto& a,const auto& b) {
        auto x=lowerAscii(a), y=lowerAscii(b);return x==y?a<b:x<y;
    });
    for(const auto& main:mains) {
        UgcTextureEntry e; e.stem=main.substr(0,main.size()-10); e.ugctexPath=main;
        auto associate=[&](const std::string& suffix) {
            auto it=byName.find(lowerAscii(e.stem+suffix));
            if(it==byName.end()) return std::string{};
            if(it->second.size()!=1) {result.warning=true;return std::string{};}
            return it->second.front();
        };
        e.canvasPath=associate(".canvas.zs");
        e.thumbPath=associate("_Thumb_ugctex.zs");
        if(e.thumbPath.empty()) e.thumbPath=associate("_Thumb.ugctex.zs");
        result.entries.push_back(std::move(e));
    }
    return result;
}
}
