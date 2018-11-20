/*
 * Android API wrapper
 *
 * It makes a few Android functions available for the android flavour of openMSX
 *
 */

#include "build-info.hh"
#include "AndroidApiWrapper.hh"

#if PLATFORM_ANDROID

#include "openmsx.hh"
#include <stdlib.h>
#include <string.h>
#include <jni.h>

// The jniVM parameter gets set by the JNI mechanism directly after loading the
// shared lib containing openMSX by invoking a method with following
// signature if it exists in the shared lib:
//   JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
// This method is defined at the end of this source file and must be able to
// initialize this jniVM parameter, hence it must exist outside the openmsx
// namespace
static JavaVM *jniVM = NULL;

namespace openmsx {

std::string AndroidApiWrapper::getStorageDirectory()
{
	JNIEnv * jniEnv = NULL;
	jclass cls;
	jmethodID mid;
	jobject storageDirectory;
	jstring storageDirectoryName;

	jniVM->AttachCurrentThread(&jniEnv, NULL);
	if( !jniEnv ) {
		throw JniException("Java VM AttachCurrentThread() failed");
	}
	cls = jniEnv->FindClass("android/os/Environment");
	if (cls == 0) {
		throw JniException("Cant find class android/os/Environment");
	}
	mid = jniEnv->GetStaticMethodID(cls, "getExternalStorageDirectory", "()Ljava/io/File;");
	if (mid == 0) {
		throw JniException("Cant find getExternalStorageDirectory method");
	}
	storageDirectory = jniEnv->CallStaticObjectMethod(cls, mid);
	if (storageDirectory == 0) {
		throw JniException("Cant get storageDirectory");
	}
	cls = jniEnv->GetObjectClass(storageDirectory);
	if (cls == 0) {
		throw JniException("Cant find class for storageDirectory object");
	}
	mid = jniEnv->GetMethodID(cls, "getAbsolutePath", "()Ljava/lang/String;");
	if (mid == 0) {
		throw JniException("Cant find getAbsolutePath method");
	}
	storageDirectoryName = (jstring)jniEnv->CallObjectMethod(storageDirectory, mid);
	if (storageDirectoryName == 0) {
		throw JniException("Cant get storageDirectoryName");
	}
	const char *str = jniEnv->GetStringUTFChars(storageDirectoryName, NULL);
	if (str == NULL) {
		throw JniException("Cant convert storageDirectoryName to C format");
	}
	std::string rslt(str);
	jniEnv->ReleaseStringUTFChars(storageDirectoryName, str);
	return rslt;
}

} // namespace openmsx

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void * /*reserved*/)
{
	// Store the reference to the JVM so that the JNI calls can use it
	jniVM = vm;
	return JNI_VERSION_1_2;
};

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM * /*vm*/, void * /*reserved*/)
{
	// Nothing to do
};

#endif
