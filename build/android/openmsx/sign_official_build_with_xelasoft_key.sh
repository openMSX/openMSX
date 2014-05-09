#!/bin/bash

echo "LAB:INFO Signing APK with xelasoft certificate, for publication in keystore"

# Read environment.props (if it exists) to get following two params:
#   sdl_android_port_path
#   my_home_dir
if [ ! -f environment.props ]; then
    echo "LAB:ERROR: No file environment.props in $(pwd)"
    exit 1
fi
. ./environment.props

export ANDROID_KEYSTORE_FILE=~awulms/Ontwikkel/android/release_certificate/xelasoft_eu.keystore
export ANDROID_KEYSTORE_ALIAS=xelasoft

if [ ! -f ${ANDROID_KEYSTORE_FILE} ]; then
	echo "LAB:ERROR Keystore not found ${ANDROID_KEYSTORE_FILE}"
	exit 1
fi

echo "LAB:INFO launching commandergenius sign.sh script"
cd "${sdl_android_port_path}"
./sign.sh
exit $?
