#ifndef SERIALIZE_HH
#define SERIALIZE_HH

#include "TypeInfo.hh"
#include "XMLElement.hh"
#include "XMLLoader.hh"
#include "StringOp.hh"
#include "Base64.hh"
#include "tuple.hh"
#include "shared_ptr.hh"
#include "noncopyable.hh"
#include "static_assert.hh"
#include "type_traits.hh"
#include <string>
#include <vector>
#include <deque>
#include <list>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <limits>
#include <iostream>
#include <cassert>
#include <cstring>
#include <memory>
#include <algorithm>

namespace openmsx {

/** Utility to do     T* t = new T(...)
 *
 * The tricky part is that the constructor of T can have a variable number
 * of parameters.
 *
 * For example:
 *       Creator<Foo> creator;
 *       Tuple<int, float> args = make_tuple(42, 3.14);
 *       Foo* foo = creator(args);
 * This is equivalent to
 *       Foo* foo = new Foo(42, 3.14);
 * But the former can be used in a generic context (where the number of
 * constructor parameters is unknown).
 */
template<typename T> class Creator
{
public:
	template<typename TUPLE>
	T* operator()(TUPLE args) {
		DoInstantiate<TUPLE::NUM, TUPLE> inst;
		return inst(args);
	}

private:
	template<int I, typename TUPLE> struct DoInstantiate;
	template<typename TUPLE> struct DoInstantiate<0, TUPLE> {
		T* operator()(TUPLE /*args*/) {
			return new T();
		}
	};
	template<typename TUPLE> struct DoInstantiate<1, TUPLE> {
		T* operator()(TUPLE args) {
			return new T(args.t1);
		}
	};
	template<typename TUPLE> struct DoInstantiate<2, TUPLE> {
		T* operator()(TUPLE args) {
			return new T(args.t1, args.t2);
		}
	};
	template<typename TUPLE> struct DoInstantiate<3, TUPLE> {
		T* operator()(TUPLE args) {
			return new T(args.t1, args.t2, args.t3);
		}
	};
};

/** Serialize (local) constructor arguments.
 *
 * Some classes don't have a default constructor. To be able to create new
 * instances of such classes, we need to invoke the constructor with some
 * parameters (see also Creator utility class above).
 *
 * SerializeConstructorArgs can be specialized for classes that need such extra
 * constructor arguments.
 *
 * This only stores the 'local' constructor arguments, this means the arguments
 * that are specific per instance. There can also be 'global' args, these are
 * known in the context where the class is being loaded (so these don't need to
 * be stored in the archive). See below for more details on global constr args.
 *
 * The serialize_as_enum class has the following members:
 *   typedef Tuple<...> type
 *     Tuple that holds the result of load() (see below)
 *   void save(Archive& ar, const T& t)
 *     This method should store the constructor args in the given archive
 *   type load(Archive& ar, unsigned version)
 *     This method should load the args from the given archive and return
 *     them in a tuple.
 */
template<typename T> struct SerializeConstructorArgs
{
	typedef Tuple<> type;
	template<typename Archive>
	void save(Archive& /*ar*/, const T& /*t*/) { }
	template<typename Archive> type
	load(Archive& /*ar*/, unsigned /*version*/) { return make_tuple(); }
};

///////////////////////////////

// Polymorphic class loader/saver

// forward declarations
// ClassSaver: used to save actually save a class. We also store the name of
//   the class so that the loader knows which concrete class it should load.
template<typename T> struct ClassSaver;
// NonPolymorphicPointerLoader: once we know which concrete type to load,
//   we use the 'normal' class loader to load it.
template<typename T> struct NonPolymorphicPointerLoader;

/** Store association between polymorphic class (base- or subclass) and
 *  the list of constructor arguments.
 * Specializations of this class should store the constructor arguments
 * as a 'typedef tupple<...> type'.
 */
template<typename T> struct PolymorphicConstructorArgs;

/** Store association between (polymorphic) sub- and baseclass.
 * Specialization of this class should provide a 'typedef <base> type'.
 */
template<typename T> struct PolymorphicBaseClass;

template<typename Base> struct MapConstrArgsEmpty
{
	typedef typename PolymorphicConstructorArgs<Base>::type TUPLEIn;
	Tuple<> operator()(const TUPLEIn& /*t*/)
	{
		return make_tuple();
	}
};
template<typename Base, typename Derived> struct MapConstrArgsCopy
{
	typedef typename PolymorphicConstructorArgs<Base>::type TUPLEIn;
	typedef typename PolymorphicConstructorArgs<Derived>::type TUPLEOut;
	STATIC_ASSERT((is_same_type<TUPLEIn, TUPLEOut>::value));
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
	: if_<is_same_type<Tuple<>,
	                   typename PolymorphicConstructorArgs<Derived>::type>,
	      MapConstrArgsEmpty<Base>,
	      MapConstrArgsCopy<Base, Derived> > {};

/** Stores the name of a base class.
 * This name is used as tag-name in XML archives.
 *
 * Specializations of this class should provide a function
 *   static const char* getName()
 */
template<typename Base> struct BaseClassName;

template<typename Archive> class PolymorphicSaverBase
{
public:
	virtual ~PolymorphicSaverBase() {}
	virtual void save(Archive& ar, const void* p) const = 0;
};

template<typename Archive> class PolymorphicLoaderBase
{
public:
	virtual ~PolymorphicLoaderBase() {}
	virtual void* load(Archive& ar, unsigned id, const TupleBase& args) const = 0;
};

template<typename Archive, typename T>
class PolymorphicSaver : public PolymorphicSaverBase<Archive>
{
public:
	PolymorphicSaver(const std::string& name_)
		: name(name_)
	{
	}
	virtual void save(Archive& ar, const void* p) const
	{
		ClassSaver<T> saver;
		const T* tp = static_cast<const T*>(p);
		saver(ar, *tp, name, true, true); // save type, id, constr-args
	}
private:
	const std::string name;
};

template<typename Archive, typename T>
class PolymorphicLoader : public PolymorphicLoaderBase<Archive>
{
public:
	virtual void* load(Archive& ar, unsigned id, const TupleBase& args) const
	{
		typedef typename PolymorphicBaseClass<T>::type BaseType;
		typedef typename PolymorphicConstructorArgs<BaseType>::type TUPLEIn;
		typedef typename PolymorphicConstructorArgs<T>::type TUPLEOut;
		const TUPLEIn& argsIn = static_cast<const TUPLEIn&>(args);
		MapConstructorArguments<BaseType, T> mapArgs;
		TUPLEOut argsOut = mapArgs(argsIn);
		NonPolymorphicPointerLoader<T> loader;
		return loader(ar, id, argsOut);
	}
};


template<typename Archive>
class PolymorphicSaverRegistry : private noncopyable
{
public:
	static PolymorphicSaverRegistry& instance()
	{
		static PolymorphicSaverRegistry oneInstance;
		return oneInstance;
	}

	template<typename T> void registerClass(const std::string& name)
	{
		STATIC_ASSERT(is_polymorphic<T>::value);
		STATIC_ASSERT(!is_abstract<T>::value);
		saverMap[typeid(T)] = new PolymorphicSaver<Archive, T>(name);
	}

	template<typename T> const PolymorphicSaverBase<Archive>& getSaver(T& t)
	{
		TypeInfo typeInfo(typeid(t));
		typename SaverMap::const_iterator it = saverMap.find(typeInfo);
		if (it == saverMap.end()) {
			std::cerr << "Trying to save an unregistered polymorphic type: "
			          << typeInfo.name() << std::endl;
			assert(false);
		}
		return *it->second;
	}

private:
	PolymorphicSaverRegistry() {}
	~PolymorphicSaverRegistry()
	{
		for (typename SaverMap::const_iterator it = saverMap.begin();
		     it != saverMap.end(); ++it) {
			delete it->second;
		}
	}

	typedef std::map<TypeInfo, PolymorphicSaverBase <Archive>*> SaverMap;
	SaverMap saverMap;
};

template<typename Archive>
class PolymorphicLoaderRegistry : private noncopyable
{
public:
	static PolymorphicLoaderRegistry& instance()
	{
		static PolymorphicLoaderRegistry oneInstance;
		return oneInstance;
	}

	template<typename T> void registerClass(const std::string& name)
	{
		STATIC_ASSERT(is_polymorphic<T>::value);
		STATIC_ASSERT(!is_abstract<T>::value);
		loaderMap[name] = new PolymorphicLoader<Archive, T>();
	}

	const PolymorphicLoaderBase<Archive>& getLoader(const std::string& type)
	{
		typename LoaderMap::const_iterator it = loaderMap.find(type);
		assert(it != loaderMap.end());
		return *it->second;
	}

private:
	PolymorphicLoaderRegistry() {}
	~PolymorphicLoaderRegistry()
	{
		for (typename LoaderMap::const_iterator it = loaderMap.begin();
		     it != loaderMap.end(); ++it) {
			delete it->second;
		}
	}

	typedef std::map<std::string, PolymorphicLoaderBase<Archive>*> LoaderMap;
	LoaderMap loaderMap;
};


template<typename Archive, typename T> struct RegisterSaverHelper
{
	RegisterSaverHelper(const std::string& name)
	{
		PolymorphicSaverRegistry<Archive>::instance().
			template registerClass<T>(name);
	}
};
template<typename Archive, typename T> struct RegisterLoaderHelper
{
	RegisterLoaderHelper(const std::string& name)
	{
		PolymorphicLoaderRegistry<Archive>::instance().
			template registerClass<T>(name);
	}
};

#define REGISTER_CONSTRUCTOR_ARGS_0(C) \
template<> struct PolymorphicConstructorArgs<C> \
{ typedef Tuple<> type; };

#define REGISTER_CONSTRUCTOR_ARGS_1(C,T1) \
template<> struct PolymorphicConstructorArgs<C> \
{ typedef Tuple<T1> type; };

#define REGISTER_CONSTRUCTOR_ARGS_2(C,T1,T2) \
template<> struct PolymorphicConstructorArgs<C> \
{ typedef Tuple<T1,T2> type; };

#define REGISTER_CONSTRUCTOR_ARGS_3(C,T1,T2,T3) \
template<> struct PolymorphicConstructorArgs<C> \
{ typedef Tuple<T1,T2,T3> type; };

#define REGISTER_POLYMORPHIC_CLASS_HELPER(B,C,N) \
STATIC_ASSERT((is_base_and_derived<B,C>::value)); \
static RegisterLoaderHelper<TextInputArchive,  C> registerHelper1##C(N); \
static RegisterSaverHelper <TextOutputArchive, C> registerHelper2##C(N); \
static RegisterLoaderHelper<XmlInputArchive,   C> registerHelper3##C(N); \
static RegisterSaverHelper <XmlOutputArchive,  C> registerHelper4##C(N); \
static RegisterLoaderHelper<MemInputArchive,   C> registerHelper5##C(N); \
static RegisterSaverHelper <MemOutputArchive,  C> registerHelper6##C(N); \
template<> struct PolymorphicBaseClass<C> { typedef B type; };

#define REGISTER_BASE_NAME_HELPER(B,N) \
template<> struct BaseClassName<B> \
{ static const char* getName() { static const char* name = N; return name; } };

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
	static const unsigned value = 1;
};
#define SERIALIZE_CLASS_VERSION(CLASS, VERSION) \
template<> struct SerializeClassVersion<CLASS> \
{ \
	static const unsigned value = VERSION; \
};

/////////////

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
// are recognized as collections.
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
	typedef       T* output_iterator;
	static void prepare(T (&/*array*/)[N], int /*n*/) { }
	static T* output(T (&array)[N]) { return &array[0]; }
};
template<typename T> struct serialize_as_stl_collection : is_true
{
	static const int size = -1; // variable size
	typedef typename T::value_type value_type;
	// save
	typedef typename T::const_iterator const_iterator;
	static const_iterator begin(const T& t) { return t.begin(); }
	static const_iterator end  (const T& t) { return t.end();   }
	// load
	typedef typename std::insert_iterator<T> output_iterator;
	static void prepare(T& t, int /*n*/) { t.clear(); }
	static output_iterator output(T& t) { return std::inserter(t, t.begin()); }
};
template<typename T> struct serialize_as_collection<std::list<T> >
	: serialize_as_stl_collection<std::list<T> > {};
