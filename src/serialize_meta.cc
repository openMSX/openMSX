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

template class PolymorphicSaverRegistry<XmlInputArchive>;
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

template class PolymorphicLoaderRegistry<XmlInputArchive>;
template class PolymorphicLoaderRegistry<XmlOutputArchive>;

} // namespace openmsx
