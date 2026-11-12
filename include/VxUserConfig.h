#ifndef VXUSERCONFIG_H
#define VXUSERCONFIG_H

#include "VxMathDefines.h"

/**
 * @brief Selects the process-wide application configuration profile.
 * Call before opening configuration sections or starting worker threads. Names
 * are copied; null or empty values select "NeMo2" and "NEMO_USER_CONFIG_HOME".
 * The environment variable contains a parent directory, read on each path lookup.
 * VxGetUserConfigPath uses this application name when its appName is null or empty;
 * VxConfig stores registry.ini in that directory.
 * Returns FALSE for names exceeding XString::MAX_LENGTH, leaving the profile intact.
 */
VX_EXPORT XBOOL VxSetUserConfigProfile(const char *appName, const char *homeEnvironmentVariable = NULL);

#endif
