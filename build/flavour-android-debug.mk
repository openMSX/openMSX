# Android flavour. Copy all ANDROID_CXXFLAGS into TARGET_FLAGS
# Otherwise, probe fails as it won't be able to find the include files

include build/flavour-android.mk
TARGET_FLAGS+=-fPIE
#CXXFLAGS+=-O0 -g -DDEBUG -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC \
#          -D_GLIBCPP_CONCEPT_CHECKS
CXXFLAGS+=-DDEBUG -O0 -fPIE

# Strip executable?
OPENMSX_STRIP:=false
