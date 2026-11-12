#include "XString.h"
#include "VxUserConfig.h"
#include "VxUserConfigInternal.h"
#include <string.h>

namespace {
struct UserConfigProfile {
    XString applicationName;
    XString homeEnvironmentVariable;

    UserConfigProfile()
        : applicationName("NeMo2"), homeEnvironmentVariable("NEMO_USER_CONFIG_HOME") {}
};

UserConfigProfile &Profile() {
    static UserConfigProfile profile;
    return profile;
}
}

XBOOL VxSetUserConfigProfile(const char *appName, const char *homeEnvironmentVariable) {
    const char *name = appName && *appName ? appName : "NeMo2";
    const char *variable = homeEnvironmentVariable && *homeEnvironmentVariable
        ? homeEnvironmentVariable : "NEMO_USER_CONFIG_HOME";
    if (strlen(name) > XString::MAX_LENGTH || strlen(variable) > XString::MAX_LENGTH)
        return FALSE;
    UserConfigProfile &profile = Profile();
    profile.applicationName = name;
    profile.homeEnvironmentVariable = variable;
    return TRUE;
}

const char *VxUserConfigApplicationName() {
    return Profile().applicationName.CStr();
}

const char *VxUserConfigHomeEnvironmentVariable() {
    return Profile().homeEnvironmentVariable.CStr();
}
