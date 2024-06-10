#ifndef SERIALIZE_CORE_HH
#define SERIALIZE_CORE_HH

#include "serialize_constr.hh"
#include "serialize_meta.hh"
#include "one_of.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <array>
#include <cassert>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

namespace openmsx {

// Is there a specialized way to serialize 'T'.
template<typename T> struct Serializer : std::false_type {
	struct Saver {};
	struct Loader {};
};

// Type-queries for serialization framework
// is_primitive<T>
template<typename T> struct is_primitive           : std::false_type {};
template<> struct is_primitive<bool>               : std::true_type {};
template<> struct is_primitive<char>               : std::true_type {};
template<> struct is_primitive<signed char>        : std::true_type {};
template<> struct is_primitive<signed short>       : std::true_type {};
template<> struct is_primitive<signed int>         : std::true_type {};
template<> struct is_primitive<signed long>        : std::true_type {};
template<> struct is_primitive<unsigned char>      : std::true_type {};
template<> struct is_primitive<unsigned short>     : std::true_type {};
template<> struct is_primitive<unsigned int>       : std::true_type {};
template<> struct is_primitive<unsigned long>      : std::true_type {};
template<> struct is_primitive<float>              : std::true_type {};
template<> struct is_primitive<double>             : std::true_type {};
template<> struct is_primitive<long double>        : std::true_type {};
template<> struct is_primitive<long long>          : std::true_type {};
template<> struct is_primitive<unsigned long long> : std::true_type {};
template<> struct is_primitive<std::string>        : std::true_type {};


// Normally to make a class serializable, you have to implement a serialize()
// method on the class. For some classes we cannot extend the source code. So
// we need an alternative, non-intrusive, way to make those classes
// serializable.
template<typename Archive, typename T>
void serialize(Archive& ar, T& t, unsigned version)
{
	// By default use the serialize() member. But this function can
	// be overloaded to serialize classes in a non-intrusive way.
	t.serialize(ar, version);
}

template<typename Archive, typename T1, typename T2>
void serialize(Archive& ar, std::pair<T1, T2>& p, unsigned /*version*/)
{
	ar.serialize("first",  p.first,
	             "second", p.second);
}
template<typename T1, typename T2> struct SerializeClassVersion<std::pair<T1, T2>>
{
	static constexpr unsigned value = 0;
};

template<typename Archive, typename T>
void serialize(Archive& ar, std::optional<T>& o, unsigned /*version*/)
{
	bool hasValue = o.has_value();
	ar.attribute("hasValue", hasValue);
	if (hasValue) {
		if (o.has_value()) {
			ar.serialize("value", *o);
		} else {
			T value{};
			ar.serialize("value", value);
			o = std::move(value);
		}
	} else {
		o.reset();
	}
}
template<typename T> struct SerializeClassVersion<std::optional<T>>
{
	static constexpr unsigned value = 0;
};

///////////

/** serialize_as_enum<T>
 *
 * For serialization of enums to work you have to specialize the
 * serialize_as_enum struct for that specific enum. This has a double purpose:
 *  - let the framework know this type should be treated as an enum
 *  - define a mapping between enum values and string representations
 *
 * The serialize_as_enum class has the following members:
 *  - static bool value
 *      True iff this type must be serialized as an enum.
 *      The fields below are only valid (even only present) if this variable
 *      is true.
 *  - std::string toString(T t)
 *      convert enum value to string
 *  - T fromString(const std::string& str)
 *      convert from string back to enum value
 *
 * If the enum has all consecutive values, starting from zero (as is the case
 * if you don't explicitly mention the numeric values in the enum definition),
 * you can use the SERIALIZE_ENUM macro as a convenient way to define a
 * specialization of serialize_as_enum:

 * example:
 *    enum MyEnum { FOO, BAR };
 *    std::initializer_list<enum_string<MyEnum>> myEnumInfo = {
 *          { "FOO", FOO },
 *          { "BAR", BAR },
 *    };
 *    SERIALIZE_ENUM(MyEnum, myEnumInfo);
 *
 * Note: when an enum definition evolves it has impact on the serialization,
 *   more specific on de-serialization of older version of the enum: adding
 *   values or changing the numerical value is no problem. But be careful with
 *   removing a value or changing the string representation.
 */

template<typename T> struct serialize_as_enum : std::false_type {};

template<typename T> struct enum_string {
	const char* str;
	T e;
};
[[noreturn]] void enumError(std::string_view str);

template<typename T>
inline std::string toString(std::initializer_list<enum_string<T>> list, T t_)
{
	for (auto& [str, t] : list) {
		if (t == t_) return str;
	}
	assert(false);
	return "internal-error-unknown-enum-value";
}

template<typename T>
T fromString(std::initializer_list<enum_string<T>> list, std::string_view str_)
{
	for (auto& [str, t] : list) {
		if (str == str_) return t;
	}
	enumError(str_); // does not return (throws)
	return T(); // avoid warning
}

#define SERIALIZE_ENUM(TYPE,INFO) \
template<> struct serialize_as_enum< TYPE > : std::true_type { \
	serialize_as_enum() : info(INFO) {} \
	std::initializer_list<enum_string< TYPE >> info; \
};

template<typename Archive, typename T, typename SaveAction>
void saveEnum(std::initializer_list<enum_string<T>> list, T t, SaveAction save)
{
	if constexpr (Archive::TRANSLATE_ENUM_TO_STRING) {
		save(toString(list, t));
	} else {
		save(int(t));
	}
}

template<typename Archive, typename T, typename LoadAction>
void loadEnum(std::initializer_list<enum_string<T>> list, T& t, LoadAction load)
{
	if constexpr (Archive::TRANSLATE_ENUM_TO_STRING) {
		std::string str;
		load(str);
		t = fromString(list, str);
	} else {
		int i;
		load(i);
		t = T(i);
	}
}

///////////

template<size_t I, typename Variant>
struct DefaultConstructVariant {
	Variant operator()(size_t index) const {
		if constexpr (I == std::variant_size_v<Variant>) {
			UNREACHABLE;
		} else if (index == I) {
			return std::variant_alternative_t<I, Variant>{};
		} else {
			return DefaultConstructVariant<I + 1, Variant>{}(index);
		}
	}
};
template<typename Variant>
Variant defaultConstructVariant(size_t index)
{
	return DefaultConstructVariant<0, Variant>{}(index);
}

template<typename V> struct VariantSerializer : std::true_type
{
	template<typename A>
	static constexpr size_t index = get_index<A, V>::value;

	struct Saver {
		template<typename Archive>
		void operator()(Archive& ar, const V& v, bool saveId) const {
			saveEnum<Archive>(Serializer<V>::list, v.index(),
				[&](const auto& t) { ar.attribute("type", t); });
			std::visit([&]<typename T>(T& e) {
				using TNC = std::remove_cvref_t<T>;
				auto& e2 = const_cast<TNC&>(e);
				ClassSaver<TNC> saver;
				saver(ar, e2, saveId);
			}, v);
		}
	};
	struct Loader {
		template<typename Archive, typename TUPLE>
		void operator()(Archive& ar, V& v, TUPLE args, int id) const {
			size_t idx;
			loadEnum<Archive>(Serializer<V>::list, idx,
				[&](auto& l) { ar.attribute("type", l); });
			v = defaultConstructVariant<V>(idx);
			std::visit([&]<typename T>(T& e) {
				ClassLoader<T> loader;
				loader(ar, e, args, id);
			}, v);
		}
	};
};



/////////////

// serialize_as_pointer<T>
//
// Type-trait class that indicates whether a certain type should be serialized
// as a pointer or not. There can be multiple pointers to the same object,
// only the first such pointer is actually stored, the others are stored as
// a reference to this first object.
//
// By default all pointer types are treated as pointer, but also smart pointer
// can be treated as such. Though only unique_ptr<T> is implemented ATM.
//
// The serialize_as_pointer class has the following members:
//  - static bool value
//      True iff this type must be serialized as a pointer.
//      The fields below are only valid (even only present) if this variable
//      is true.
//  - using type = T
//      The pointed-to type
//  - T* getPointer(X x)
//      Get an actual pointer from the abstract pointer x
//      (X can be a smart pointer type)
//  - void setPointer(X x, T* p, Archive& ar)
//      Copy the raw-pointer p to the abstract pointer x
//      The archive can be used to store per-archive data, this is for example
//      needed for shared_ptr.

template<typename T> struct serialize_as_pointer : std::false_type {};
template<typename T> struct serialize_as_pointer_impl : std::true_type
{
	// pointer to primitive types not supported
	static_assert(!is_primitive<T>::value,
	              "can't serialize ptr to primitive type");
	using type = T;
};
template<typename T> struct serialize_as_pointer<T*>
	: serialize_as_pointer_impl<T>
{
	static T* getPointer(T* t) { return t; }
	template<typename Archive>
	static void setPointer(T*& t, T* p, Archive& /*ar*/) {
		t = p;
	}
};
template<typename T> struct serialize_as_pointer<std::unique_ptr<T>>
	: serialize_as_pointer_impl<T>
{
	static T* getPointer(const std::unique_ptr<T>& t) { return t.get(); }
	template<typename Archive>
	static void setPointer(std::unique_ptr<T>& t, T* p, Archive& /*ar*/) {
		t.reset(p);
	}
};
template<typename T> struct serialize_as_pointer<std::shared_ptr<T>>
	: serialize_as_pointer_impl<T>
{
	static T* getPointer(const std::shared_ptr<T>& t) { return t.get(); }
	template<typename Archive>
	static void setPointer(std::shared_ptr<T>& t, T* p, Archive& ar) {
		ar.resetSharedPtr(t, p);
	}
};

///////////

// serialize_as_collection<T>
//
// Type-trait class that indicates whether a certain type should be serialized
// as a collection or not. The serialization code 'knows' how to serialize
// collections, so as a user of the serializer you only need to list the
// collection to have it serialized (you don't have to iterate over it
// manually).
//
// By default arrays, std::vector, std::list, std::deque and std::map are
// recognized as collections. Though for STL collections you need to add
//    #include "serialize_stl.hh"
//
// The serialize_as_collection class has the following members:
//
//  - static bool value
//      True iff this type must be serialized as a collection.
//      The fields below are only valid (even only present) if this variable
//      is true.
//  - int size
//      The size of the collection, -1 for variable sized collections.
//      Fixed sized collections can be serialized slightly more efficient
//      because we don't need to explicitly store the size.
//  - using value_type = ...
//      The type stored in the collection (only homogeneous collections are
//      supported).
//  - bool loadInPlace
//      Indicates whether we can directly load the elements in the correct
//      position in the container, otherwise there will be an extra assignment.
//      For this to be possible, the output iterator must support a dereference
//      operator that returns a 'regular' value_type.
//  - using const_iterator = ...
//  - const_iterator begin(...)
//  - const_iterator end(...)
//      Returns begin/end iterator for the given collection. Used for saving.
//  - void prepare(..., int n)
//  - output_iterator output(...)
//      These are used for loading. The prepare() method should prepare the
//      collection to receive 'n' elements. The output() method returns an
//      output_iterator to the beginning of the collection.

template<typename T> struct serialize_as_collection : std::false_type {};

template<typename T, size_t N> struct serialize_as_collection<std::array<T, N>> : std::true_type
{
	static constexpr int size = N; // fixed size
	using value_type = T;
	// save
	using const_iterator = typename std::array<T, N>::const_iterator;
	static auto begin(const std::array<T, N>& a) { return a.begin(); }
	static auto end  (const std::array<T, N>& a) { return a.end(); }
	// load
	static constexpr bool loadInPlace = true;
	static void prepare(std::array<T, N>& /*a*/, int /*n*/) { }
	static T* output(std::array<T, N>& a) { return a.data(); }
};

///////////

// Implementation of the different save-strategies.
//
// ATM we have
//  - PrimitiveSaver
//      Saves primitive values: int, bool, string, ...
//      Primitive values cannot be versioned.
//  - EnumSaver
//      Depending on the archive type, enums are either saved as strings (XML
//      archive) or as integers (memory archive).
//      This does not work automatically: it needs a specialization of
//      serialize_as_enum, see above.
//  - ClassSaver
//      From a serialization POV classes are a (heterogeneous) collection of
//      other to-be-serialized items.
//      Classes can have a version number, this allows to evolve the class
//      structure while still being able to load older versions (the load
//      method receive the version number as parameter, the user still needs
//      to keep the old loading code in place).
//      Optionally the name of the (concrete) class is saved in the stream.
//      This is used to support loading of polymorphic classes.
//      There is also an (optional) id saved in the stream. This used to
//      resolve possible multiple pointers to the same class.
//  - PointerSaver
//      Saves a pointer to a class (pointers to primitive types are not
//      supported). See also serialize_as_pointer
//  - IDSaver
//      Weaker version of PointerSaver: it can only save pointers to objects
//      that are already saved before (so it's will be saved by storing a
//      reference). To only reason to use IDSaver (iso PointerSaver) is that
//      it will not instantiate the object construction code.
//  - CollectionSaver
//      Saves a whole collection. See also serialize_as_collection
//
// All these strategies have a method:
//   template<typename Archive> void operator()(Archive& ar, const T& t)
//     'ar' is archive where the serialized stream will go
//     't'  is the to-be-saved object
//     'saveId' Should ID be saved

template<typename T> struct PrimitiveSaver
{
	template<typename Archive> void operator()(Archive& ar, const T& t,
	                                           bool /*saveId*/) const
	{
		static_assert(is_primitive<T>::value, "must be primitive type");
		ar.save(t);
	}
};
template<typename T> struct EnumSaver
{
	template<typename Archive> void operator()(Archive& ar, const T& t,
	                                           bool /*saveId*/) const
	{
		serialize_as_enum<T> sae;
		saveEnum<Archive>(sae.info, t,
			[&](const auto& s) { ar.save(s); });
	}
};
template<typename T> struct ClassSaver
{
	template<typename Archive> void operator()(
		Archive& ar, const T& t, bool saveId,
		const char* type = nullptr, bool saveConstrArgs = false) const
	{
		// Order is important (for non-xml archives). We use this order:
		//    - id
		//    - type
		//    - version
		//    - constructor args
		// Rational:
		//  - 'id' must be first: it could be nullptr, in that
		//    case the other fields are not even present.
		//  - 'type' must be before version, because for some types we
		//    might not need to store version (though it's unlikely)
		//  - version must be before constructor args because the
		//    constr args depend on the version
		if (saveId) {
			unsigned id = ar.generateId(&t);
			ar.attribute("id", id);
		}

		if (type) {
			ar.attribute("type", type);
		}

		unsigned version = SerializeClassVersion<T>::value;
		if ((version != 0) && ar.NEED_VERSION) {
			if (!ar.CAN_HAVE_OPTIONAL_ATTRIBUTES ||
			    (version != 1)) {
				ar.attribute("version", version);
			}
		}

		if (saveConstrArgs) {
			// save local constructor args (if any)
			SerializeConstructorArgs<T> constrArgs;
			constrArgs.save(ar, t);
		}

		using TNC = std::remove_const_t<T>;
		auto& t2 = const_cast<TNC&>(t);
		serialize(ar, t2, version);
	}
};
template<typename TP> struct PointerSaver
{
	// note: we only support pointer to class
	template<typename Archive> void operator()(Archive& ar, const TP& tp2,
	                                           bool /*saveId*/) const
	{
		static_assert(serialize_as_pointer<TP>::value,
		              "must be serialized as pointer");
		using T = typename serialize_as_pointer<TP>::type;
		const T* tp = serialize_as_pointer<TP>::getPointer(tp2);
		if (!tp) {
			unsigned id = 0;
			ar.attribute("id_ref", id);
			return;
		}
		if (unsigned id = ar.getId(tp)) {
			ar.attribute("id_ref", id);
		} else {
			if constexpr (std::is_polymorphic_v<T>) {
				PolymorphicSaverRegistry<Archive>::save(ar, tp);
			} else {
				ClassSaver<T> saver;
				// don't store type
				// store id, constr-args
				saver(ar, *tp, true, nullptr, true);
			}
		}
	}
};
template<typename TP> struct IDSaver
{
	template<typename Archive> void operator()(Archive& ar, const TP& tp2) const
	{
		static_assert(serialize_as_pointer<TP>::value,
		              "must be serialized as pointer");
		auto tp = serialize_as_pointer<TP>::getPointer(tp2);
		unsigned id;
		if (!tp) {
			id = 0;
		} else {
			id = ar.getId(tp);
			assert(id);
		}
		ar.attribute("id_ref", id);
	}
};
template<typename TC> struct CollectionSaver
{
	template<typename Archive> void operator()(Archive& ar, const TC& tc,
	                                           bool saveId) const
	{
		using sac = serialize_as_collection<TC>;
		static_assert(sac::value, "must be serialized as collection");
		auto begin = sac::begin(tc);
		auto end   = sac::end  (tc);
		if ((sac::size < 0) && (!ar.CAN_COUNT_CHILDREN)) {
			// variable size
			// e.g. in an XML archive the loader can look-ahead and
			// count the number of sub-tags, so no need to
			// explicitly store the size for such archives.
			int n = int(std::distance(begin, end));
			ar.serialize("size", n);
		}
		for (/**/; begin != end; ++begin) {
			if (saveId) {
				ar.serializeWithID("item", *begin);
			} else {
				ar.serialize("item", *begin);
			}
		}
	}
};

// Delegate to a specific Saver class
// (implemented as inheriting from a specific base class).
template<typename T> struct Saver
	: std::conditional_t<is_primitive<T>::value,            PrimitiveSaver<T>,
	  std::conditional_t<Serializer<T>::value,              typename Serializer<T>::Saver,
	  std::conditional_t<serialize_as_enum<T>::value,       EnumSaver<T>,
	  std::conditional_t<serialize_as_pointer<T>::value,    PointerSaver<T>,
	  std::conditional_t<serialize_as_collection<T>::value, CollectionSaver<T>,
	                                                        ClassSaver<T>>>>>> {};

////

// Implementation of the different load-strategies.
//
// This matches very closely with the save-strategies above.
//
// All these strategies have a method:
//   template<typename Archive, typename TUPLE>
//   void operator()(Archive& ar, const T& t, TUPLE args)
//     'ar' Is archive where the serialized stream will go
//     't'  Is the object that has to be restored.
//          In case of a class (not a pointer to a class) the actual object
//          is already constructed, but it still needs to be filled in with
//          the correct data.
//     'args' (Only used by PointerLoader) holds extra parameters used
//            to construct objects.
//     'id' Used to skip loading an ID, see comment in ClassLoader

template<typename T> struct PrimitiveLoader
{
	template<typename Archive, typename TUPLE>
	void operator()(Archive& ar, T& t, TUPLE /*args*/, int /*id*/) const
	{
		static_assert(std::tuple_size_v<TUPLE> == 0,
		              "can't have constructor arguments");
		ar.load(t);
	}
};
template<typename T> struct EnumLoader
{
	template<typename Archive, typename TUPLE>
	void operator()(Archive& ar, T& t, TUPLE /*args*/, int /*id*/) const
	{
		static_assert(std::tuple_size_v<TUPLE> == 0,
		              "can't have constructor arguments");
		serialize_as_enum<T> sae;
		loadEnum<Archive>(sae.info, t, [&](auto& l) { ar.load(l); });
	}
};

unsigned loadVersionHelper(MemInputArchive& ar, const char* className,
                           unsigned latestVersion);
unsigned loadVersionHelper(XmlInputArchive& ar, const char* className,
                           unsigned latestVersion);
template<typename T, typename Archive> unsigned loadVersion(Archive& ar)
{
	unsigned latestVersion = SerializeClassVersion<T>::value;
	if ((latestVersion != 0) && ar.NEED_VERSION) {
		return loadVersionHelper(ar, typeid(T).name(), latestVersion);
	} else {
		return latestVersion;
	}
}
template<typename T> struct ClassLoader
{
	template<typename Archive, typename TUPLE>
	void operator()(Archive& ar, T& t, TUPLE /*args*/, int id = 0,
	                int version = -1) const
	{
		static_assert(std::tuple_size_v<TUPLE> == 0,
		              "can't have constructor arguments");

		// id == -1: don't load id, don't addPointer
		// id ==  0: load id from archive, addPointer
		// id ==  N: id already loaded, still addPointer
		if (id != -1) {
			if (id == 0) {
				ar.attribute("id", id);
			}
			ar.addPointer(id, &t);
		}

		// version == -1: load version
		// version ==  N: version already loaded
		if (version == -1) {
			version = loadVersion<T>(ar);
		}

		using TNC = std::remove_const_t<T>;
		auto& t2 = const_cast<TNC&>(t);
		serialize(ar, t2, version);
	}
};
template<typename T> struct NonPolymorphicPointerLoader
{
	template<typename Archive, typename GlobalTuple>
	T* operator()(Archive& ar, unsigned id, GlobalTuple globalArgs)
	{
		int version = loadVersion<T>(ar);

		// load (local) constructor args (if any)
		using TNC = std::remove_const_t<T>;
		using ConstrArgs = SerializeConstructorArgs<TNC>;
		ConstrArgs constrArgs;
		auto localArgs = constrArgs.load(ar, version);

		// combine global and local constr args
		auto args = std::tuple_cat(globalArgs, localArgs);
		// TODO make combining global/local constr args configurable

		Creator<T> creator;
		auto tp = creator(args);
		ClassLoader<T> loader;
		loader(ar, *tp, std::tuple<>(), id, version);
		return tp.release();
	}
};
template<typename T> struct PolymorphicPointerLoader
{
	template<typename Archive, typename TUPLE>
	T* operator()(Archive& ar, unsigned id, TUPLE args)
	{
		using ArgsType = typename PolymorphicConstructorArgs<T>::type;
		static_assert(std::is_same_v<TUPLE, ArgsType>,
		              "constructor arguments types must match");
		return static_cast<T*>(
			PolymorphicLoaderRegistry<Archive>::load(ar, id, &args));
	}
};
template<typename T> struct PointerLoader2
	// extra indirection needed because inlining the body of
	// NonPolymorphicPointerLoader in PointerLoader does not compile
	// for abstract types
	: std::conditional_t<std::is_polymorphic_v<T>,
	                     PolymorphicPointerLoader<T>,
	                     NonPolymorphicPointerLoader<T>> {};

template<typename TP> struct PointerLoader
{
	template<typename Archive, typename GlobalTuple>
	void operator()(Archive& ar, TP& tp2, GlobalTuple globalArgs, int /*id*/) const
	{
		static_assert(serialize_as_pointer<TP>::value,
		              "must be serialized as a pointer");
		// in XML archives we use 'id_ref' or 'id', in other archives
		// we don't care about the name
		unsigned id = [&] {
			if constexpr (Archive::CAN_HAVE_OPTIONAL_ATTRIBUTES) {
				if (auto i = ar.template findAttributeAs<unsigned>("id_ref")) {
					return *i;
				}
			}
			unsigned i;
			ar.attribute("id", i);
			return i;
		}();

		using T = typename serialize_as_pointer<TP>::type;
		T* tp = [&]() -> T* {
			if (id == 0) {
				return nullptr;
			} else {
				if (void* p = ar.getPointer(id)) {
					return static_cast<T*>(p);
				} else {
					PointerLoader2<T> loader;
					return loader(ar, id, globalArgs);
				}
			}
		}();
		serialize_as_pointer<TP>::setPointer(tp2, tp, ar);
	}
};
[[noreturn]] void pointerError(unsigned id);
template<typename TP> struct IDLoader
{
	template<typename Archive>
	void operator()(Archive& ar, TP& tp2) const
	{
		static_assert(serialize_as_pointer<TP>::value,
		              "must be serialized as a pointer");
		unsigned id;
		ar.attribute("id_ref", id);

		using T = typename serialize_as_pointer<TP>::type;
		T* tp;
		if (id == 0) {
			tp = nullptr;
		} else {
			void* p = ar.getPointer(id);
			if (!p) {
				pointerError(id);
			}
			tp = static_cast<T*>(p);
		}
		serialize_as_pointer<TP>::setPointer(tp2, tp, ar);
	}
};

template<typename sac, bool IN_PLACE = sac::loadInPlace> struct CollectionLoaderHelper;
template<typename sac> struct CollectionLoaderHelper<sac, true>
{
	// used for array and vector
	template<typename Archive, typename TUPLE, typename OUT_ITER>
	void operator()(Archive& ar, TUPLE args, OUT_ITER it, int id) const
	{
		ar.doSerialize("item", *it, args, id);
	}
};
template<typename sac> struct CollectionLoaderHelper<sac, false>
{
	// We can't directly load the element in the correct position:
	// This screws-up id/pointer management because the element is still
	// copied after construction (and pointer value of initial object is
	// stored).
	template<typename Archive, typename TUPLE, typename OUT_ITER>
	void operator()(Archive& ar, TUPLE args, OUT_ITER it, int id) const
	{
		typename sac::value_type elem;
		ar.doSerialize("item", elem, args, id);
		*it = std::move(elem);
	}
};
template<typename TC> struct CollectionLoader
{
	template<typename Archive, typename TUPLE>
	void operator()(Archive& ar, TC& tc, TUPLE args, int id = 0) const
	{
		assert(id == one_of(0, -1));
		using sac = serialize_as_collection<TC>;
		static_assert(sac::value, "must be serialized as a collection");
		int n = sac::size;
		if (n < 0) {
			// variable size
			if constexpr (Archive::CAN_COUNT_CHILDREN) {
				n = ar.countChildren();
			} else {
				ar.serialize("size", n);
			}
		}
		sac::prepare(tc, n);
		auto it = sac::output(tc);
		CollectionLoaderHelper<sac> loadOneElement;
		repeat(n, [&] {
			loadOneElement(ar, args, it, id);
			++it;
		});
	}
};
template<typename T> struct Loader
	: std::conditional_t<is_primitive<T>::value,            PrimitiveLoader<T>,
	  std::conditional_t<Serializer<T>::value,              typename Serializer<T>::Loader,
	  std::conditional_t<serialize_as_enum<T>::value,       EnumLoader<T>,
	  std::conditional_t<serialize_as_pointer<T>::value,    PointerLoader<T>,
	  std::conditional_t<serialize_as_collection<T>::value, CollectionLoader<T>,
	                                                        ClassLoader<T>>>>>> {};

} // namespace openmsx

#endif