template<typename T> struct serialize_as_collection<std::set<T> >
	: serialize_as_stl_collection<std::set<T> > {};
template<typename T> struct serialize_as_collection<std::deque<T> >
	: serialize_as_stl_collection<std::deque<T> > {};
template<typename T1, typename T2> struct serialize_as_collection<std::map<T1, T2> >
	: serialize_as_stl_collection<std::map<T1, T2> > {};
template<typename T> struct serialize_as_collection<std::vector<T> >
	: serialize_as_stl_collection<std::vector<T> >
{
	// override save-part from base class,
	// can be done more efficient for vector
	typedef typename std::vector<T>::iterator output_iterator;
	static void prepare(std::vector<T>& v, int n) { v.resize(n); }
	static output_iterator output(std::vector<T>& v) { return v.begin(); }
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

template<typename T> struct PrimitiveSaver
{
	template<typename Archive> void operator()(Archive& ar, const T& t)
	{
		STATIC_ASSERT(is_primitive<T>::value);
		ar.save(t);
	}
};
template<typename T> struct EnumSaver
{
	template<typename Archive> void operator()(Archive& ar, const T& t)
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
		Archive& ar, const T& t, const std::string& type = "",
		bool saveId = true, bool saveConstrArgs = false)
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

		if (!type.empty()) {
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
	template<typename Archive> void operator()(Archive& ar, const TP& tp2)
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
				const PolymorphicSaverBase<Archive>& saver =
					PolymorphicSaverRegistry<Archive>::
						instance().getSaver(*tp);
				saver.save(ar, tp);
			} else {
				ClassSaver<T> saver;
				// don't store type
				// store id, constr-args
				saver(ar, *tp, "", true, true);
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
	template<typename Archive> void operator()(Archive& ar, const TC& tc)
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
			int n = std::distance(begin, end);
			ar.serialize("size", n);
		}
		for (/**/; begin != end; ++begin) {
			ar.serialize("item", *begin);
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

template<typename T> struct PrimitiveLoader
{
	template<typename Archive, typename TUPLE>
	void operator()(Archive& ar, T& t, TUPLE /*args*/)
	{
		STATIC_ASSERT(TUPLE::NUM == 0);
		ar.load(t);
	}
};
template<typename T> struct EnumLoader
{
	template<typename Archive, typename TUPLE>
	void operator()(Archive& ar, T& t, TUPLE /*args*/)
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
template<typename T, typename Archive> unsigned loadVersion(Archive& ar)
{
	unsigned version = SerializeClassVersion<T>::value;
	if ((version != 0) && ar.needVersion()) {
		if (!ar.canHaveOptionalAttributes() ||
		    ar.hasAttribute("version")) {
			ar.attribute("version", version);
		} else {
			version = 1;
		}
	}
	return version;
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
		T* tp = creator(args);
		ClassLoader<T> loader;
		loader(ar, *tp, make_tuple(), id, version);
		return tp;
	}
};
template<typename T> struct PolymorphicPointerLoader
{
	template<typename Archive, typename TUPLE>
	T* operator()(Archive& ar, unsigned id, TUPLE args)
	{
		typedef typename PolymorphicConstructorArgs<T>::type ArgsType;
		STATIC_ASSERT((is_same_type<TUPLE, ArgsType>::value));
		std::string type;
		ar.attribute("type", type);
		const PolymorphicLoaderBase<Archive>& loader =
			PolymorphicLoaderRegistry<Archive>::instance().getLoader(type);
		return static_cast<T*>(loader.load(ar, id, args));
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
	void operator()(Archive& ar, TP& tp2, GlobalTuple globalArgs)
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
			assert(p); // TODO throw
			tp = static_cast<T*>(p);
		}
		serialize_as_pointer<TP>::setPointer(tp2, tp, ar);
	}
};
template<typename TC> struct CollectionLoader
{
	template<typename Archive, typename TUPLE>
	void operator()(Archive& ar, TC& tc, TUPLE args)
	{
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
		for (int i = 0; i < n; ++i, ++it) {
			// TODO this screws-up id/pointer management
			//   because the element is still copied after
			//   construction (pointer value of construction is
			//   stored)
			//   For std::vector this is not a problem, but it
			//   might be for arrays or lists.
			typename sac::value_type elem;
			ar.doSerialize("item", elem, args);
			*it = elem;
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

//////////////////

// In this section, the archive classes are defined.
//
// Archives can be categorized in two ways:
//   - backing stream they use
//   - input or output (each backing stream has exactly one input and one
//     output variant)
//
// ATM these backing streams implemented:
//   - Mem
//      Stores stream in memory. Is meant to be very compact and very fast.
//      It does not support versioning (it's not possible to load this stream
//      in a newer version of openMSX). It is also not platform independent
//      (e.g. integers are stored using native platform endianess).
//      The main use case for this archive format is regular in memory
//      snapshots, for example to support replay/rewind.
//   - XML
//      Stores the stream in a XML file. These files are meant to be portable
//      to different architectures (e.g. little/big endian, 32/64 bit system).
//      There is version information in the stream, so it should be possible
//      to load streams created with older openMSX versions a newer openMSX.
//      The XML files are meant to be human readable. Having editable XML files
//      is not a design goal (e.g. simply changing a value will probably work,
//      but swapping the position of two tag or adding or removing tags can
//      easily break the stream).
//   - Text
//      This stores to stream in a flat ascii file (one item per line). This
//      format is only written as a proof-of-concept to test the design. It's
//      not meant to be used in practice.
//
// Archive code is heavily templatized. It uses the CRTP (curiously recuring
// template pattern ; a base class templatized on it's derived class). To
// implement static polymorphism. This means there is practically no run-time
// overhead of using this mechansim compared to 6 seperatly handcoded functions
// (Mem/XML/Text x input/output).
// TODO At least in theory, still need to verify this in practice.
//      Though my experience is that gcc is generally very good in this.

template<typename Derived> class ArchiveBase
{
public:
	/** Is this archive a loader or a saver.
	bool isLoader() const;*/

	/** Serialize the base class of this classtype.
	 * Should preferably be called as the first statement in the
	 * implementation of a serialize() method of a class type.
	 * See also serializeInlinedBase() below.
	 */
	template<typename Base, typename T>
	void serializeBase(T& t)
	{
		const char* tag = BaseClassName<Base>::getName();
		self().serializeNoID(tag, static_cast<Base&>(t));
	}

	/** Serialize the base class of this classtype.
	 * Should preferably be called as the first statement in the
	 * implementation of a serialize() method of a class type.
	 * See also serializeBase() above.
	 *
	 * The differece between serializeBase() and serializeInlinedBase()
	 * is only relevant for versioned archives (see needVersion(), e.g.
	 * XML archives). In XML archives serializeBase() will put the base
	 * class in a new subtag, serializeInlinedBase() puts the members
	 * of the base class (inline) in the current tag. The advantage
	 * of serializeBase() is that the base class can be versioned
	 * seperatly from the subclass. The disadvantage is that it exposes
	 * an internal implementation detail in the XML file, and thus makes
	 * it harder to for example change the class hierarchy or move
	 * members from base to subclass or vice-versa.
	 */
	template<typename Base, typename T>
	void serializeInlinedBase(T& t, unsigned version)
	{
		::openmsx::serialize(self(), static_cast<Base&>(t), version);
	}

	// Each concrete archive class also has the following methods:
	// Because of the implementation with static polymorphism, this
	// interface is not explictly visible in the base class.
	//
	//
	// template<typename T> void serialize(const char* tag, const T& t, ...)
	//
	//   This is _the_most_important_ method of the serialization
	//   framework. Depending on the concrete archive type (loader/saver)
	//   this method will load or save the given type 't'. In case of an XML
	//   archive the 'tag' parameter will be used as tagname.
	//
	//   At the end there are still a number of optional parameters (in the
	//   current implementation there can be between 0 and 3, but can be
	//   extened when needed). These are 'global' constructor parameters,
	//   constructor parameters that are not stored in the stream, but that
	//   are needed to reconstruct the object (for example can be references
	//   to structures that were already stored in the stream). So these
	//   parameters are only actually used while loading.
	//   TODO document this in more detail in some section where the
	//        (polymorphic) constructors are also described.
	//
	//
	// void serialize_blob(const char* tag, const void* data, unsigned len)
	//
	//   Serialize the given data as a binary blob.
	//   This cannot be part of the serialize() method above because we
	//   cannot know whether a byte-array should be serialized as a blob
	//   or as a collection of bytes (IOW we cannot decide it based on the
	//   type).
	//
	//
	// template<typename T> void serializeNoID(const char* tag, const T& t)
	//
	//   This is much like the serialize() method above, but it doesn't
	//   store an ID with this element. This means that it's not possible,
	//   later on in the stream, to refer to this element. For many elements
	//   you know this will not happen. This method results in a slightly
	//   more compact stream.
	//
	//   Note that for primitive types we already don't store an ID, because
	//   pointers to primitive types are not supported (at least not ATM).
	//
	//
	// template<typename T> void serializePointerID(const char* tag, const T& t)
	//
	//   Serialize a pointer by storing the ID of the object it points to.
	//   This only works if the object was already serialized. The only
	//   reason to use this method instead of the more general serialize()
	//   method is that this one does not instantiate the object
	//   construction code. (So in some cases you can avoid having to
	//   provide specializations of SerializeConstructorArgs.)

/*internal*/
	// These must be public for technical reasons, but they should only
	// be used by the serialization framework.

	/** Does this archive store version information. */
	bool needVersion() const { return true; }

	/** Does this archive store enums as strings.
	 * See also struct serialize_as_enum.
	 */
	bool translateEnumToString() const { return false; }

	/** Load/store an attribute from/in the archive.
	 * Depending on the underlying concrete stream, attributes are either
	 * stored like XML attributes or as regular values. Because of this
	 * (and thus unlike XML attributes) the order of attributes matter. It
	 * also matters whether an attribute is present or not.
	 */
	template<typename T> void attribute(const char* name, T& t)
	{
		self().serialize(name, t);
	}

	/** Some archives (like XML archives) can store optional attributes.
	 * This method indicates whether that's the case or not.
	 * This can be used to for example in XML files don't store attributes
	 * with default values (thus to make the XML look prettier).
	 */
	bool canHaveOptionalAttributes() const { return false; }

	/** Check the presence of a (optional) attribute.
	 * It's only allowed to call this method on archives that can have
	 * optional attributes.
	 */
	bool hasAttribute(const std::string& /*name*/)
	{
		assert(false); return false;
	}

	/** Some archives (like XML archives) can count the number of subtags
	 * that belong to the current tag. This method indicates whether that's
	 * the case for this archive or not.
	 * This can for example be used to make the XML files look prettier in
	 * case of serialization of collections: in that case we don't need to
	 * explictly store the size of the collection, it can be derived from
	 * the number of subtags.
	 */
	bool canCountChildren() const { return false; }

	/** Count the number of child tags.
	 * It's only allowed to call this method on archives that have support
	 * for this operation.
	 */
	int countChildren() const { assert(false); return -1; }

	/** Indicate begin of a tag.
	 * Only XML archives use this, other archives ignore it.
	 * XML saver uses it as a name for the current tag, it doesn't
	 * interpret the name in any way.
	 * XML loader uses it only as a check: it checks whether the current
	 * tag name matches the given name. So we will NOT search the tag
	 * with the given name, the tags have to be in the correct order.
	 */
	void beginTag(const char* /*tag*/)
	{
		// nothing
	}
	/** Indicate begin of a tag.
	 * Only XML archives use this, other archives ignore it.
	 * Both XML loader and saver only use the given tag name to do some
	 * internal checks (with checks disabled, the tag parameter has no
	 * influence at all on loading or saving of the stream).
	 */
	void endTag(const char* /*tag*/)
	{
		// nothing
	}

	// These (internal) methods should be implemented in the concrete
	// archive classes.
	//
	// template<typename T> void save(const T& t)
	//
	//   Should only be implemented for OuputArchives. Is called to
	//   store primitive types in the stream. In the end all structures
	//   are broken down to primitive types, so all data that ends up
	//   in the stream passes via this method (ok, depending on how
	//   attribute() and serialize_blob() is implemented, that data may
	//   not pass via save()).
	//
	//   Often this method will be overloaded to handle different certain
	//   types in a specific way.
	//
	//
	// template<typename T> void load(T& t)
	//
	//   Should only be implemented for InputArchives. This is similar (but
	//   opposite) to the save() method above. Loading of primitive types
	//   is done via this method.

protected:
	/** Returns a reference to the most derived class.
	 * Helper function to implement static polymorphism.
	 */
	inline Derived& self()
	{
		return static_cast<Derived&>(*this);
	}
};

template<typename Derived>
class OutputArchiveBase : public ArchiveBase<Derived>
{
	typedef ArchiveBase<Derived> THIS;
public:
	bool isLoader() const { return false; }

	OutputArchiveBase()
		: lastId(0)
	{
	}

	// Main saver method. Heavy lifting is done in the Saver class.
	template<typename T> void serialize(const char* tag, const T& t)
	{
		this->self().beginTag(tag);
		Saver<T> saver;
		saver(this->self(), t);
		this->self().endTag(tag);
	}
	// 3 methods below implement 'global constructor arguments'. Though
	// the saver archives completly ignore those extra parameters. We
	// anyway need to provide them because the same (templatized) code
	// path is used both for saving and loading.
	template<typename T, typename T1>
	void serialize(const char* tag, const T& t, T1 /*t1*/)
	{
		serialize(tag, t);
	}
	template<typename T, typename T1, typename T2>
	void serialize(const char* tag, const T& t, T1 /*t1*/, T2 /*t2*/)
	{
		serialize(tag, t);
	}
	template<typename T, typename T1, typename T2, typename T3>
	void serialize(const char* tag, const T& t, T1 /*t1*/, T2 /*t2*/, T3 /*t3*/)
	{
		serialize(tag, t);
	}

	// Default implementation is to base64-encode the blob and serialize
	// the resulting string. But memory archives will memcpy the blob.
	void serialize_blob(const char* tag, const void* data, unsigned len)
	{
		std::string tmp = Base64::encode(data, len);
		serialize(tag, tmp);
	}
	template<typename T> void serializeNoID(const char* tag, const T& t)
	{
		this->self().beginTag(tag);
		Saver<T> saver;
		saver(this->self(), t, "", false);
		this->self().endTag(tag);
	}
	template<typename T> void serializePointerID(const char* tag, const T& t)
	{
		this->self().beginTag(tag);
		IDSaver<T> saver;
		saver(this->self(), t);
		this->self().endTag(tag);
	}

/*internal*/
	// Generate a new ID for the given pointer and store this association
	// for later (see getId()).
	template<typename T> unsigned generateId(const T* p)
	{
		// For composed structures, for example
		//   struct A { ... };
		//   struct B { A a; ... };
		// The pointer to the outer and inner structure can be the
		// same while we still want a different ID to refer to these
		// two. That's why we use a std::pair<void*, TypeInfo> as key
		// in the map.
		// For polymorphic types you do sometimes use a base pointer
		// to refer to a subtype. So there we only use the pointer
		// value as key in the map.
		++lastId;
		if (is_polymorphic<T>::value) {
			assert(polyIdMap.find(p) == polyIdMap.end());
			polyIdMap[p] = lastId;
		} else {
			IdKey key = std::make_pair(p, TypeInfo(typeid(T)));
			assert(idMap.find(key) == idMap.end());
			idMap[key] = lastId;
		}
		return lastId;
	}
	template<typename T> unsigned getId(const T* p)
	{
		if (is_polymorphic<T>::value) {
			PolyIdMap::const_iterator it = polyIdMap.find(p);
			return it != polyIdMap.end() ? it->second : 0;
		} else {
			IdKey key = std::make_pair(p, TypeInfo(typeid(T)));
			IdMap::const_iterator it = idMap.find(key);
			return it != idMap.end() ? it->second : 0;
		}
	}

private:
	typedef std::pair<const void*, TypeInfo> IdKey;
	typedef std::map<IdKey, unsigned> IdMap;
	typedef std::map<const void*, unsigned> PolyIdMap;
	IdMap idMap;
	PolyIdMap polyIdMap;
	unsigned lastId;
};

template<typename Derived>
class InputArchiveBase : public ArchiveBase<Derived>
{
	typedef ArchiveBase<Derived> THIS;
public:
	bool isLoader() const { return true; }

	template<typename T>
	void serialize(const char* tag, T& t)
	{
		doSerialize(tag, t, make_tuple());
	}
	template<typename T, typename T1>
	void serialize(const char* tag, T& t, T1 t1)
	{
		doSerialize(tag, t, make_tuple(t1));
	}
	template<typename T, typename T1, typename T2>
	void serialize(const char* tag, T& t, T1 t1, T2 t2)
	{
		doSerialize(tag, t, make_tuple(t1, t2));
	}
	template<typename T, typename T1, typename T2, typename T3>
	void serialize(const char* tag, T& t, T1 t1, T2 t2, T3 t3)
	{
		doSerialize(tag, t, make_tuple(t1, t2, t3));
	}
	void serialize_blob(const char* tag, void* data, unsigned len)
	{
		std::string tmp;
		serialize(tag, tmp);
		std::string tmp2 = Base64::decode(tmp);
		assert(tmp2.size() == len); // TODO exception
		memcpy(data, tmp2.data(), len);
	}
	template<typename T>
	void serializeNoID(const char* tag, T& t)
	{
		this->self().beginTag(tag);
		typedef typename remove_const<T>::type TNC;
		TNC& tnc = const_cast<TNC&>(t);
		Loader<TNC> loader;
		loader(this->self(), tnc, make_tuple(), -1); // don't load id
		this->self().endTag(tag);
	}
	template<typename T> void serializePointerID(const char* tag, const T& t)
	{
		this->self().beginTag(tag);
		typedef typename remove_const<T>::type TNC;
		TNC& tnc = const_cast<TNC&>(t);
		IDLoader<TNC> loader;
		loader(this->self(), tnc);
		this->self().endTag(tag);
	}

/*internal*/
	void* getPointer(unsigned id)
	{
		IdMap::const_iterator it = idMap.find(id);
		return it != idMap.end() ? it->second : NULL;
	}
	void addPointer(unsigned id, const void* p)
	{
		assert(idMap.find(id) == idMap.end());
		idMap[id] = const_cast<void*>(p);
	}
	template<typename T> void resetSharedPtr(shared_ptr<T>& s, T* r)
	{
		if (r == NULL) {
			s.reset();
			return;
		}
		SharedPtrMap::const_iterator it = sharedPtrMap.find(r);
		if (it == sharedPtrMap.end()) {
			s.reset(r);
			sharedPtrMap[r] = &s;
			//sharedPtrMap[r] = s;
		} else {
			shared_ptr<T>* s2 = static_cast<shared_ptr<T>*>(it->second);
			s = *s2;
			//s = std::tr1::static_pointer_cast<T>(it->second);
		}
	}

	// Actual loader method. Heavy lifting is done in the Loader class.
	template<typename T, typename TUPLE>
	void doSerialize(const char* tag, T& t, TUPLE args)
	{
		this->self().beginTag(tag);
		typedef typename remove_const<T>::type TNC;
		TNC& tnc = const_cast<TNC&>(t);
		Loader<TNC> loader;
		loader(this->self(), tnc, args);
		this->self().endTag(tag);
	}

private:
	typedef std::map<unsigned, void*> IdMap;
	IdMap idMap;

	typedef std::map<void*, void*> SharedPtrMap;
	//typedef std::map<void*, std::tr1::shared_ptr<void> > SharedPtrMap;
	SharedPtrMap sharedPtrMap;
};

////

class TextOutputArchive : public OutputArchiveBase<TextOutputArchive>
{
public:
	TextOutputArchive(const std::string& filename)
		: os(filename.c_str())
	{
	}

	template<typename T> void save(const T& t)
	{
		if (is_floating<T>::value) {
			os << std::setprecision(std::numeric_limits<T>::digits10 + 2);
		}
		os << t << '\n';
	}
	void serialize_blob(const char* tag, const void* data, unsigned len)
	{
		std::string tmp = Base64::encode(data, len, false); // no newlines
		serialize(tag, tmp);
	}

private:
	std::ofstream os;
};

class TextInputArchive : public InputArchiveBase<TextInputArchive>
{
public:
	TextInputArchive(const std::string& filename)
		: is(filename.c_str())
	{
	}

	template<typename T> void load(T& t)
	{
		std::string s;
		getline(is, s);
		std::istringstream ss(s);
		ss >> t;
	}
	void load(std::string& t)
	{
		// !!! this doesn't handle strings with !!!
		// !!! embedded newlines correctly      !!!
		getline(is, t);
	}

private:
	std::ifstream is;
};

////

class MemOutputArchive : public OutputArchiveBase<MemOutputArchive>
{
public:
	MemOutputArchive(std::vector<char>& buffer_)
		: buffer(buffer_)
	{
	}

	bool needVersion() const { return false; }

	template <typename T> void save(const T& t)
	{
		put(&t, sizeof(t));
	}
	void save(const std::string& s)
	{
		unsigned size = s.size();
		save(size);
		put(s.data(), size);
	}
	void serialize_blob(const char* /*tag*/, const void* data, unsigned len)
	{
		put(data, len);
	}

private:
	void put(const void* data, unsigned len)
	{
		if (len) {
			unsigned pos = buffer.size();
			buffer.resize(pos + len);
			memcpy(&buffer[pos], data, len);
		}
	}

	std::vector<char>& buffer;
};

class MemInputArchive : public InputArchiveBase<MemInputArchive>
{
public:
	MemInputArchive(const std::vector<char>& buffer_)
		: buffer(buffer_)
		, pos(0)
	{
	}

	bool needVersion() const { return false; }

	template<typename T> void load(T& t)
	{
		get(&t, sizeof(t));
	}
	void load(std::string& s)
	{
		unsigned length;
		load(length);
		s.resize(length);
		if (length) {
			get(&s[0], length);
		}
	}
	void serialize_blob(const char* /*tag*/, void* data, unsigned len)
	{
		get(data, len);
	}

private:
	void get(void* data, unsigned len)
	{
		if (len) {
			assert((pos + len) <= buffer.size());
			memcpy(data, &buffer[pos], len);
			pos += len;
		}
	}

	const std::vector<char>& buffer;
	unsigned pos;
};

////

using namespace openmsx; // ***

class XmlOutputArchive : public OutputArchiveBase<XmlOutputArchive>
{
public:
	XmlOutputArchive(const std::string& filename)
		: os(filename.c_str())
		, current(new XMLElement("serial"))
	{
	}

	~XmlOutputArchive()
	{
		os << "<?xml version=\"1.0\" ?>\n"
                      "<!DOCTYPE openmsx-serialize SYSTEM 'openmsx-serialize.dtd'>\n";
		os << current->dump();
		delete current;
	}

	template <typename T> void save(const T& t)
	{
		assert(current);
		assert(current->getData().empty());
		// TODO make sure floating point is printed with enough digits
		//      maybe print as hex?
		current->setData(StringOp::toString(t));
	}
	void save(bool b)
	{
		assert(current);
		assert(current->getData().empty());
		current->setData(b ? "true" : "false");
	}
	void save(unsigned char b)
	{
		save(unsigned(b));
	}

/*protected:*/
	bool translateEnumToString() const { return true; }
	void beginTag(const std::string& tag)
	{
		XMLElement* elem = new XMLElement(tag);
		if (current) {
			current->addChild(std::auto_ptr<XMLElement>(elem));
		}
		current = elem;
	}
	void endTag(const std::string& tag)
	{
		assert(current);
		assert(current->getName() == tag);
		(void)tag;
		current = current->getParent();
	}
	bool canHaveOptionalAttributes() const { return true; }
	template<typename T> void attribute(const std::string& name, T& t)
	{
		assert(current);
		assert(!current->hasAttribute(name));
		current->addAttribute(name, StringOp::toString(t));
	}
	bool canCountChildren() const { return true; }

private:
	std::ofstream os;
	XMLElement* current;
};

class XmlInputArchive : public InputArchiveBase<XmlInputArchive>
{
public:
	XmlInputArchive(const std::string& filename)
		: elem(openmsx::XMLLoader::loadXML(filename, "openmsx-serialize.dtd"))
		, pos(0)
	{
		init(elem.get());
	}

	void init(const XMLElement* e)
	{
		elems.push_back(e);
		const XMLElement::Children& children = e->getChildren();
		for (XMLElement::Children::const_iterator it = children.begin();
		     it != children.end(); ++it) {
			init(*it);
		}
		elems.push_back(e);
	}

	template<typename T> void load(T& t)
	{
		assert(elems[pos]->getChildren().empty());
		std::istringstream is(elems[pos]->getData());
		is >> t;
	}
	void load(std::string& t)
	{
		assert(elems[pos]->getChildren().empty());
		t = elems[pos]->getData();
	}
	void load(bool& b)
	{
		assert(elems[pos]->getChildren().empty());
		std::string s = elems[pos]->getData();
		if (s == "true") {
			b = true;
		} else if (s == "false") {
			b = false;
		} else {
			// throw
			assert(false);
		}
	}
	void load(unsigned char& b)
	{
		unsigned i;
		load(i);
		b = i;
	}

/*protected:*/
	bool translateEnumToString() const { return true; }
	void beginTag(const std::string& tag)
	{
		++pos;
		assert(elems[pos]->getName() == tag);
		(void)tag;
	}
	void endTag(const std::string& tag)
	{
		++pos;
		assert(elems[pos]->getName() == tag);
		(void)tag;
	}
	template<typename T> void attribute(const std::string& name, T& t)
	{
		assert(elems[pos]->hasAttribute(name));
		std::istringstream is(elems[pos]->getAttribute(name));
		is >> t;
	}
	bool canHaveOptionalAttributes() const { return true; }
	bool hasAttribute(const std::string& name)
	{
		return elems[pos]->hasAttribute(name);
	}
	bool canCountChildren() const { return true; }
	int countChildren() const { return elems[pos]->getChildren().size(); }

private:
	std::auto_ptr<XMLElement> elem;
	std::vector<const XMLElement*> elems;
	unsigned pos;
};

#define INSTANTIATE_SERIALIZE_METHODS(CLASS) \
template void CLASS::serialize(TextInputArchive&,  unsigned); \
template void CLASS::serialize(TextOutputArchive&, unsigned); \
template void CLASS::serialize(MemInputArchive&,   unsigned); \
template void CLASS::serialize(MemOutputArchive&,  unsigned); \
template void CLASS::serialize(XmlInputArchive&,   unsigned); \
template void CLASS::serialize(XmlOutputArchive&,  unsigned);

} // namespace openmsx

#endif
