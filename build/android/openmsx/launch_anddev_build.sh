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

./generate_AndroidAppSettings.sh
if [ $? -ne 0 ]; then
	exit 1
fi

echo "LAB:INFO launching commandergenius build file"
cd "${sdl_android_port_path}"
./build.sh $*
exit $?
