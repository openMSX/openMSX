#ifndef SERIALIZE_META_HH
#define SERIALIZE_META_HH

#include "hash_map.hh"
#include "likely.hh"
#include "xxhash.hh"
#include <memory>
#include <tuple>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <vector>

namespace openmsx {

/** Utility to do     T* t = new T(...)
 *
 * The tricky part is that the constructor of T can have a variable number
 * of parameters.
 *
 * For example:
 *       Creator<Foo> creator;
 *       auto args = std::tuple(42, 3.14);
 *       std::unique_ptr<Foo> foo = creator(args);
 * This is equivalent to
 *       auto foo = std::make_unique<Foo>(42, 3.14);
 * But the former can be used in a generic context (where the number of
 * constructor parameters is unknown).
 */
template<typename T> struct Creator
{
	template<typename TUPLE>
	std::unique_ptr<T> operator()(TUPLE tuple) {
		auto makeT = [](auto&& ...args) {
			return std::make_unique<T>(std::forward<decltype(args)>(args)...);
		};
		return std::apply(makeT, tuple);
	}
};

///////////////////////////////

// Polymorphic class loader/saver

// forward declarations
// ClassSaver: used to actually save a class. We also store the name of
//   the class so that the loader knows which concrete class it should load.
template<typename T> struct ClassSaver;
// NonPolymorphicPointerLoader: once we know which concrete type to load,
//   we use the 'normal' class loader to load it.
template<typename T> struct NonPolymorphicPointerLoader;
// Used by PolymorphicInitializer to initialize a concrete type.
template<typename T> struct ClassLoader;

/** Store association between polymorphic class (base- or subclass) and
 *  the list of constructor arguments.
 * Specializations of this class should store the constructor arguments
 * as a 'using type = tuple<...>'.
 */
template<typename T> struct PolymorphicConstructorArgs;

/** Store association between (polymorphic) sub- and baseclass.
 * Specialization of this class should provide a 'using type = <base>'.
 */
template<typename T> struct PolymorphicBaseClass;

template<typename Base> struct MapConstrArgsEmpty
{
	using TUPLEIn = typename PolymorphicConstructorArgs<Base>::type;
	std::tuple<> operator()(const TUPLEIn& /*t*/)
	{
		return std::tuple<>();
	}
};
template<typename Base, typename Derived> struct MapConstrArgsCopy
{
	using TUPLEIn  = typename PolymorphicConstructorArgs<Base>::type;
	using TUPLEOut = typename PolymorphicConstructorArgs<Derived>::type;
	static_assert(std::is_same_v<TUPLEIn, TUPLEOut>,
	              "constructor argument types must match");
	TUPLEOut operator()(const TUPLEIn& t)
	{
		return t;
	}
};

/** Define mapping between constructor arg list of base- and subclass.
 *
 * When loading a polymorphic base class, the user must provide the union of
 * constructor arguments for all subclasses (because it's not yet known which
 * concrete subtype will be deserialized). This class defines the mapping
 * between this union of parameters and the subset used for a specific
 * subclass.
 *
 * In case the parameter list of the subclass is empty or if it is the same
 * as the base class, this mapping will be defined automatically. In the other
 * cases, the user must define a specialization of this class.
 */
template<typename Base, typename Derived> struct MapConstructorArguments
	: std::conditional_t<std::is_same<std::tuple<>,
	                     typename PolymorphicConstructorArgs<Derived>::type>::value,
	      MapConstrArgsEmpty<Base>,
	      MapConstrArgsCopy<Base, Derived>> {};

/** Stores the name of a base class.
 * This name is used as tag-name in XML archives.
 *
 * Specializations of this class should provide a function
 *   static const char* getName()
 */
template<typename Base> struct BaseClassName;

void polyInitError(const char* expected, const char* actual);

template<typename Archive>
class PolymorphicSaverRegistry
{
public:
	PolymorphicSaverRegistry(const PolymorphicSaverRegistry&) = delete;
	PolymorphicSaverRegistry& operator=(const PolymorphicSaverRegistry&) = delete;

