#ifndef ANDROIDAPIWRAPPER_HH

#include "build-info.hh"
#if PLATFORM_ANDROID
#include "MSXException.hh"

namespace openmsx {

class AndroidApiWrapper
{
public:
	static std::string getStorageDirectory();
};

class JniException final : public MSXException
{
public:
	explicit JniException(string_view message)
		: MSXException(message) {}
};

} // namespace openmsx
#endif // #if PLATFORM_ANDROID

#endif
