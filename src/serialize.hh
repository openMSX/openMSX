#ifndef SERIALIZE_HH
#define SERIALIZE_HH

#include "serialize_core.hh"
#include "SerializeBuffer.hh"
#include "StringOp.hh"
#include "XMLElement.hh"
#include "XMLOutputStream.hh"
#include "MemBuffer.hh"
#include "hash_map.hh"
#include "inline.hh"
#include "strCat.hh"
#include "unreachable.hh"
#include "zstring_view.hh"
#include <zlib.h>
#include <array>
#include <cassert>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <typeindex>
#include <type_traits>
#include <vector>

namespace openmsx {

class LastDeltaBlocks;
class DeltaBlock;

// TODO move somewhere in utils once we use this more often
struct HashPair {
	template<typename T1, typename T2>
	size_t operator()(const std::pair<T1, T2>& p) const {
		return 31 * std::hash<T1>()(p.first) +
			    std::hash<T2>()(p.second);
	}
};


template<typename T> struct SerializeClassVersion;

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
// Archive code is heavily templatized. It uses the CRTP (curiously recurring
// template pattern ; a base class templatized on it's derived class). To
// implement static polymorphism. This means there is practically no run-time
// overhead of using this mechanism compared to 6 separately hand coded functions
// (Mem/XML/Text x input/output).
// TODO At least in theory, still need to verify this in practice.
//      Though my experience is that gcc is generally very good in this.

template<typename Derived> class ArchiveBase
{
public:
	/** Is this archive a loader or a saver.
	static constexpr bool IS_LOADER = ...;*/

	/** Serialize the base class of this classtype.
	 * Should preferably be called as the first statement in the
	 * implementation of a serialize() method of a class type.
	 * See also serializeInlinedBase() below.
	 */
	template<typename Base, typename T>
	void serializeBase(T& t)
	{
		const char* tag = BaseClassName<Base>::getName();
		self().serialize(tag, static_cast<Base&>(t));
	}

	/** Serialize the base class of this classtype.
	 * Should preferably be called as the first statement in the
	 * implementation of a serialize() method of a class type.
	 * See also serializeBase() above.
	 *
	 * The difference between serializeBase() and serializeInlinedBase()
	 * is only relevant for versioned archives (see NEED_VERSION, e.g.
	 * XML archives). In XML archives serializeBase() will put the base
	 * class in a new subtag, serializeInlinedBase() puts the members
	 * of the base class (inline) in the current tag. The advantage
	 * of serializeBase() is that the base class can be versioned
	 * separately from the subclass. The disadvantage is that it exposes
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
	// interface is not explicitly visible in the base class.
	//
	//
	// template<typename T> void serializeWithID(const char* tag, const T& t, ...)
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
	// void serialize_blob(const char* tag, std::span<uint8_t> data, bool diff = true)
	//
	//   Serialize the given data as a binary blob.
	//   This cannot be part of the serialize() method above because we
	//   cannot know whether a byte-array should be serialized as a blob
	//   or as a collection of bytes (IOW we cannot decide it based on the
	//   type).

	template<typename T>
	void serialize_blob(const char* tag, std::span<T> data, bool diff = true)
	{
		self().serialize_blob(tag, std::span<uint8_t>{static_cast<uint8_t*>(data.data()), data.size_bytes()}, diff);
	}
	template<typename T>
	void serialize_blob(const char* tag, std::span<const T> data, bool diff = true)
	{
		self().serialize_blob(tag, std::span<uint8_t>{static_cast<const uint8_t*>(data.data()), data.size_bytes()}, diff);
	}

	//
	//
	// template<typename T> void serialize(const char* tag, const T& t)
	//
	//   This is much like the serializeWithID() method above, but it doesn't
	//   store an ID with this element. This means that it's not possible,
	//   later on in the stream, to refer to this element. For many elements
	//   you know this will not happen. This method results in a slightly
	//   more compact stream.
	//
	//   Note that for primitive types we already don't store an ID, because
	//   pointers to primitive types are not supported (at least not ATM).
	//
	//
	// template<typename T, typename ...Args>
	// void serialize(const char* tag, const T& t, Args&& ...args)
	//
	//   Optionally serialize() accepts more than one tag-variable pair.
	//   This does conceptually the same as repeated calls to serialize()
	//   with each time a single pair, but it might be more efficient. E.g.
	//   the MemOutputArchive implementation is more efficient when called
	//   with multiple simple types.
	//
	// template<typename T> void serializePointerID(const char* tag, const T& t)
	//
	//   Serialize a pointer by storing the ID of the object it points to.
	//   This only works if the object was already serialized. The only
	//   reason to use this method instead of the more general serialize()
	//   method is that this one does not instantiate the object
	//   construction code. (So in some cases you can avoid having to
	//   provide specializations of SerializeConstructorArgs.)
	//
	//
	// template<typename T> void serializePolymorphic(const char* tag, const T& t)
	//
	//   Serialize a value-type whose concrete type is not yet known at
	//   compile-time (polymorphic pointers are already handled by the
	//   generic serialize() method).
	//
	//   The difference between pointer and value-types is that for
	//   pointers, the de-serialize code also needs to construct the
	//   object, while for value-types, the object (with the correct
	//   concrete type) is already constructed, it only needs to be
	//   initialized.
	//
	//
	// bool versionAtLeast(unsigned actual, unsigned required) const
	// bool versionBelow  (unsigned actual, unsigned required) const
	//
	//   Checks whether the actual version is respective 'bigger or equal'
	//   or 'strictly lower' than the required version. So in fact these are
	//   equivalent to respectively:
	//       return actual >= required;
	//       return actual <  required;
	//   Note that these two methods are the exact opposite of each other.
	//   Though for memory-archives and output-archives we know that the
	//   actual version is always equal to the latest class version and the
	//   required version can never be bigger than this latest version, so
	//   in these cases the methods can be optimized to respectively:
	//       return true;
	//       return false;
	//   By using these methods instead of direct comparisons, the compiler
	//   is able to perform more dead-code-elimination.

/*internal*/
	// These must be public for technical reasons, but they should only
	// be used by the serialization framework.

	/** Does this archive store version information. */
	static constexpr bool NEED_VERSION = true;

	/** Is this a reverse-snapshot? */
	[[nodiscard]] bool isReverseSnapshot() const { return false; }

	/** Does this archive store enums as strings.
	 * See also struct serialize_as_enum.
	 */
	static constexpr bool TRANSLATE_ENUM_TO_STRING = false;

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
	void attribute(const char* name, const char* value);

	/** Some archives (like XML archives) can store optional attributes.
	 * This method indicates whether that's the case or not.
	 * This can be used to for example in XML files don't store attributes
	 * with default values (thus to make the XML look prettier).
	 */
	static constexpr bool CAN_HAVE_OPTIONAL_ATTRIBUTES = false;

	/** Check the presence of a (optional) attribute.
	 * It's only allowed to call this method on archives that can have
	 * optional attributes.
	 */
	[[nodiscard]] bool hasAttribute(const char* /*name*/) const
	{
		UNREACHABLE;
	}

	/** Optimization: combination of hasAttribute() and getAttribute().
	 * Returns true if hasAttribute() and (if so) also fills in the value.
	 */
	template<typename T>
	[[nodiscard]] std::optional<T> findAttributeAs(const char* /*name*/)
	{
		UNREACHABLE;
	}

	/** Some archives (like XML archives) can count the number of subtags
	 * that belong to the current tag. This method indicates whether that's
	 * the case for this archive or not.
	 * This can for example be used to make the XML files look prettier in
	 * case of serialization of collections: in that case we don't need to
	 * explicitly store the size of the collection, it can be derived from
	 * the number of subtags.
	 */
	static constexpr bool CAN_COUNT_CHILDREN = false;

	/** Count the number of child tags.
	 * It's only allowed to call this method on archives that have support
	 * for this operation.
	 */
	[[nodiscard]] int countChildren() const
	{
		UNREACHABLE;
	}

	/** Indicate begin of a tag.
	 * Only XML archives use this, other archives ignore it.
	 * XML saver uses it as a name for the current tag, it doesn't
	 * interpret the name in any way.
	 * XML loader uses it only as a check: it checks whether the current
	 * tag name matches the given name. So we will NOT search the tag
	 * with the given name, the tags have to be in the correct order.
	 */
	void beginTag(const char* /*tag*/) const
	{
		// nothing
	}
	/** Indicate begin of a tag.
	 * Only XML archives use this, other archives ignore it.
	 * Both XML loader and saver only use the given tag name to do some
	 * internal checks (with checks disabled, the tag parameter has no
	 * influence at all on loading or saving of the stream).
	 */
	void endTag(const char* /*tag*/) const
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
	//   Often this method will be overloaded to handle certain types in a
	//   specific way.
	//
	//
	// template<typename T> void load(T& t)
	//
	//   Should only be implemented for InputArchives. This is similar (but
	//   opposite) to the save() method above. Loading of primitive types
	//   is done via this method.

	// void beginSection()
	// void endSection()
	// void skipSection(bool skip)
	//   The methods beginSection() and endSection() can only be used in
	//   output archives. These mark the location of a section that can
	//   later be skipped during loading.
	//   The method skipSection() can only be used in input archives. It
	//   optionally skips a section that was marked during saving.
	//   For every beginSection() call in the output, there must be a
	//   corresponding skipSection() call in the input (even if you don't
	//   actually want to skip the section).

protected:
	/** Returns a reference to the most derived class.
	 * Helper function to implement static polymorphism.
	 */
	inline Derived& self()
	{
		return static_cast<Derived&>(*this);
	}
};

// The part of OutputArchiveBase that doesn't depend on the template parameter
class OutputArchiveBase2
{
public:
	static constexpr bool IS_LOADER = false;
	[[nodiscard]] inline bool versionAtLeast(unsigned /*actual*/, unsigned /*required*/) const
	{
		return true;
	}
	[[nodiscard]] inline bool versionBelow(unsigned /*actual*/, unsigned /*required*/) const
	{
		return false;
	}

