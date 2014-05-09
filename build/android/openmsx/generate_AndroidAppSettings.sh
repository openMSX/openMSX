#!/bin/bash

echo "LAB:INFO Generating AndroidAppSettings.cfg from template file"

# Read environment.props (if it exists) to get following two params:
#   sdl_android_port_path
#   my_home_dir
if [ ! -f environment.props ]; then
    echo "LAB:ERROR: No file environment.props in $(pwd)"
    exit 1
fi
. ./environment.props

# Determine current revision and version name
PYTHONPATH="${my_home_dir}/build"
export PYTHONPATH
cd "${my_home_dir}"
# The (Android) version code must be an increasing number so that Android application
# manager can recognize that it is a new version of the same application and take
# appropriate action like retaining or migrating the user settings
# The easiest way to get an increasing number for new builds in git is by
# counting the number of commit messages.
#VERSION_CODE=$(git log --oneline | wc -l)
VERSION_CODE=$(python -c "import version; print version.getAndroidVersionCode()")

# The (Android) version name can be any arbitrary string that is hopefully
# meaningfull to the user. Best is to use the version package name
# to be aligned with the version name used for builds for other platforms.
VERSION_NAME=$(python -c "import version; print version.getVersionedPackageName()")

# Return to the directory containing this script and the AndroidAppSettings files
cd "${my_home_dir}/build/android/openmsx"

# Determine version number of AndroidAppSettings supported
# by current version of anddev Android build script
CHANGE_APP_SETTINS_SCRIPT="${sdl_android_port_path}/changeAppSettings.sh"
if [ ! -f "${CHANGE_APP_SETTINS_SCRIPT}" ]; then
	echo "LAB:ERROR: No such file ${CHANGE_APP_SETTINS_SCRIPT}."
	echo "           Please follow instructions in compilation guide for android"
	echo "           port to correctly set-up the android build environment."
	exit 1
fi
CHANGE_APP_SETTINGS_VERSION=$(grep 'CHANGE_APP_SETTINGS_VERSION=[0-9][0-9]*' "${CHANGE_APP_SETTINS_SCRIPT}")
CHANGE_APP_SETTINGS_VERSION=${CHANGE_APP_SETTINGS_VERSION#*=}

if [ -z "${CHANGE_APP_SETTINGS_VERSION}" ]; then
	# Latest version of changeAppSettings.sh no longer contains an explicit version
	# number for the app-settings
	# However, it does have a mechanism to automagically upgrade to newer app-settings
	# with (hopefully sane) default values
	# As such, use a non-versioned template when the changeAppSettings.sh does not
	# contain an explicit settings version
	CHANGE_APP_SETTINGS_VERSION=noVersion
fi

APP_SETTINGS_CFG="AndroidAppSettings.cfg"
APP_SETTINGS_TEMPLATE="${APP_SETTINGS_CFG}.template.${CHANGE_APP_SETTINGS_VERSION}"
if [ ! -f ${APP_SETTINGS_TEMPLATE} ]; then
	echo "LAB:ERROR: No such file ${APP_SETTINGS_TEMPLATE}."
	echo "           Please create one manually. It can be based on one"
	echo "           of the existing app settings template file"
	exit 1
fi

cp ${APP_SETTINGS_TEMPLATE} ${APP_SETTINGS_CFG}
. ${APP_SETTINGS_CFG}
if [ "$AppVersionCode" != "${VERSION_CODE}" ]; then
	sed -i "s/^AppVersionCode=.*$/AppVersionCode=${VERSION_CODE}/" ${APP_SETTINGS_CFG}
fi
if [ "$AppVersionName" != "${VERSION_NAME}" ]; then
	sed -i "s/^AppVersionName=.*$/AppVersionName=${VERSION_NAME}/" ${APP_SETTINGS_CFG}
fi

echo "LAB:INFO AndroidAppSettings.cfg generated for version ${VERSION_CODE} with version name ${VERSION_NAME}"

