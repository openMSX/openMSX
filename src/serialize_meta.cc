// $Id$

#include "serialize_meta.hh"
#include <cassert>
#include <iostream>

namespace openmsx {

template<typename Archive>
PolymorphicSaverRegistry<Archive>::PolymorphicSaverRegistry()
{
}

template<typename Archive>
PolymorphicSaverRegistry<Archive>::~PolymorphicSaverRegistry()
{
	for (typename SaverMap::const_iterator it = saverMap.begin();
	     it != saverMap.end(); ++it) {
		delete it->second;
	}
}

template<typename Archive>
PolymorphicSaverRegistry<Archive>& PolymorphicSaverRegistry<Archive>::instance()
{
	static PolymorphicSaverRegistry oneInstance;
	return oneInstance;
}

template<typename Archive>
const PolymorphicSaverBase<Archive>&
PolymorphicSaverRegistry<Archive>::getSaver(TypeInfo typeInfo)
{
	typename SaverMap::const_iterator it = saverMap.find(typeInfo);
	if (it == saverMap.end()) {
		std::cerr << "Trying to save an unregistered polymorphic type: "
			  << typeInfo.name() << std::endl;
		assert(false);
	}
	return *it->second;
}

template class PolymorphicSaverRegistry<MemOutputArchive>;
template class PolymorphicSaverRegistry<XmlOutputArchive>;

////

template<typename Archive>
PolymorphicLoaderRegistry<Archive>::PolymorphicLoaderRegistry()
{
}

template<typename Archive>
PolymorphicLoaderRegistry<Archive>::~PolymorphicLoaderRegistry()
{
	for (typename LoaderMap::const_iterator it = loaderMap.begin();
	     it != loaderMap.end(); ++it) {
		delete it->second;
	}
}

template<typename Archive>
PolymorphicLoaderRegistry<Archive>& PolymorphicLoaderRegistry<Archive>::instance()
{
	static PolymorphicLoaderRegistry oneInstance;
	return oneInstance;
}

template<typename Archive>
const PolymorphicLoaderBase<Archive>&
PolymorphicLoaderRegistry<Archive>::getLoader(const std::string& type)
{
	typename LoaderMap::const_iterator it = loaderMap.find(type);
	assert(it != loaderMap.end());
	return *it->second;
}

template class PolymorphicLoaderRegistry<MemInputArchive>;
template class PolymorphicLoaderRegistry<XmlInputArchive>;

////

template<typename Archive>
PolymorphicInitializerRegistry<Archive>::PolymorphicInitializerRegistry()
{
}

template<typename Archive>
PolymorphicInitializerRegistry<Archive>::~PolymorphicInitializerRegistry()
{
	for (typename InitializerMap::const_iterator it = initializerMap.begin();
	     it != initializerMap.end(); ++it) {
		delete it->second;
	}
}

template<typename Archive>
PolymorphicInitializerRegistry<Archive>& PolymorphicInitializerRegistry<Archive>::instance()
{
	static PolymorphicInitializerRegistry oneInstance;
	return oneInstance;
}

template<typename Archive>
const PolymorphicInitializerBase<Archive>&
PolymorphicInitializerRegistry<Archive>::getInitializer(const std::string& type)
{
	typename InitializerMap::const_iterator it = initializerMap.find(type);
	assert(it != initializerMap.end());
	return *it->second;
}

template class PolymorphicInitializerRegistry<MemInputArchive>;
template class PolymorphicInitializerRegistry<XmlInputArchive>;

} // namespace openmsx