	void skipSection(bool /*skip*/) const
	{
		UNREACHABLE;
	}

/*internal*/
	#ifdef linux
	// This routine is not portable, for example it breaks in
	// windows (mingw) because there the location of the stack
	// is _below_ the heap.
	// But this is anyway only used to check assertions. So for now
	// only do that in linux.
	[[nodiscard]] static NEVER_INLINE bool addressOnStack(const void* p)
	{
		// This is not portable, it assumes:
		//  - stack grows downwards
		//  - heap is at lower address than stack
		// Also in c++ comparison between pointers is only defined when
		// the two pointers point to objects in the same array.
		int dummy;
		return &dummy < p;
	}
	#endif

	// Generate a new ID for the given pointer and store this association
	// for later (see getId()).
	template<typename T> [[nodiscard]] unsigned generateId(const T* p)
	{
		// For composed structures, for example
		//   struct A { ... };
		//   struct B { A a; ... };
		// The pointer to the outer and inner structure can be the
		// same while we still want a different ID to refer to these
		// two. That's why we use a std::pair<void*, type_index> as key
		// in the map.
		// For polymorphic types you do sometimes use a base pointer
		// to refer to a subtype. So there we only use the pointer
		// value as key in the map.
		if constexpr (std::is_polymorphic_v<T>) {
			return generateID1(p);
		} else {
			return generateID2(p, typeid(T));
		}
	}

