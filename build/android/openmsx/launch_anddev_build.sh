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
REVISION=$(python -c "import version; print version.extractRevisionString()")
if [ "${REVISION}" = "unknown" ]; then
  echo "LAB:ERROR Could not determine revision"
  exit 1
fi
VERSION_NAME=$(python -c "import version; print version.getVersionedPackageName()")
echo "LAB:INFO building revision ${REVISION} with version name ${VERSION_NAME}"

# Return to the directory containing this script and the AndroidAppSettings files
cd "${my_app_android_dir}"

APP_SETTINGS_CFG="${my_home_dir}/build/android/openmsx/AndroidAppSettings.cfg"
cp ${APP_SETTINGS_CFG}.template ${APP_SETTINGS_CFG}
. ${APP_SETTINGS_CFG}
MANIFEST="${sdl_android_port_path}/project/AndroidManifest.xml"
if [ "$AppVersionCode" != "${REVISION}" ]; then
	sed -i "s/^AppVersionCode=.*$/AppVersionCode=${REVISION}/" ${APP_SETTINGS_CFG}
#	sed -i "s^android:versionCode=.*^android:versionCode=\"${REVISION}\"^" ${MANIFEST}
fi
if [ "$AppVersionName" != "${VERSION_NAME}" ]; then
	sed -i "s/^AppVersionName=.*$/AppVersionName=${VERSION_NAME}/" ${APP_SETTINGS_CFG}
#	sed -i "s^android:versionName=.*^android:versionName=\"${VERSION_NAME}\"^" ${MANIFEST}
fi

echo "LAB:INFO launching commandergenius build file"
cd "${sdl_android_port_path}"
./build.sh $*
exit $?
