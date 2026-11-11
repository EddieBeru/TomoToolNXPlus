#include "profile_selector.hpp"
#include <switch.h>
#include <cstring>

namespace borealis_app {
ProfileSelection selectNativeProfile() {
    PselUiSettings settings{};
    auto rc = pselUiCreate(&settings, PselUiMode_UserSelector);
    if (R_FAILED(rc))
        return {ProfileSelectionState::Failed, {}, rc, {}};

    pselUiSetAllowUserCreation(&settings, false);
    pselUiSetNetworkServiceRequired(&settings, false);
    pselUiSetSkipButtonEnabled(&settings, true);
    AccountUid uid{};
    rc = pselUiShow(&settings, &uid);
    if (R_FAILED(rc))
        return {ProfileSelectionState::Failed, {}, rc, {}};
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
        u32 imageSize = 0;
        if (R_SUCCEEDED(accountProfileGetImageSize(&profile, &imageSize)) &&
            imageSize > 0 && imageSize <= 256 * 1024) {
            result.avatarJpeg.resize(imageSize);
            u32 actualSize = 0;
            auto imageResult = accountProfileLoadImage(&profile, result.avatarJpeg.data(),
                result.avatarJpeg.size(), &actualSize);
            if (R_FAILED(imageResult) || !actualSize || actualSize > imageSize)
                result.avatarJpeg.clear();
            else
                result.avatarJpeg.resize(actualSize);
        }
        accountProfileClose(&profile);
    }
    result.account.profileWarning = R_FAILED(rc);
    accountExit();
    return result;
}
}
