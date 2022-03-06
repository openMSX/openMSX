#include "serialize_meta.hh"
#include "serialize.hh"
#include "MSXException.hh"
#include "ranges.hh"
#include "stl.hh"
#include <cassert>
#include <iostream>

namespace openmsx {

template<typename Archive>
PolymorphicSaverRegistry<Archive>& PolymorphicSaverRegistry<Archive>::instance()
{
	static PolymorphicSaverRegistry oneInstance;
	return oneInstance;
}

template<typename Archive>
void PolymorphicSaverRegistry<Archive>::registerHelper(
	const std::type_info& type, SaveFunction saver)
{
	assert(!initialized);
	assert(!contains(saverMap, type, &Entry::index)); // not yet sorted
	saverMap.emplace_back(Entry{type, std::move(saver)});
}

template<typename Archive>
void PolymorphicSaverRegistry<Archive>::save(
	Archive& ar, const void* t, const std::type_info& typeInfo)
{
	auto& reg = PolymorphicSaverRegistry<Archive>::instance();
	if (!reg.initialized) [[unlikely]] {
		reg.initialized = true;
		ranges::sort(reg.saverMap, {}, &Entry::index);
	}
	auto it = ranges::lower_bound(reg.saverMap, typeInfo, {}, &Entry::index);
	if ((it == end(reg.saverMap)) || (it->index != typeInfo)) {
		std::cerr << "Trying to save an unregistered polymorphic type: "
			  << typeInfo.name() << '\n';
		assert(false); return;
	}
	it->saver(ar, t);
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
PolymorphicLoaderRegistry<Archive>& PolymorphicLoaderRegistry<Archive>::instance()
{
	static PolymorphicLoaderRegistry oneInstance;
	return oneInstance;
}

template<typename Archive>
void PolymorphicLoaderRegistry<Archive>::registerHelper(
	const char* name, LoadFunction loader)
{
	assert(!loaderMap.contains(name));
	loaderMap.emplace_noDuplicateCheck(name, std::move(loader));
}

template<typename Archive>
void* PolymorphicLoaderRegistry<Archive>::load(
	Archive& ar, unsigned id, const void* args)
{
	std::string type;
	ar.attribute("type", type);
	auto& reg = PolymorphicLoaderRegistry<Archive>::instance();
	auto v = lookup(reg.loaderMap, type);
	assert(v);
	return (*v)(ar, id, args);
}

template class PolymorphicLoaderRegistry<MemInputArchive>;
template class PolymorphicLoaderRegistry<XmlInputArchive>;

////

void polyInitError(const char* expected, const char* actual)
{
	throw MSXException("Expected type: ", expected, " but got: ", actual, '.');
}

template<typename Archive>
PolymorphicInitializerRegistry<Archive>& PolymorphicInitializerRegistry<Archive>::instance()
{
	static PolymorphicInitializerRegistry oneInstance;
	return oneInstance;
}

template<typename Archive>
void PolymorphicInitializerRegistry<Archive>::registerHelper(
	const char* name, InitFunction initializer)
{
	assert(!initializerMap.contains(name));
	initializerMap.emplace_noDuplicateCheck(name, std::move(initializer));
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

	auto& reg = PolymorphicInitializerRegistry<Archive>::instance();
	auto v = lookup(reg.initializerMap, type);
	if (!v) {
		throw MSXException("Deserialize unknown polymorphic type: '", type, "'.");
	}
	(*v)(ar, t, id);

	ar.endTag(tag);
}

template class PolymorphicInitializerRegistry<MemInputArchive>;
template class PolymorphicInitializerRegistry<XmlInputArchive>;

} // namespace openmsx
