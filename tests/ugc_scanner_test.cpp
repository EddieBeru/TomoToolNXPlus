#include "ugc_scanner.h"
#include <cassert>
#include <iostream>
int main() {
    auto r=UgcScanner::Associate({"UgcFood001.ugctex.zs","UgcFood001_Thumb_ugctex.zs",
      "UgcFood001.canvas.zs","orphan_Thumb.ugctex.zs","UgcFood000.ugctex.zs"});
    assert(r.ok && r.entries.size()==2 && r.entries[0].stem=="UgcFood000");
    assert(!r.entries[1].thumbPath.empty() && !r.entries[1].canvasPath.empty());
    assert(!UgcScanner::Associate({"../a.ugctex.zs"}).ok);
    assert(!UgcScanner::Associate(std::vector<std::string>(8193,"a")).ok);
    std::vector<std::string> names;
    for(int i=0;i<2049;i++) names.push_back(std::to_string(i)+".ugctex.zs");
    assert(!UgcScanner::Associate(names).ok);
    r=UgcScanner::Associate({"X.ugctex.zs","x.ugctex.zs","X.canvas.zs","x.canvas.zs"});
    assert(r.ok && r.entries.size()==2 && r.warning && r.entries[0].canvasPath.empty());
    std::cout<<"ugc scanner: OK\n";
}
