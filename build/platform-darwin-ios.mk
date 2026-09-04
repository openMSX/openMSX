# Configuration for iOS (iPhone/iPad), arm64.
#
# Invoke with:
#   make OPENMSX_TARGET_CPU=aarch64 OPENMSX_TARGET_OS=darwin-ios
# This builds for the simulator; pass IOS_SDK=iphoneos for a device build.
#
# Named "darwin-ios" so it matches the build system's existing darwin% filters
# (clang, libc++, Objective-C++) while staying distinct from desktop macOS.

# iOS apps are not launched from $PATH.
USE_SYMLINK:=false

# The app bundle supplies the icon.
SET_WINDOW_ICON:=false

# Emit a static libopenmsx.a for an Xcode app target to link against; the app
# supplies the UIKit entry point. Linking an executable here would fail on a
# missing _main, since SDL's <SDL_main.h> renames it to SDL_main.
EXEEXT:=
LIBRARYEXT:=.a

# See configure.py for LINK_MODE definition and usage
LINK_MODE:=3RD_STA_GLES

# Which SDK to build against. ':=' rather than '?=' because buildinfo2code.py
# parses this file with a mini-parser that only understands '=', ':=' and '+='.
# A command line override still wins.
IOS_SDK:=iphonesimulator

# openMSX formats floats with std::format, which routes through std::to_chars.
# Apple's libc++ only ships the floating-point overloads from iOS 16.3
# (_LIBCPP_AVAILABILITY_TO_CHARS_FLOATING_POINT); targeting lower is a hard
# "unavailable" error, not a fallback.
IOS_MIN_VERSION:=16.3

IOS_SYSROOT:=$(shell xcrun --sdk $(IOS_SDK) --show-sdk-path)

ifeq ($(IOS_SDK),iphonesimulator)
IOS_VERSION_FLAG:=-mios-simulator-version-min=$(IOS_MIN_VERSION)
else
IOS_VERSION_FLAG:=-miphoneos-version-min=$(IOS_MIN_VERSION)
endif

# Simulator and device objects are link-incompatible (Mach-O platform 7 versus
# 2), so they must not share a derived/ tree; main.mk appends this to
# BUILD_PATH. Plain ifeq rather than $(if $(filter ...)) for the same
# mini-parser reason as IOS_SDK above.
ifeq ($(IOS_SDK),iphonesimulator)
IOS_BUILD_SUFFIX:=-sim
else
IOS_BUILD_SUFFIX:=-dev
endif

TARGET_FLAGS+=-arch arm64
TARGET_FLAGS+=-isysroot $(IOS_SYSROOT)
TARGET_FLAGS+=$(IOS_VERSION_FLAG)
TARGET_FLAGS+=-stdlib=libc++

# Enable automatic reference counting in Objective-C, as platform-darwin.mk does.
ifneq ($(3RDPARTY_FLAG),true)
TARGET_FLAGS+=-fobjc-arc
endif

# The bundled 3rd party C sources predate clang treating implicit function
# declarations and int/pointer conversions as errors. Demote those for the
# dependency builds only; openMSX's own code keeps the strict default.
ifeq ($(3RDPARTY_FLAG),true)
TARGET_FLAGS+=-Wno-error=implicit-function-declaration -Wno-error=int-conversion
endif

ifeq ($(origin CXX),default)
CXX:=$(shell xcrun --sdk $(IOS_SDK) --find clang++)
endif