	template<typename T> [[nodiscard]] unsigned getId(const T* p)
	{
		if constexpr (std::is_polymorphic_v<T>) {
			return getID1(p);
		} else {
			return getID2(p, typeid(T));
		}
	}

protected:
	OutputArchiveBase2() = default;

private:
	[[nodiscard]] unsigned generateID1(const void* p);
	[[nodiscard]] unsigned generateID2(const void* p, const std::type_info& typeInfo);
	[[nodiscard]] unsigned getID1(const void* p);
	[[nodiscard]] unsigned getID2(const void* p, const std::type_info& typeInfo);

private:
	hash_map<std::pair<const void*, std::type_index>, unsigned, HashPair> idMap;
	hash_map<const void*, unsigned> polyIdMap;
	unsigned lastId = 0;
};

template<typename Derived>
class OutputArchiveBase : public ArchiveBase<Derived>, public OutputArchiveBase2
{
public:
	template<typename Base, typename T>
	void serializeInlinedBase(T& t, unsigned version)
	{
		// same implementation as base class, but with extra check
		static_assert(SerializeClassVersion<Base>::value ==
		              SerializeClassVersion<T   >::value,
		              "base and derived must have same version when "
		              "using serializeInlinedBase()");
		ArchiveBase<Derived>::template serializeInlinedBase<Base>(t, version);
	}
	// Main saver method. Heavy lifting is done in the Saver class.
	// The 'global constructor arguments' parameters are ignored because
	// the saver archives also completely ignore those extra parameters.
	// But we need to provide them because the same (templatized) code path
	// is used both for saving and loading.
	template<typename T, typename... Args>
	void serializeWithID(const char* tag, const T& t, Args... /*globalConstrArgs*/)
	{
		this->self().beginTag(tag);
		Saver<T> saver;
		saver(this->self(), t, true);
		this->self().endTag(tag);
	}

