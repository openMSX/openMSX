#!/bin/bash

echo "LAB:INFO Starting launch_anddev_build.sh"

# Read environment.props (if it exists) to get following two params:
#   sdl_android_port_path
#   my_home_dir
if [ ! -f environment.props ]; then
    echo "LAB:ERROR: No file environment.props in $(pwd)"
    exit 1
fi
. ./environment.props

# Remember location of the current directory, which is the directory
# with all android specific code for the app
my_app_android_dir="$(pwd)"


# Determine current revision and version name
PYTHONPATH="${my_home_dir}/build"
export PYTHONPATH
cd "${my_home_dir}"
# The (Android) version code must be an increasing number so that Android application
# manager can recognize that it is a new version of the same application and take
# appropriate action like retaining or migrating the user settings
# The easiest way to get an increasing number for new builds in git is by
# counting the number of commit messages.
VERSION_CODE=$(git log --oneline | wc -l)

# The (Android) version name can be any arbitrary string that is hopefully
# meaningfull to the user. Best is to use the version package name
# to be aligned with the version name used for builds for other platforms.
VERSION_NAME=$(python -c "import version; print version.getVersionedPackageName()")

echo "LAB:INFO building version ${VERSION_CODE} with version name ${VERSION_NAME}"

# Return to the directory containing this script and the AndroidAppSettings files
cd "${my_app_android_dir}"

APP_SETTINGS_CFG="${my_home_dir}/build/android/openmsx/AndroidAppSettings.cfg"
cp ${APP_SETTINGS_CFG}.template ${APP_SETTINGS_CFG}
. ${APP_SETTINGS_CFG}
MANIFEST="${sdl_android_port_path}/project/AndroidManifest.xml"
if [ "$AppVersionCode" != "${VERSION_CODE}" ]; then
	sed -i "s/^AppVersionCode=.*$/AppVersionCode=${VERSION_CODE}/" ${APP_SETTINGS_CFG}
#	sed -i "s^android:versionCode=.*^android:versionCode=\"${VERSION_CODE}\"^" ${MANIFEST}
fi
if [ "$AppVersionName" != "${VERSION_NAME}" ]; then
	sed -i "s/^AppVersionName=.*$/AppVersionName=${VERSION_NAME}/" ${APP_SETTINGS_CFG}
#	sed -i "s^android:versionName=.*^android:versionName=\"${VERSION_NAME}\"^" ${MANIFEST}
fi

echo "LAB:INFO launching commandergenius build file"
cd "${sdl_android_port_path}"
./build.sh $*
exit $?
