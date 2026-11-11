#include "profile_selector.hpp"
#include <switch.h>
#include <cassert>
#include <cstring>
#include <iostream>

namespace {
Result createResult = 0, showResult = 0, profileResult = 0, serviceResult = 0;
AccountUid selected{{42, 17}};
u32 imageSize = 4;
Result imageResult = 0, imageLoadResult = 0;
bool invalidImageLength = false;
int shows = 0, accountCloses = 0, profileCloses = 0;
}
Result pselUiCreate(PselUiSettings* ui, PselUiMode mode) {
    assert(mode == PselUiMode_UserSelector);
    *ui = {};
    return createResult;
}
void pselUiSetAllowUserCreation(PselUiSettings* ui, bool enabled) { ui->allowCreation = enabled; }
void pselUiSetNetworkServiceRequired(PselUiSettings* ui, bool enabled) { ui->networkRequired = enabled; }
void pselUiSetSkipButtonEnabled(PselUiSettings* ui, bool enabled) { ui->skipButton = enabled; }
Result pselUiShow(PselUiSettings* ui, AccountUid* uid) {
    ++shows;
    assert(!ui->allowCreation && !ui->networkRequired && ui->skipButton);
    *uid = selected;
    return showResult;
}
Result accountInitialize(AccountServiceType) { return serviceResult; }
void accountExit() { ++accountCloses; }
Result accountGetProfile(AccountProfile*, AccountUid uid) {
    assert(uid.uid[0] == 42 && uid.uid[1] == 17);
    return profileResult;
}
Result accountProfileGet(AccountProfile*, AccountUserData*, AccountProfileBase* base) {
    std::memset(base->nickname, 'A', sizeof(base->nickname));
    return profileResult;
}
Result accountProfileGetImageSize(AccountProfile*, u32* size) { *size=imageSize;return imageResult; }
Result accountProfileLoadImage(AccountProfile*, void* out, size_t size, u32* actual) {
    assert(size==imageSize);std::memset(out,7,size);*actual=u32(size)+(invalidImageLength?1:0);return imageLoadResult;
}
void accountProfileClose(AccountProfile*) { ++profileCloses; }

int main() {
    using namespace borealis_app;
    auto result = selectNativeProfile();
    assert(result.state == ProfileSelectionState::Selected);
    assert(result.account.key.words[0] == 42 && result.account.key.words[1] == 17);
    assert(result.account.name.size() == 32 && accountCloses == 1 && profileCloses == 1);
    assert(result.avatarJpeg==std::vector<uint8_t>(4,7));
    imageSize=256*1024+1;
    result=selectNativeProfile();assert(result.avatarJpeg.empty());
    imageSize=4;imageResult=5;
    result=selectNativeProfile();assert(result.avatarJpeg.empty());
    imageResult=0;
    assert(accountCloses==3 && profileCloses==3);
    imageLoadResult=4;
    result=selectNativeProfile();assert(result.avatarJpeg.empty());imageLoadResult=0;
    invalidImageLength=true;
    result=selectNativeProfile();assert(result.avatarJpeg.empty());invalidImageLength=false;
    assert(accountCloses==5 && profileCloses==5);
    selected = {};
    result = selectNativeProfile();
    assert(result.state == ProfileSelectionState::Skipped && accountCloses == 5);
    showResult = 4;
    assert(selectNativeProfile().state == ProfileSelectionState::Failed);
    showResult = 0; createResult = 3;
    auto before = shows;
    assert(selectNativeProfile().state == ProfileSelectionState::Failed && shows == before);
    createResult = 0; selected = {{42,17}}; profileResult = 5;
    result = selectNativeProfile();
    assert(result.state == ProfileSelectionState::Selected && result.account.profileWarning);
    assert(result.account.name.empty() && accountCloses == 6 && profileCloses == 5);
    profileResult = 0; serviceResult = 6;
    result = selectNativeProfile();
    assert(result.state == ProfileSelectionState::Selected && result.account.profileWarning);
    assert(accountCloses == 6);
    std::cout << "native profile selector: OK\n";
}