	// Default implementation is to base64-encode the blob and serialize
	// the resulting string. But memory archives will memcpy the blob.
	void serialize_blob(const char* tag, std::span<const uint8_t> data,
	                    bool diff = true);

	template<typename T> void serialize(const char* tag, const T& t)
	{
		this->self().beginTag(tag);
		Saver<T> saver;
		saver(this->self(), t, false);
		this->self().endTag(tag);
	}
	template<typename T> void serializePointerID(const char* tag, const T& t)
	{
		this->self().beginTag(tag);
		IDSaver<T> saver;
		saver(this->self(), t);
		this->self().endTag(tag);
	}
	template<typename T> void serializePolymorphic(const char* tag, const T& t)
	{
		static_assert(std::is_polymorphic_v<T>,
		              "must be a polymorphic type");
		PolymorphicSaverRegistry<Derived>::save(tag, this->self(), t);
	}
	template<typename T> void serializeOnlyOnce(const char* tag, const T& t)
	{
		if (!getId(&t)) {
			serializeWithID(tag, t);
		}
	}

	// You shouldn't use this, it only exists for backwards compatibility
	void serializeChar(const char* tag, char c)
	{
		this->self().beginTag(tag);
		this->self().saveChar(c);
		this->self().endTag(tag);
	}

protected:
	OutputArchiveBase() = default;
};


// Part of InputArchiveBase that doesn't depend on the template parameter
class InputArchiveBase2
{
public:
	static constexpr bool IS_LOADER = true;

	void beginSection() const
	{
		UNREACHABLE;
	}
	void endSection() const
	{
		UNREACHABLE;
	}

/*internal*/
	[[nodiscard]] void* getPointer(unsigned id);
	void addPointer(unsigned id, const void* p);
	[[nodiscard]] unsigned getId(const void* p) const;

	template<typename T> void resetSharedPtr(std::shared_ptr<T>& s, T* r)
	{
		if (!r) {
			s.reset();
			return;
		}
		auto it = sharedPtrMap.find(r);
		if (it == end(sharedPtrMap)) {
			s.reset(r);
			sharedPtrMap.emplace_noDuplicateCheck(r, s);
		} else {
			s = std::static_pointer_cast<T>(it->second);
		}
	}

protected:
	InputArchiveBase2() = default;

private:
	hash_map<unsigned, void*> idMap;
	hash_map<void*, std::shared_ptr<void>> sharedPtrMap;
};

template<typename Derived>
class InputArchiveBase : public ArchiveBase<Derived>, public InputArchiveBase2
{
public:
	template<typename T, typename... Args>
	void serializeWithID(const char* tag, T& t, Args... args)
	{
		doSerialize(tag, t, std::tuple<Args...>(args...));
	}
	void serialize_blob(const char* tag, std::span<uint8_t> data,
	                    bool diff = true);