	static PolymorphicSaverRegistry& instance();

	template<typename T> void registerClass(const char* name)
	{
		static_assert(std::is_polymorphic_v<T>,
		              "must be a polymorphic type");
		static_assert(!std::is_abstract_v<T>,
		              "can't be an abstract type");
		registerHelper(typeid(T), [name](Archive& ar, const void* v) {
			using BaseType = typename PolymorphicBaseClass<T>::type;
			auto base = static_cast<const BaseType*>(v);
			auto tp = static_cast<const T*>(base);
			ClassSaver<T> saver;
			saver(ar, *tp, true, name, true); // save id, type, constr-args
		});
	}

	template<typename T> static void save(Archive& ar, T* t)
	{
		save(ar, t, typeid(*t));
	}
	template<typename T> static void save(const char* tag, Archive& ar, T& t)
	{
		save(tag, ar, &t, typeid(t));
	}

private:
	PolymorphicSaverRegistry() = default;
	~PolymorphicSaverRegistry() = default;

	using SaveFunction = std::function<void(Archive&, const void*)>;
	void registerHelper(const std::type_info& type,
	                    SaveFunction saver);
	static void save(Archive& ar, const void* t,
	                 const std::type_info& typeInfo);
	static void save(const char* tag, Archive& ar, const void* t,
	                 const std::type_info& typeInfo);

	struct Entry {
		std::type_index index;
		SaveFunction saver;
	};
	std::vector<Entry> saverMap;
	bool initialized = false;
};

template<typename Archive>
class PolymorphicLoaderRegistry
{
public:
	PolymorphicLoaderRegistry(const PolymorphicLoaderRegistry&) = delete;
	PolymorphicLoaderRegistry& operator=(const PolymorphicLoaderRegistry&) = delete;

	static PolymorphicLoaderRegistry& instance();

	template<typename T> void registerClass(const char* name)
	{
		static_assert(std::is_polymorphic_v<T>,
		              "must be a polymorphic type");
		static_assert(!std::is_abstract_v<T>,
		              "can't be an abstract type");
		registerHelper(name, [](Archive& ar, unsigned id, const void* args) {
			using BaseType = typename PolymorphicBaseClass<T>::type;
			using TUPLEIn  = typename PolymorphicConstructorArgs<BaseType>::type;
			using TUPLEOut = typename PolymorphicConstructorArgs<T>::type;
			auto& argsIn = *static_cast<const TUPLEIn*>(args);
			MapConstructorArguments<BaseType, T> mapArgs;
			TUPLEOut argsOut = mapArgs(argsIn);
			NonPolymorphicPointerLoader<T> loader;
			return loader(ar, id, argsOut);
		});
	}

	static void* load(Archive& ar, unsigned id, const void* args);

private:
	PolymorphicLoaderRegistry() = default;
	~PolymorphicLoaderRegistry() = default;

	using LoadFunction = std::function<void*(Archive&, unsigned, const void*)>;
	void registerHelper(const char* name, LoadFunction loader);

	hash_map<std::string_view, LoadFunction, XXHasher> loaderMap;
};

template<typename Archive>
class PolymorphicInitializerRegistry
{
public:
	PolymorphicInitializerRegistry(const PolymorphicInitializerRegistry&) = delete;
	PolymorphicInitializerRegistry& operator=(const PolymorphicInitializerRegistry&) = delete;

	static PolymorphicInitializerRegistry& instance();

	template<typename T> void registerClass(const char* name)
	{
		static_assert(std::is_polymorphic_v<T>,
		              "must be a polymorphic type");
		static_assert(!std::is_abstract_v<T>,
		              "can't be an abstract type");
		registerHelper(name, [](Archive& ar, void* v, unsigned id) {
			using BaseType = typename PolymorphicBaseClass<T>::type;
			auto base = static_cast<BaseType*>(v);
			if (unlikely(dynamic_cast<T*>(base) != static_cast<T*>(base))) {
				polyInitError(typeid(T).name(), typeid(*base).name());
			}
			auto t = static_cast<T*>(base);
			ClassLoader<T> loader;
			loader(ar, *t, std::tuple<>(), id);
		});
	}

