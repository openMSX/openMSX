#!/bin/bash
#set -xv

# TODO: find out if flavour can be passed from SDL build environment
openmsx_flavour="opt"

echo "AB:INFO Starting AndroidBuild.sh, #params: $#, params: $*"
echo "AB:INFO pwd: $(pwd)"

# Read environment.props (if it exists) to get following two params:
#   sdl_android_port_path
#   my_home_dir
if [ ! -f environment.props ]; then
    echo "AB:ERROR: No file environment.props in $(pwd)"
    exit 1
fi
. ./environment.props

# Remember location of the current directory, which is the directory
# with all android specific code for the app
my_app_android_dir="$(pwd)"


# Use latest version of the setEnvironment script; it is the one that uses GCC 4.9
set_sdl_app_environment="${sdl_android_port_path}/project/jni/application/setEnvironment.sh"

# Parsing the CPU architecture information
CPU_ARCH="$1"
if [ "${CPU_ARCH}" = "armeabi" ]; then
    so_file=libapplication.so
    openmsx_target_cpu=arm
else
    echo "AB:ERROR Unsupported architecture: $1"
    exit 1
fi

#echo "AB:INFO current shell: ${SHELL}"
#echo "AB:INFO BEGIN all environment params:"
#set
#echo "AB:INFO END all environment params:"
#cd ${my_home_dir}

# Unset make related environment parameters that get set by the
# SDL for Android build system and that conflict with openMSX
# build system
unset BUILD_NUM_CPUS
unset MAKEFLAGS
unset MAKELEVEL
unset MAKEOVERRIDES
unset MFLAGS
unset V

# The commandergenius build will modify the PATH.
# Pick the right compiler now and store its absolute path.
CXX=$(which arm-linux-androideabi-g++)
if [ $? -ne 0 ]; then
    echo "AB:ERROR Could not find compiler"
    exit 1
fi

cpu_count=1
if [ -f /proc/cpuinfo ]; then
	cpu_count=$(grep "processor[[:space:]]*:" /proc/cpuinfo | wc -l)
	if [ ${cpu_count} -eq 0 ]; then
		cpu_count=1
	fi
fi
echo "AB:INFO Detected ${cpu_count} CPUs for parallel build"

echo "AB:INFO Making this app for CPU architecture ${CPU_ARCH}"
export SDL_ANDROID_PORT_PATH="${sdl_android_port_path}"
unset BUILD_EXECUTABLE

#"${set_sdl_app_environment}" /bin/bash -c "set"
"${set_sdl_app_environment}" /bin/bash -c "\
    echo \"AB:INFO entering openMSX home directory: ${my_home_dir}\"; \
    cd ${my_home_dir};\
    echo \"AB:INFO CXX: \${CXX}\";\
    export _CC=\${CC};\
    export _LD=\${LD};\
    unset CXXFLAGS;\
    make -k -j ${cpu_count} staticbindist\
         OPENMSX_TARGET_CPU=${openmsx_target_cpu}\
         OPENMSX_TARGET_OS=android\
         OPENMSX_FLAVOUR=${openmsx_flavour}\
         CXX=${CXX}\
"
if [ $? -ne 0 ]; then
    echo "AB:ERROR Make failed"
    exit 1
fi

# Return to the directory containing this script and all data about this application
# that the SDL APK build system requires
cd "${my_app_android_dir}"

# Copy the shared library overhere
echo "AB:INFO Copying output file into android directory $(pwd)"
cp "${my_home_dir}/derived/${openmsx_target_cpu}-android-${openmsx_flavour}-3rd/lib/openmsx.so" "${so_file}"
if [ $? -ne 0 ]; then
    echo "AB:ERROR Copy failed"
fi

echo "AB:INFO Done with build of app"

echo "AB:INFO Copying icon file"
openmsx_icon_file="${my_home_dir}/share/icons/openMSX-logo-256.png"
cp -p "${openmsx_icon_file}" icon.png

echo "AB:INFO Building appdata.zip"
rm -f AndroidData/appdata.zip
rm -rf AndroidData/appdata
mkdir -p AndroidData/appdata/openmsx_system
cd "${my_home_dir}"/derived/${openmsx_target_cpu}-android-${openmsx_flavour}-3rd/bindist/install/share
tar -c -f - . | ( cd "${my_app_android_dir}"/AndroidData/appdata/openmsx_system ; tar xf - )
cd "${my_app_android_dir}"/AndroidData/appdata
zip -r ../appdata.zip * > /dev/null
cd ..
rm -rf appdata
echo "AB:INFO Done rebuilding appdata.zip"

MANIFEST="${sdl_android_port_path}/project/AndroidManifest.xml"

# Patch manifest file to target android 2.3 and older so that the
# virtual menu button will be rendered by newer Android versions
sed -i "s^android:targetSdkVersion=\"[0-9]*\"^android:targetSdkVersion=\"10\"^" ${MANIFEST}

# Remove network permissions from manifest. OpenMSX does not need it
sed -i "s/<uses-permission android:name=\"android.permission.INTERNET\"><\/uses-permission>/<\!-- -->/" ${MANIFEST}
exit 0