	template<typename T>
	void serialize(const char* tag, T& t)
	{
		this->self().beginTag(tag);
		using TNC = std::remove_const_t<T>;
		auto& tnc = const_cast<TNC&>(t);
		Loader<TNC> loader;
		loader(this->self(), tnc, std::tuple<>(), -1); // don't load id
		this->self().endTag(tag);
	}
	template<typename T> void serializePointerID(const char* tag, const T& t)
	{
		this->self().beginTag(tag);
		using TNC = std::remove_const_t<T>;
		auto& tnc = const_cast<TNC&>(t);
		IDLoader<TNC> loader;
		loader(this->self(), tnc);
		this->self().endTag(tag);
	}
	template<typename T> void serializePolymorphic(const char* tag, T& t)
	{
		static_assert(std::is_polymorphic_v<T>,
		              "must be a polymorphic type");
		PolymorphicInitializerRegistry<Derived>::init(tag, this->self(), &t);
	}
	template<typename T> void serializeOnlyOnce(const char* tag, const T& t)
	{
		if (!getId(&t)) {
			serializeWithID(tag, t);
		}
	}

	// You shouldn't use this, it only exists for backwards compatibility
	void serializeChar(const char* tag, char& c)
	{
		this->self().beginTag(tag);
		this->self().loadChar(c);
		this->self().endTag(tag);
	}

/*internal*/
	// Actual loader method. Heavy lifting is done in the Loader class.
	template<typename T, typename TUPLE>
	void doSerialize(const char* tag, T& t, TUPLE args, int id = 0)
	{
		this->self().beginTag(tag);
		using TNC = std::remove_const_t<T>;
		auto& tnc = const_cast<TNC&>(t);
		Loader<TNC> loader;
		loader(this->self(), tnc, args, id);
		this->self().endTag(tag);
	}

protected:
	InputArchiveBase() = default;
};


// Enumerate all types which can be serialized using a simple memcpy. This
// trait can be used by MemOutputArchive to apply certain optimizations.
template<typename T> struct SerializeAsMemcpy : std::false_type {};
template<> struct SerializeAsMemcpy<         bool     > : std::true_type {};
template<> struct SerializeAsMemcpy<         char     > : std::true_type {};
template<> struct SerializeAsMemcpy<  signed char     > : std::true_type {};
template<> struct SerializeAsMemcpy<unsigned char     > : std::true_type {};
template<> struct SerializeAsMemcpy<         short    > : std::true_type {};
template<> struct SerializeAsMemcpy<unsigned short    > : std::true_type {};
template<> struct SerializeAsMemcpy<         int      > : std::true_type {};
template<> struct SerializeAsMemcpy<unsigned int      > : std::true_type {};
template<> struct SerializeAsMemcpy<         long     > : std::true_type {};
template<> struct SerializeAsMemcpy<unsigned long     > : std::true_type {};
template<> struct SerializeAsMemcpy<         long long> : std::true_type {};
template<> struct SerializeAsMemcpy<unsigned long long> : std::true_type {};
template<> struct SerializeAsMemcpy<         float    > : std::true_type {};
template<> struct SerializeAsMemcpy<         double   > : std::true_type {};
template<> struct SerializeAsMemcpy<    long double   > : std::true_type {};
template<typename T, size_t N> struct SerializeAsMemcpy<std::array<T, N>> : SerializeAsMemcpy<T> {};

class MemOutputArchive final : public OutputArchiveBase<MemOutputArchive>
{
public:
	MemOutputArchive(LastDeltaBlocks& lastDeltaBlocks_,
	                 std::vector<std::shared_ptr<DeltaBlock>>& deltaBlocks_,
			 bool reverseSnapshot_)
		: lastDeltaBlocks(lastDeltaBlocks_)
		, deltaBlocks(deltaBlocks_)
		, reverseSnapshot(reverseSnapshot_)
	{
	}

