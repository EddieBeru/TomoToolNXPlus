#include "profile_selector.hpp"
#include <switch.h>
#include <cassert>
#include <cstring>
#include <iostream>

namespace {
Result createResult = 0, showResult = 0, profileResult = 0, serviceResult = 0;
AccountUid selected{{42, 17}};
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
void accountProfileClose(AccountProfile*) { ++profileCloses; }

int main() {
    using namespace borealis_app;
    auto result = selectStartupProfile();
    assert(result.state == ProfileSelectionState::Selected);
    assert(result.account.key.words[0] == 42 && result.account.key.words[1] == 17);
    assert(result.account.name.size() == 32 && accountCloses == 1 && profileCloses == 1);
    selected = {};
    result = selectStartupProfile();
    assert(result.state == ProfileSelectionState::Skipped && accountCloses == 1);
    showResult = 4;
    assert(selectStartupProfile().state == ProfileSelectionState::Failed);
    showResult = 0; createResult = 3;
    auto before = shows;
    assert(selectStartupProfile().state == ProfileSelectionState::Failed && shows == before);
    createResult = 0; selected = {{42,17}}; profileResult = 5;
    result = selectStartupProfile();
    assert(result.state == ProfileSelectionState::Selected && result.account.profileWarning);
    assert(result.account.name.empty() && accountCloses == 2 && profileCloses == 1);
    profileResult = 0; serviceResult = 6;
    result = selectStartupProfile();
    assert(result.state == ProfileSelectionState::Selected && result.account.profileWarning);
    assert(accountCloses == 2);
    std::cout << "native profile selector: OK\n";
}