	static void init(const char* tag, Archive& ar, void* t);

private:
	PolymorphicInitializerRegistry() = default;
	~PolymorphicInitializerRegistry() = default;

	using InitFunction = std::function<void(Archive&, void*, unsigned)>;
	void registerHelper(const char* name, InitFunction initializer);

	hash_map<std::string_view, InitFunction, XXHasher> initializerMap;
};


template<typename Archive, typename T> struct RegisterSaverHelper
{
	explicit RegisterSaverHelper(const char* name)
	{
		PolymorphicSaverRegistry<Archive>::instance().
			template registerClass<T>(name);
	}
};
template<typename Archive, typename T> struct RegisterLoaderHelper
{
	explicit RegisterLoaderHelper(const char* name)
	{
		PolymorphicLoaderRegistry<Archive>::instance().
			template registerClass<T>(name);
	}
};
template<typename Archive, typename T> struct RegisterInitializerHelper
{
	explicit RegisterInitializerHelper(const char* name)
	{
		PolymorphicInitializerRegistry<Archive>::instance().
			template registerClass<T>(name);
	}
};

#define REGISTER_CONSTRUCTOR_ARGS_0(C) \
template<> struct PolymorphicConstructorArgs<C> \
{ using type = std::tuple<>; };

#define REGISTER_CONSTRUCTOR_ARGS_1(C,T1) \
template<> struct PolymorphicConstructorArgs<C> \
{ using type = std::tuple<T1>; };

#define REGISTER_CONSTRUCTOR_ARGS_2(C,T1,T2) \
template<> struct PolymorphicConstructorArgs<C> \
{ using type = std::tuple<T1,T2>; };

#define REGISTER_CONSTRUCTOR_ARGS_3(C,T1,T2,T3) \
template<> struct PolymorphicConstructorArgs<C> \
{ using type = std::tuple<T1,T2,T3>; };

class MemInputArchive;
class MemOutputArchive;
class XmlInputArchive;
class XmlOutputArchive;

/*#define REGISTER_POLYMORPHIC_CLASS_HELPER(B,C,N) \
static_assert(std::is_base_of_v<B,C>, "must be base and sub class"); \
static RegisterLoaderHelper<TextInputArchive,  C> registerHelper1##C(N); \
static RegisterSaverHelper <TextOutputArchive, C> registerHelper2##C(N); \
static RegisterLoaderHelper<XmlInputArchive,   C> registerHelper3##C(N); \
static RegisterSaverHelper <XmlOutputArchive,  C> registerHelper4##C(N); \
static RegisterLoaderHelper<MemInputArchive,   C> registerHelper5##C(N); \
static RegisterSaverHelper <MemOutputArchive,  C> registerHelper6##C(N); \*/
#define REGISTER_POLYMORPHIC_CLASS_HELPER(B,C,N) \
static_assert(std::is_base_of_v<B,C>, "must be base and sub class"); \
static RegisterLoaderHelper<MemInputArchive,  C> registerHelper3##C(N); \
static RegisterSaverHelper <MemOutputArchive, C> registerHelper4##C(N); \
static RegisterLoaderHelper<XmlInputArchive,  C> registerHelper5##C(N); \
static RegisterSaverHelper <XmlOutputArchive, C> registerHelper6##C(N); \
template<> struct PolymorphicBaseClass<C> { using type = B; };

#define REGISTER_POLYMORPHIC_INITIALIZER_HELPER(B,C,N) \
static_assert(std::is_base_of_v<B,C>, "must be base and sub class"); \
static RegisterInitializerHelper<MemInputArchive,  C> registerHelper3##C(N); \
static RegisterSaverHelper      <MemOutputArchive, C> registerHelper4##C(N); \
static RegisterInitializerHelper<XmlInputArchive,  C> registerHelper5##C(N); \
static RegisterSaverHelper      <XmlOutputArchive, C> registerHelper6##C(N); \
template<> struct PolymorphicBaseClass<C> { using type = B; };

#define REGISTER_BASE_NAME_HELPER(B,N) \
template<> struct BaseClassName<B> \
{ static const char* getName() { static constexpr const char* const name = N; return name; } };

// public macros
//   these are a more convenient way to define specializations of the
//   PolymorphicConstructorArgs and PolymorphicBaseClass classes
#define REGISTER_POLYMORPHIC_CLASS(BASE,CLASS,NAME) \
	REGISTER_POLYMORPHIC_CLASS_HELPER(BASE,CLASS,NAME) \
	REGISTER_CONSTRUCTOR_ARGS_0(CLASS)

#define REGISTER_POLYMORPHIC_CLASS_1(BASE,CLASS,NAME,TYPE1) \
	REGISTER_POLYMORPHIC_CLASS_HELPER(BASE,CLASS,NAME) \
	REGISTER_CONSTRUCTOR_ARGS_1(CLASS,TYPE1)

#define REGISTER_POLYMORPHIC_CLASS_2(BASE,CLASS,NAME,TYPE1,TYPE2) \
	REGISTER_POLYMORPHIC_CLASS_HELPER(BASE,CLASS,NAME) \
	REGISTER_CONSTRUCTOR_ARGS_2(CLASS,TYPE1,TYPE2)

#define REGISTER_POLYMORPHIC_CLASS_3(BASE,CLASS,NAME,TYPE1,TYPE2,TYPE3) \
	REGISTER_POLYMORPHIC_CLASS_HELPER(BASE,CLASS,NAME) \
	REGISTER_CONSTRUCTOR_ARGS_3(CLASS,TYPE1,TYPE2,TYPE3)

#define REGISTER_BASE_CLASS(CLASS,NAME) \
	REGISTER_BASE_NAME_HELPER(CLASS,NAME) \
	REGISTER_CONSTRUCTOR_ARGS_0(CLASS)

#define REGISTER_BASE_CLASS_1(CLASS,NAME,TYPE1) \
	REGISTER_BASE_NAME_HELPER(CLASS,NAME) \
	REGISTER_CONSTRUCTOR_ARGS_1(CLASS,TYPE1)

#define REGISTER_BASE_CLASS_2(CLASS,NAME,TYPE1,TYPE2) \
	REGISTER_BASE_NAME_HELPER(CLASS,NAME) \
	REGISTER_CONSTRUCTOR_ARGS_2(CLASS,TYPE1,TYPE2)

#define REGISTER_BASE_CLASS_3(CLASS,NAME,TYPE1,TYPE2,TYPE3) \
	REGISTER_BASE_NAME_HELPER(CLASS,NAME) \
	REGISTER_CONSTRUCTOR_ARGS_3(CLASS,TYPE1,TYPE2,TYPE3)


#define REGISTER_POLYMORPHIC_INITIALIZER(BASE,CLASS,NAME) \
	REGISTER_POLYMORPHIC_INITIALIZER_HELPER(BASE,CLASS,NAME)

//////////////

/** Store serialization-version number of a class.
 *
 * Classes are individually versioned. Use the SERIALIZE_CLASS_VERSION
 * macro below as a convenient way to set the version of a class.
 *
 * The initial (=default) version is 1. When the layout of a class changes
 * in an incompatible way, you should increase the version number. But
 * remember: to be able to load older version the (de)serialize code for the
 * older version(s) must be kept.
 *
 * Version number 0 is special. It means the layout for this class will _NEVER_
 * change. This can be a bit more efficient because the version number must
 * not be stored in the stream. Though be careful to only use version 0 for
 * _VERY_ stable classes, std::pair is a good example of a stable class.
 */
template<typename T> struct SerializeClassVersion
{
	static constexpr unsigned value = 1;
};
#define SERIALIZE_CLASS_VERSION(CLASS, VERSION) \
template<> struct SerializeClassVersion<CLASS> \
{ \
	static constexpr unsigned value = VERSION; \
};

} // namespace openmsx

#endif