	~MemOutputArchive()
	{
		assert(openSections.empty());
	}

	static constexpr bool NEED_VERSION = false;
	[[nodiscard]] bool isReverseSnapshot() const { return reverseSnapshot; }

	template<typename T> void save(const T& t)
	{
		buffer.insert(&t, sizeof(t));
	}
	inline void saveChar(char c)
	{
		save(c);
	}
	void save(const std::string& s) { save(std::string_view(s)); }
	void save(std::string_view s);
	void serialize_blob(const char* tag, std::span<const uint8_t> data,
	                    bool diff = true);

	using OutputArchiveBase<MemOutputArchive>::serialize;
	template<typename T, typename ...Args>
	ALWAYS_INLINE void serialize(const char* tag, const T& t, Args&& ...args)
	{
		// - Walk over all elements. Process non-memcpy-able elements at
		//   once. Collect memcpy-able elements in a tuple. At the end
		//   process the collected tuple with a single call.
		// - Only do this when there are at least two pairs (it is
		//   correct for a single pair, but it's less tuned for that
		//   case).
		serialize_group(std::tuple<>(), tag, t, std::forward<Args>(args)...);
	}
	template<typename T, size_t N>
	ALWAYS_INLINE void serialize(const char* /*tag*/, const std::array<T, N>& t)
		requires(SerializeAsMemcpy<T>::value)
	{
		buffer.insert(t.data(), N * sizeof(T));
	}

	void beginSection()
	{
		size_t skip = 0; // filled in later
		save(skip);
		size_t beginPos = buffer.getPosition();
		openSections.push_back(beginPos);
	}
	void endSection()
	{
		assert(!openSections.empty());
		size_t endPos   = buffer.getPosition();
		size_t beginPos = openSections.back();
		openSections.pop_back();
		size_t skip = endPos - beginPos;
		buffer.insertAt(beginPos - sizeof(skip),
		                &skip, sizeof(skip));
	}

	[[nodiscard]] MemBuffer<uint8_t> releaseBuffer() &&
	{
		return std::move(buffer).release();
	}

private:
	ALWAYS_INLINE void serialize_group(const std::tuple<>& /*tuple*/) const
	{
		// done categorizing, there were no memcpy-able elements
	}
	template<typename ...Args>
	ALWAYS_INLINE void serialize_group(const std::tuple<Args...>& tuple)
	{
		// done categorizing, process all memcpy-able elements
		buffer.insert_tuple_ptr(tuple);
	}
	template<typename TUPLE, typename T, typename ...Args>
	ALWAYS_INLINE void serialize_group(const TUPLE& tuple, const char* tag, const T& t, Args&& ...args)
	{
		// categorize one element
		if constexpr (SerializeAsMemcpy<T>::value) {
			// add to the group and continue categorizing
			(void)tag;
			serialize_group(std::tuple_cat(tuple, std::tuple(&t)), std::forward<Args>(args)...);
		} else {
			serialize(tag, t);      // process single (ungroupable) element
			serialize_group(tuple, std::forward<Args>(args)...); // continue categorizing
		}
	}

private:
	OutputBuffer buffer;
	std::vector<size_t> openSections;
	LastDeltaBlocks& lastDeltaBlocks;
	std::vector<std::shared_ptr<DeltaBlock>>& deltaBlocks;
	const bool reverseSnapshot;
};

class MemInputArchive final : public InputArchiveBase<MemInputArchive>
{
public:
	MemInputArchive(std::span<const uint8_t> buf_,
	                std::span<const std::shared_ptr<DeltaBlock>> deltaBlocks_)
		: buffer(buf_)
		, deltaBlocks(deltaBlocks_)
	{
	}

