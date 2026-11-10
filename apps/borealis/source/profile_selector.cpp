#include "profile_selector.hpp"
#include <switch.h>
#include <cstring>

namespace borealis_app {
ProfileSelection selectStartupProfile() {
    PselUiSettings settings{};
    auto rc = pselUiCreate(&settings, PselUiMode_UserSelector);
    if (R_FAILED(rc))
        return {ProfileSelectionState::Failed, {}, rc};

    pselUiSetAllowUserCreation(&settings, false);
    pselUiSetNetworkServiceRequired(&settings, false);
    pselUiSetSkipButtonEnabled(&settings, true);
    AccountUid uid{};
    rc = pselUiShow(&settings, &uid);
    if (R_FAILED(rc))
        return {ProfileSelectionState::Failed, {}, rc};
    if (!(uid.uid[0] || uid.uid[1]))
        return {};

    ProfileSelection result;
    result.state = ProfileSelectionState::Selected;
    result.account.key = {{uid.uid[0], uid.uid[1]}};
    rc = accountInitialize(AccountServiceType_Application);
    if (R_FAILED(rc))
        rc = accountInitialize(AccountServiceType_System);
    if (R_FAILED(rc)) {
        result.account.profileWarning = true;
        return result;
    }
    AccountProfile profile{};
    rc = accountGetProfile(&profile, uid);
    if (R_SUCCEEDED(rc)) {
        AccountProfileBase base{};
        rc = accountProfileGet(&profile, nullptr, &base);
        if (R_SUCCEEDED(rc))
            result.account.name.assign(base.nickname, strnlen(base.nickname, sizeof(base.nickname)));
        accountProfileClose(&profile);
    }
    result.account.profileWarning = R_FAILED(rc);
    accountExit();
    return result;
}
}
