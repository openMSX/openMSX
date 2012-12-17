#set -xv
echo "AB:INFO Starting AndroidBuild.sh, #params: $#, params: $*"
echo "AB:INFO pwd: $(pwd)"

# TODO: find out if flavour can be passed from SDL build environment
#openmsx_flavour="android-debug"
openmsx_flavour="android"

if [ ! -f environment.props ]; then
    echo "AB:ERROR: No file environment.props in $(pwd)"
    exit 1
fi

# Read environment.props to get following two params:
#   sdl_android_port_path
#   my_home_dir
. ./environment.props

# Remember location of the current directory, which is the directory
# with all android specific code for the app
my_app_android_dir="$(pwd)"

# Use latest version of the setEnvironment script; it is the one that uses GCC 4.6
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

echo "AB:INFO entering openMSX home directory: ${my_home_dir}"
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
export CXXFLAGS='-frtti -fexceptions -marm'
export LDFLAGS='-lpng'
unset BUILD_EXECUTABLE

if [ $openmsx_flavour = "android" ]; then
    CXX_FLAGS_FILTER="sed -e 's/\\-mthumb//'"
elif [ $openmsx_flavour = "android-debug" ]; then
    CXX_FLAGS_FILTER="sed -e 's/\\-mthumb//' -e 's/\\-DNDEBUG//g'"
else
	echo "AB:ERROR Unknown openmsx_flavour: $openmsx_flavour"
fi
echo "AB:DEBUG CXX_FLAGS_FILTER: $CXX_FLAGS_FILTER"

#"${set_sdl_app_environment}" /bin/sh -c "set"
"${set_sdl_app_environment}" /bin/sh -c "\
    cd ${my_home_dir};\
    echo \"AB:INFO CXX: \${CXX}\";\
    echo \"AB:INFO CXXFLAGS: \${CXXFLAGS}\";\
    export _CC=\${CC};\
    export _LD=\${LD};\
    export ANDROID_LDFLAGS=\${LDFLAGS};\
    export ANDROID_CXXFLAGS=\$(echo \${CXXFLAGS} | $CXX_FLAGS_FILTER);\
    echo \"AB:INFO ANDROID_CXXFLAGS: \${ANDROID_CXXFLAGS}\";\
    unset CXXFLAGS;\
    make -j ${cpu_count} all\
         OPENMSX_TARGET_CPU=${openmsx_target_cpu}\
         OPENMSX_TARGET_OS=android\
         OPENMSX_FLAVOUR=${openmsx_flavour}\
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
cp "${my_home_dir}/derived/${openmsx_target_cpu}-android-${openmsx_flavour}/lib/openmsx.so" "${so_file}"
if [ $? -ne 0 ]; then
    echo "AB:ERROR Copy failed"
fi

echo "AB:INFO Done with build of app"

echo "AB:INFO Copying icon file"
openmsx_icon_file="${my_home_dir}/share/icons/openMSX-logo-128.png"
cp -p "${openmsx_icon_file}" icon.png

echo "AB:INFO Validating if appdata.zip must be rebuild"
if [ ! -f AndroidData/appdata.zip ]; then
	newfiles=1
else
	newfiles=$(find ${my_home_dir}/share ${my_home_dir}/Contrib/cbios -newer AndroidData/appdata.zip | wc -l)
fi
if [ ${newfiles} -gt 0 ]; then
	echo "AB:INFO Rebuilding appdata.zip"
	rm -f AndroidData/appdata.zip
	rm -rf AndroidData/appdata
	mkdir -p AndroidData/appdata/openmsx_system
	cd AndroidData/appdata/openmsx_system
	cp -r "${my_home_dir}"/share/* .
	cd machines
	cp -r "${my_home_dir}"/Contrib/cbios/* .
	cd "${my_app_android_dir}"/AndroidData/appdata
	zip -r ../appdata.zip * > /dev/null
	cd ..
	rm -rf appdata
	echo "AB:INFO Done rebuilding appdata.zip"
else
	echo "AB/INFO appdata.zip is still fine"
fi

APP_SETTINGS_CFG="${my_home_dir}/build/android/openmsx/AndroidAppSettings.cfg"
revision=$(PYTHONPATH="${my_home_dir}/build" python -c \
	"import version; print version.extractRevisionString()" \
	)
version_name=$(PYTHONPATH="${my_home_dir}/build" python -c \
	"import version; print version.getVersionedPackageName()" \
	)
echo "AB:INFO revision: $revision"
. ${APP_SETTINGS_CFG}
MANIFEST="${sdl_android_port_path}/project/AndroidManifest.xml"
if [ "$AppVersionCode" != "$revision" ]; then
	sed -i "s/^AppVersionCode=.*$/AppVersionCode=$revision/" ${APP_SETTINGS_CFG}
	sed -i "s^android:versionCode=.*^android:versionCode=\"$revision\"^" ${MANIFEST}
fi
if [ "$AppVersionName" != "$version_name" ]; then
	sed -i "s/^AppVersionName=.*$/AppVersionName=$version_name/" ${APP_SETTINGS_CFG}
	sed -i "s^android:versionName=.*^android:versionName=\"$version_name\"^" ${MANIFEST}
fi

# Patch manifest file to target android 2.3 and older so that the
# virtual menu button will be rendered by newer Android versions
sed -i "s^android:targetSdkVersion=\"[0-9]*\"^android:targetSdkVersion=\"10\"^" ${MANIFEST}

exit 0
AppVersionCode=VERSION_CODE_PLACEHOLDER
AppVersionName=VERSION_NAME_PLACEHOLDER