	static constexpr bool NEED_VERSION = false;
	[[nodiscard]] inline bool versionAtLeast(unsigned /*actual*/, unsigned /*required*/) const
	{
		return true;
	}
	[[nodiscard]] inline bool versionBelow(unsigned /*actual*/, unsigned /*required*/) const
	{
		return false;
	}

	template<typename T> void load(T& t)
	{
		buffer.read(&t, sizeof(t));
	}
	inline void loadChar(char& c)
	{
		load(c);
	}
	void load(std::string& s);
	[[nodiscard]] std::string_view loadStr();
	void serialize_blob(const char* tag, std::span<uint8_t> data,
	                    bool diff = true);

	using InputArchiveBase<MemInputArchive>::serialize;
	template<typename T, typename ...Args>
	ALWAYS_INLINE void serialize(const char* tag, T& t, Args&& ...args)
	{
		// see comments in MemOutputArchive
		serialize_group(std::tuple<>(), tag, t, std::forward<Args>(args)...);
	}

	template<typename T, size_t N>
	ALWAYS_INLINE void serialize(const char* /*tag*/, std::array<T, N>& t)
		requires(SerializeAsMemcpy<T>::value)
	{
		buffer.read(t.data(), N * sizeof(T));
	}

	void skipSection(bool skip)
	{
		size_t num;
		load(num);
		if (skip) {
			buffer.skip(num);
		}
	}

private:
	// See comments in MemOutputArchive
	template<typename TUPLE>
	ALWAYS_INLINE void serialize_group(const TUPLE& tuple)
	{
		auto read = [&](auto* p) { buffer.read(p, sizeof(*p)); };
		std::apply([&](auto&&... args) { (read(args), ...); }, tuple);
	}
	template<typename TUPLE, typename T, typename ...Args>
	ALWAYS_INLINE void serialize_group(const TUPLE& tuple, const char* tag, T& t, Args&& ...args)
	{
		if constexpr (SerializeAsMemcpy<T>::value) {
			(void)tag;
			serialize_group(std::tuple_cat(tuple, std::tuple(&t)), std::forward<Args>(args)...);
		} else {
			serialize(tag, t);
			serialize_group(tuple, std::forward<Args>(args)...);
		}
	}

private:
	InputBuffer buffer;
	std::span<const std::shared_ptr<DeltaBlock>> deltaBlocks;
};

////

class XmlOutputArchive final : public OutputArchiveBase<XmlOutputArchive>
{
public:
	explicit XmlOutputArchive(zstring_view filename);
	void close();
	~XmlOutputArchive();

	template<typename T> void saveImpl(const T& t)
	{
		// TODO make sure floating point is printed with enough digits
		//      maybe print as hex?
		save(std::string_view(tmpStrCat(t)));
	}
	template<typename T> void save(const T& t)
	{
		saveImpl(t);
	}
	void saveChar(char c);
	void save(std::string_view str);
	void save(bool b);
	void save(unsigned char b);
	void save(signed char c);
	void save(char c);
	void save(int i);                  // these 3 are not strictly needed
	void save(unsigned u);             // but having them non-inline
	void save(unsigned long long ull); // saves quite a bit of code

	void beginSection() const { /*nothing*/ }
	void endSection()   const { /*nothing*/ }

	// workaround(?) for visual studio 2015:
	//   put the default here instead of in the base class
	using OutputArchiveBase<XmlOutputArchive>::serialize;
	template<typename T, typename ...Args>
	ALWAYS_INLINE void serialize(const char* tag, const T& t, Args&& ...args)
	{
		// by default just repeatedly call the single-pair serialize() variant
		this->self().serialize(tag, t);
		this->self().serialize(std::forward<Args>(args)...);
	}

	auto& getXMLOutputStream() { return writer; }

//internal:
	static constexpr bool TRANSLATE_ENUM_TO_STRING = true;
	static constexpr bool CAN_HAVE_OPTIONAL_ATTRIBUTES = true;
	static constexpr bool CAN_COUNT_CHILDREN = true;

