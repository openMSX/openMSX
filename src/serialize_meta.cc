// $Id$

#include "serialize_meta.hh"
#include "serialize.hh"
#include "MSXException.hh"
#include "StringOp.hh"
#include "unreachable.hh"
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
void PolymorphicSaverRegistry<Archive>::registerHelper(
	const std::type_info& type, PolymorphicSaverBase<Archive>* saver)
{
	assert(saverMap.find(type) == saverMap.end());
	saverMap[type] = saver;
}

template<typename Archive>
void PolymorphicSaverRegistry<Archive>::save(
	Archive& ar, const void* t, const std::type_info& typeInfo)
{
	PolymorphicSaverRegistry<Archive>& reg =
		PolymorphicSaverRegistry<Archive>::instance();
	typename SaverMap::const_iterator it = reg.saverMap.find(typeInfo);
	if (it == reg.saverMap.end()) {
		std::cerr << "Trying to save an unregistered polymorphic type: "
			  << typeInfo.name() << std::endl;
		UNREACHABLE;
	}
	it->second->save(ar, t);
}
template<typename Archive>
void PolymorphicSaverRegistry<Archive>::save(
	const char* tag, Archive& ar, const void* t, const std::type_info& typeInfo)
{
	ar.beginTag(tag);
	save(ar, t, typeInfo);
	ar.endTag(tag);
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
void PolymorphicLoaderRegistry<Archive>::registerHelper(
	const char* name_, PolymorphicLoaderBase<Archive>* loader)
{
	std::string name(name_);
	assert(loaderMap.find(name) == loaderMap.end());
	loaderMap[name] = loader;
}

template<typename Archive>
void* PolymorphicLoaderRegistry<Archive>::load(
	Archive& ar, unsigned id, TupleBase& args)
{
	std::string type;
	ar.attribute("type", type);
	PolymorphicLoaderRegistry<Archive>& reg =
		PolymorphicLoaderRegistry<Archive>::instance();
	typename LoaderMap::const_iterator it = reg.loaderMap.find(type);
	assert(it != reg.loaderMap.end());
	return it->second->load(ar, id, args);
}

template class PolymorphicLoaderRegistry<MemInputArchive>;
template class PolymorphicLoaderRegistry<XmlInputArchive>;

////

void polyInitError(const char* expected, const char* actual)
{
	throw MSXException(StringOp::Builder() <<
		"Expected type: " << expected << " but got: " << actual << '.');
}

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
void PolymorphicInitializerRegistry<Archive>::registerHelper(
	const char* name_, PolymorphicInitializerBase<Archive>* initializer)
{
	std::string name(name_);
	assert(initializerMap.find(name) == initializerMap.end());
	initializerMap[name] = initializer;
}

template<typename Archive>
void PolymorphicInitializerRegistry<Archive>::init(
	const char* tag, Archive& ar, void* t)
{
	ar.beginTag(tag);
	unsigned id;
	ar.attribute("id", id);
	assert(id);
	std::string type;
	ar.attribute("type", type);

	PolymorphicInitializerRegistry<Archive>& reg =
		PolymorphicInitializerRegistry<Archive>::instance();
	typename InitializerMap::const_iterator it = reg.initializerMap.find(type);
	assert(it != reg.initializerMap.end());
	it->second->init(ar, t, id);

	ar.endTag(tag);
}

template class PolymorphicInitializerRegistry<MemInputArchive>;
template class PolymorphicInitializerRegistry<XmlInputArchive>;

} // namespace openmsx
