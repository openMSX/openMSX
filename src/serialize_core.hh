#ifndef SERIALIZE_CORE_HH
#define SERIALIZE_CORE_HH

#include "serialize_constr.hh"
#include "serialize_meta.hh"
#include "shared_ptr.hh"
#include "static_assert.hh"
#include "type_traits.hh"
#include <string>
#include <cassert>
#include <memory>

namespace openmsx {

// Normally to make a class serializable, you have to implement a serialize()
// method on the class. For some classes we cannot extend the source code. So
// we need an alternative, non-intrusive, way to make those classes
// serializable.
template <typename Archive, typename T>
void serialize(Archive& ar, T& t, unsigned version)
{
	// By default use the serialize() member. But this function can
	// be overloaded to serialize classes in a non-intrusive way.
	t.serialize(ar, version);
}

template <typename Archive, typename T1, typename T2>
void serialize(Archive& ar, std::pair<T1, T2>& p, unsigned /*version*/)
{
	ar.serialize("first",  p.first);
	ar.serialize("second", p.second);
}
template<typename T1, typename T2> struct SerializeClassVersion<std::pair<T1, T2> >
{
	static const unsigned value = 0;
};

///////////

/** serialize_as_enum<T>
 *
 * For serialization of enums to work you have to specialize the
 * serialize_as_enum struct for that specific enum. This has a double purpose:
 *  - let the framework know this type should be traited as an enum
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
 * if you don't explicity mention the numeric values in the enum definition),
 * you can use the SERIALIZE_ENUM macro as a convenient way to define a
 * specialization of serialize_as_enum:

 * example:
 *    enum MyEnum { FOO, BAR };
 *    enum_string<MyEnum> myEnumInfo[] = {
 *          { "FOO", FOO },
 *          { "BAR", BAR },
 *          { NULL, FOO } // dummy enum value
 *    };
 *    SERIALIZE_ENUM(MyEnum, myEnumInfo);
 *
 * Note: when an enum definition evolves it has impact on the serialization,
 *   more specific on de-serialization of older version of the enum: adding
 *   values or changing the numerical value is no problem. But be careful with
 *   removing a value or changing the string representation.
 */

template<typename T> struct serialize_as_enum : is_false {};

template<typename T> struct enum_string {
	const char* str;
	T e;
};
template<typename T> struct serialize_as_enum_impl : is_true {
	serialize_as_enum_impl(enum_string<T>* info_) : info(info_) {}
	std::string toString(T t) const {
		enum_string<T>* p = info;
		while (p->str) {
			if (p->e == t) return p->str;
			++p;
		}
		assert(false); return "";
	}
	T fromString(const std::string& str) const {
		enum_string<T>* p = info;
		while (p->str) {
			if (p->str == str) return p->e;
			++p;
		}
		assert(false); return T(0);
	}
private:
	enum_string<T>* info;
};

#define SERIALIZE_ENUM(TYPE,INFO) \
template<> struct serialize_as_enum< TYPE > : serialize_as_enum_impl< TYPE > { \
	serialize_as_enum() : serialize_as_enum_impl< TYPE >( INFO ) {} \
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
// can be traited as such. Though only auto_ptr<T> is implemented ATM.
//
// The serialize_as_pointer class has the following members:
//  - static bool value
//      True iff this type must be serialized as a pointer.
//      The fields below are only valid (even only present) if this variable
//      is true.
//  - typedef T type
//      The pointed-to type
//  - T* getPointer(X x)
//      Get an actual pointer from the abstract pointer x
//      (X can be a smart pointer type)
//  - void setPointer(X x, T* p, Archive& ar)
//      Copy the raw-pointer p to the abstract pointer x
//      The archive can be used to store per-archive data, this is for example
//      needed for shared_ptr.

template<typename T> struct serialize_as_pointer : is_false {};
template<typename T> struct serialize_as_pointer_impl : is_true
{
	// pointer to primitive types not supported
	STATIC_ASSERT(!is_primitive<T>::value);
	typedef T type;
};
template<typename T> struct serialize_as_pointer<T*>
	: serialize_as_pointer_impl<T>
{
	static inline T* getPointer(T* t) { return t; }
	template<typename Archive>
	static inline void setPointer(T*& t, T* p, Archive& /*ar*/) {
		t = p;
	}
};
template<typename T> struct serialize_as_pointer<std::auto_ptr<T> >
	: serialize_as_pointer_impl<T>
{
	static inline T* getPointer(const std::auto_ptr<T>& t) { return t.get(); }
	template<typename Archive>
	static inline void setPointer(std::auto_ptr<T>& t, T* p, Archive& /*ar*/) {
		t.reset(p);
	}
};
template<typename T> struct serialize_as_pointer<shared_ptr<T> >
	: serialize_as_pointer_impl<T>
{
	static T* getPointer(const shared_ptr<T>& t) { return t.get(); }
	template<typename Archive>
	static void setPointer(shared_ptr<T>& t, T* p, Archive& ar) {
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
// By default arrays, std::vector, std::list, std::set, std::deque and std::map
// are recognized as collections. Though for STL collections you need to add
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
//      becuase we don't need to explicitly store the size.
//  - typedef ... value_type
//      The type stored in the collection (only homogeneous collections are
//      supported).
//  - bool loadInPlace
//      Indicates whether we can directly load the elements in the correct
//      position in the container, otherwise there will be an extra assignment.
//      For this to be possible, the output iterator must support a dereference
//      operator that returns a 'regular' value_type.
//  - typedef ... const_iterator
//  - const_iterator begin(...)
//  - const_iterator end(...)
//      Returns begin/end iterator for the given collection. Used for saving.
//  - typedef ... output_iterator
//  - void prepare(..., int n)
//  - output_iterator output(...)
//      These are used for loading. The prepare() method should prepare the
//      collection to receive 'n' elements. The output() method returns an
//      output_iterator to the beginning of the collection.

template<typename T> struct serialize_as_collection : is_false {};
template<typename T, int N> struct serialize_as_collection<T[N]> : is_true
{
	static const int size = N; // fixed size
	typedef T value_type;
	// save
	typedef const T* const_iterator;
	static const T* begin(const T (&array)[N]) { return &array[0]; }
	static const T* end  (const T (&array)[N]) { return &array[N]; }
	// load
	static const bool loadInPlace = true;
	typedef       T* output_iterator;
	static void prepare(T (&/*array*/)[N], int /*n*/) { }
	static T* output(T (&array)[N]) { return &array[0]; }
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
	                                           bool /*saveId*/)
	{
		STATIC_ASSERT(is_primitive<T>::value);
		ar.save(t);
	}
};
template<typename T> struct EnumSaver
{
	template<typename Archive> void operator()(Archive& ar, const T& t,
	                                           bool /*saveId*/)
	{
		if (ar.translateEnumToString()) {
			serialize_as_enum<T> sae;
			std::string str = sae.toString(t);
			ar.save(str);
		} else {
			ar.save(int(t));
		}
	}
};
template<typename T> struct ClassSaver
{
	template<typename Archive> void operator()(
		Archive& ar, const T& t, bool saveId,
		const char* type = NULL, bool saveConstrArgs = false)
	{
		// Order is important (for non-xml archives). We use this order:
		//    - id
		//    - type
		//    - version
		//    - constructor args
		// Rational:
		//  - 'id' must be first: it could be a null pointer, in that
		//    case the other fields are not even present.
		//  - 'type' must be before version, because for some types we
		//    might not need to store version (though it's unlikely)
		//  - version must be before constructor args because the
		//    constr args depend on the version
		if (saveId) {
			unsigned id = ar.generateId(&t);
			ar.attribute("id", id);
		}

		if (type != NULL) {
			ar.attribute("type", type);
		}

		unsigned version = SerializeClassVersion<T>::value;
		if ((version != 0) && ar.needVersion()) {
			if (!ar.canHaveOptionalAttributes() ||
			    (version != 1)) {
				ar.attribute("version", version);
			}
		}

		if (saveConstrArgs) {
			// save local constructor args (if any)
			SerializeConstructorArgs<T> constrArgs;
			constrArgs.save(ar, t);
		}

		typedef typename remove_const<T>::type TNC;
		TNC& t2 = const_cast<TNC&>(t);
		serialize(ar, t2, version);
	}
};
template<typename TP> struct PointerSaver
{
	// note: we only support pointer to class
	template<typename Archive> void operator()(Archive& ar, const TP& tp2,
	                                           bool /*saveId*/)
	{
		STATIC_ASSERT(serialize_as_pointer<TP>::value);
		typedef typename serialize_as_pointer<TP>::type T;
		const T* tp = serialize_as_pointer<TP>::getPointer(tp2);
		if (tp == NULL) {
			unsigned id = 0;
			ar.attribute("id_ref", id);
			return;
		}
		unsigned id = ar.getId(tp);
		if (id) {
			ar.attribute("id_ref", id);
		} else {
			if (is_polymorphic<T>::value) {
				PolymorphicSaverRegistry<Archive>::save(ar, tp);
			} else {
				ClassSaver<T> saver;
				// don't store type
				// store id, constr-args
				saver(ar, *tp, true, NULL, true);
			}
		}
	}
};
template<typename TP> struct IDSaver
{
	template<typename Archive> void operator()(Archive& ar, const TP& tp2)
	{
		STATIC_ASSERT(serialize_as_pointer<TP>::value);
		typedef typename serialize_as_pointer<TP>::type T;
		const T* tp = serialize_as_pointer<TP>::getPointer(tp2);
		unsigned id;
		if (tp == NULL) {
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
	                                           bool saveId)
	{
		typedef serialize_as_collection<TC> sac;
		STATIC_ASSERT(sac::value);
		typedef typename sac::const_iterator const_iterator;
		const_iterator begin = sac::begin(tc);
		const_iterator end   = sac::end  (tc);
		if ((sac::size < 0) && (!ar.canCountChildren())) {
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
// (implemented as inheriting from a specific baseclass).
template<typename T> struct Saver
	: if_<is_primitive<T>,
	      PrimitiveSaver<T>,
	  if_<serialize_as_enum<T>,
	      EnumSaver<T>,
	  if_<serialize_as_pointer<T>,
	      PointerSaver<T>,
	  if_<serialize_as_collection<T>,
	      CollectionSaver<T>,
	      ClassSaver<T> > > > > {};

////

// Implementation of the different load-strategies.
//
// This matches very closly with the save-strategies above.
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
	void operator()(Archive& ar, T& t, TUPLE /*args*/, int /*id*/)
	{
		STATIC_ASSERT(TUPLE::NUM == 0);
		ar.load(t);
	}
};
template<typename T> struct EnumLoader
{
	template<typename Archive, typename TUPLE>
	void operator()(Archive& ar, T& t, TUPLE /*args*/, int /*id*/)
	{
		STATIC_ASSERT(TUPLE::NUM == 0);
		if (ar.translateEnumToString()) {
			std::string str;
			ar.load(str);
			serialize_as_enum<T> sae;
			t = sae.fromString(str);
		} else {
			int i;
			ar.load(i);
			t = T(i);
		}
	}
};

unsigned loadVersionHelper(MemInputArchive& ar, const char* className,
                           unsigned latestVersion);
unsigned loadVersionHelper(XmlInputArchive& ar, const char* className,
                           unsigned latestVersion);
template<typename T, typename Archive> unsigned loadVersion(Archive& ar)
{
	unsigned latestVersion = SerializeClassVersion<T>::value;
	if ((latestVersion != 0) && ar.needVersion()) {
		return loadVersionHelper(ar, typeid(T).name(), latestVersion);
	} else {
		return latestVersion;
	}
}
template<typename T> struct ClassLoader
{
	template<typename Archive, typename TUPLE>
	void operator()(Archive& ar, T& t, TUPLE /*args*/, int id = 0,
	                int version = -1)
	{
		STATIC_ASSERT(TUPLE::NUM == 0);

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

		typedef typename remove_const<T>::type TNC;
		TNC& t2 = const_cast<TNC&>(t);
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
		typedef typename remove_const<T>::type TNC;
		typedef SerializeConstructorArgs<TNC> ConstrArgs;
		typedef typename ConstrArgs::type LocalTuple;
		ConstrArgs constrArgs;
		LocalTuple localArgs = constrArgs.load(ar, version);

		// combine global and local constr args
		typedef TupleMerger<GlobalTuple, LocalTuple> Merger;
		Merger merger;
		typename Merger::type args = merger(globalArgs, localArgs);
		// TODO make combining global/local constr args configurable

		Creator<T> creator;
		std::auto_ptr<T> tp(creator(args));
		ClassLoader<T> loader;
		loader(ar, *tp, make_tuple(), id, version);
		return tp.release();
	}
};
template<typename T> struct PolymorphicPointerLoader
{
	template<typename Archive, typename TUPLE>
	T* operator()(Archive& ar, unsigned id, TUPLE args)
	{
		typedef typename PolymorphicConstructorArgs<T>::type ArgsType;
		STATIC_ASSERT((is_same_type<TUPLE, ArgsType>::value));
		return static_cast<T*>(
			PolymorphicLoaderRegistry<Archive>::load(ar, id, args));
	}
};
template<typename T> struct PointerLoader2
	// extra indirection needed because inlining the body of
	// NonPolymorphicPointerLoader in PointerLoader does not compile
	// for abstract types
	: if_<is_polymorphic<T>, PolymorphicPointerLoader<T>,
	                         NonPolymorphicPointerLoader<T> > {};

template<typename TP> struct PointerLoader
{
	template<typename Archive, typename GlobalTuple>
	void operator()(Archive& ar, TP& tp2, GlobalTuple globalArgs, int /*id*/)
	{
		STATIC_ASSERT(serialize_as_pointer<TP>::value);
		// in XML archives we use 'id_ref' or 'id', in other archives
		// we don't care about the name
		unsigned id;
		if (ar.canHaveOptionalAttributes() &&
		    ar.hasAttribute("id_ref")) {
			ar.attribute("id_ref", id);
		} else {
			ar.attribute("id", id);
		}

		typedef typename serialize_as_pointer<TP>::type T;
		T* tp;
		if (id == 0) {
			// null-pointer
			tp = NULL;
		} else {
			void* p = ar.getPointer(id);
			if (p) {
				tp = static_cast<T*>(p);
			} else {
				PointerLoader2<T> loader;
				tp = loader(ar, id, globalArgs);
			}
		}
		serialize_as_pointer<TP>::setPointer(tp2, tp, ar);
	}
};
void pointerError(unsigned id);
template<typename TP> struct IDLoader
{
	template<typename Archive>
	void operator()(Archive& ar, TP& tp2)
	{
		STATIC_ASSERT(serialize_as_pointer<TP>::value);
		unsigned id;
		ar.attribute("id_ref", id);

		typedef typename serialize_as_pointer<TP>::type T;
		T* tp;
		if (id == 0) {
			// null-pointer
			tp = NULL;
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
	void operator()(Archive& ar, TUPLE args, OUT_ITER it, int id)
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
	void operator()(Archive& ar, TUPLE args, OUT_ITER it, int id)
	{
		typename sac::value_type elem;
		ar.doSerialize("item", elem, args, id);
		*it = elem;
	}
};
template<typename TC> struct CollectionLoader
{
	template<typename Archive, typename TUPLE>
	void operator()(Archive& ar, TC& tc, TUPLE args, int id = 0)
	{
		assert((id == 0) || (id == -1));
		typedef serialize_as_collection<TC> sac;
		STATIC_ASSERT(sac::value);
		typedef typename sac::output_iterator output_iterator;
		int n = sac::size;
		if (n < 0) {
			// variable size
			if (ar.canCountChildren()) {
				n = ar.countChildren();
			} else {
				ar.serialize("size", n);
			}
		}
		sac::prepare(tc, n);
		output_iterator it = sac::output(tc);
		CollectionLoaderHelper<sac> loadOneElement;
		for (int i = 0; i < n; ++i, ++it) {
			loadOneElement(ar, args, it, id);
		}
	}
};
template<typename T> struct Loader
	: if_<is_primitive<T>,
	      PrimitiveLoader<T>,
	  if_<serialize_as_enum<T>,
	      EnumLoader<T>,
	  if_<serialize_as_pointer<T>,
	      PointerLoader<T>,
	  if_<serialize_as_collection<T>,
	      CollectionLoader<T>,
	      ClassLoader<T> > > > > {};

} // namespace openmsx

#endif