	void beginTag(const char* tag);
	void endTag(const char* tag);

	template<typename T> void attributeImpl(const char* name, const T& t)
	{
		attribute(name, std::string_view(tmpStrCat(t)));
	}
	template<typename T> void attribute(const char* name, const T& t)
	{
		attributeImpl(name, t);
	}
	void attribute(const char* name, std::string_view str);
	void attribute(const char* name, int i);
	void attribute(const char* name, unsigned u);

//internal: // called from XMLOutputStream
	void write(std::span<const char> buf);
	void write1(char c);
	void check(bool condition) const;
	[[noreturn]] void error();

private:
	zstring_view filename;
	gzFile file = nullptr;
	XMLOutputStream<XmlOutputArchive> writer;
};

class XmlInputArchive final : public InputArchiveBase<XmlInputArchive>
{
public:
	explicit XmlInputArchive(const std::string& filename);

	[[nodiscard]] inline bool versionAtLeast(unsigned actual, unsigned required) const
	{
		return actual >= required;
	}
	[[nodiscard]] inline bool versionBelow(unsigned actual, unsigned required) const
	{
		return actual < required;
	}

	void loadChar(char& c) const;
	void load(bool& b) const;
	void load(unsigned char& b) const;
	void load(signed char& c) const;
	void load(char& c) const;
	void load(int& i) const;                  // these 3 are not strictly needed
	void load(unsigned& u) const;             // but having them non-inline
	void load(unsigned long long& ull) const; // saves quite a bit of code
	void load(std::string& t) const;
	template<typename T> void load(T& t) const
	{
		std::string str;
		load(str);
		std::istringstream is(str);
		is >> t;
	}
	[[nodiscard]] std::string_view loadStr() const;

	void skipSection(bool /*skip*/) const { /*nothing*/ }

	// workaround(?) for visual studio 2015:
	//   put the default here instead of in the base class
	using InputArchiveBase<XmlInputArchive>::serialize;
	template<typename T, typename ...Args>
	ALWAYS_INLINE void serialize(const char* tag, T& t, Args&& ...args)
	{
		// by default just repeatedly call the single-pair serialize() variant
		this->self().serialize(tag, t);
		this->self().serialize(std::forward<Args>(args)...);
	}

	[[nodiscard]] const XMLElement* currentElement() const {
		return elems.back().first;
	}

//internal:
	static constexpr bool TRANSLATE_ENUM_TO_STRING = true;
	static constexpr bool CAN_HAVE_OPTIONAL_ATTRIBUTES = true;
	static constexpr bool CAN_COUNT_CHILDREN = true;

	void beginTag(const char* tag);
	void endTag(const char* tag);

	template<typename T> void attributeImpl(const char* name, T& t) const
	{
		std::string str;
		attribute(name, str);
		std::istringstream is(str);
		is >> t;
	}
	template<typename T> void attribute(const char* name, T& t)
	{
		attributeImpl(name, t);
	}
	void attribute(const char* name, std::string& t) const;
	void attribute(const char* name, int& i) const;
	void attribute(const char* name, unsigned& u) const;

	[[nodiscard]] bool hasAttribute(const char* name) const;
	[[nodiscard]] int countChildren() const;

	template<typename T>
	std::optional<T> findAttributeAs(const char* name)
	{
		if (const auto* attr = currentElement()->findAttribute(name)) {
			return StringOp::stringTo<T>(attr->getValue());
		}
		return {};
	}

private:
	XMLDocument xmlDoc{16384}; // tweak: initial allocator buffer size
	std::vector<std::pair<const XMLElement*, const XMLElement*>> elems;
};

#define INSTANTIATE_SERIALIZE_METHODS(CLASS) \
template void CLASS::serialize(MemInputArchive&,   unsigned); \
template void CLASS::serialize(MemOutputArchive&,  unsigned); \
template void CLASS::serialize(XmlInputArchive&,   unsigned); \
template void CLASS::serialize(XmlOutputArchive&,  unsigned);

} // namespace openmsx

#endif
