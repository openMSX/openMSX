# Android flavour. Copy all ANDROID_CXXFLAGS into TARGET_FLAGS
# Otherwise, probe fails as it won't be able to find the include files

TARGET_FLAGS+=$(ANDROID_CXXFLAGS)
CXXFLAGS:=$(ANDROID_CXXFLAGS)
