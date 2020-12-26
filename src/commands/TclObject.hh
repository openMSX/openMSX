#ifndef TCLOBJECT_HH
#define TCLOBJECT_HH

#include "span.hh"
#include "vla.hh"
#include "xxhash.hh"
#include <tcl.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <string_view>

struct Tcl_Obj;

namespace openmsx {

class Interpreter;

class TclObject
{
	// For STL interface, see below
	struct iterator {
		using value_type        = std::string_view;
		using reference         = std::string_view;
		using pointer           = std::string_view*;
		using difference_type   = ptrdiff_t;
		using iterator_category = std::bidirectional_iterator_tag;

		iterator(const TclObject& obj_, unsigned i_)
			: obj(&obj_), i(i_) {}

		[[nodiscard]] bool operator==(const iterator& other) const {
			assert(obj == other.obj);
			return i == other.i;
		}
		[[nodiscard]] bool operator!=(const iterator& other) const {
			return !(*this == other);
		}

		[[nodiscard]] std::string_view operator*() const {
			return obj->getListIndexUnchecked(i).getString();
		}

		iterator& operator++() {
			++i;
			return *this;
		}
		iterator operator++(int) {
			iterator result = *this;
			++result;
			return result;
		}
		iterator& operator--() {
			--i;
			return *this;
		}
		iterator operator--(int) {
			iterator result = *this;
			--result;
			return result;
		}
	private:
		const TclObject* obj;
		unsigned i;
	};

public:

	TclObject()                                  { init(Tcl_NewObj()); }
	explicit TclObject(Tcl_Obj* o)               { init(o); }
	template<typename T> explicit TclObject(T t) { init(newObj(t)); }
	TclObject(const TclObject&  o)               { init(newObj(o)); }
	TclObject(      TclObject&& o) noexcept      { init(newObj(o)); }

	struct MakeListTag {};
	template<typename... Args>
	TclObject(MakeListTag, Args&&... args) {
		init(newList({newObj(std::forward<Args>(args))...}));
	}

	struct MakeDictTag {};
	template<typename... Args>
	TclObject(MakeDictTag, Args&&... args) {
		init(Tcl_NewDictObj());
		addDictKeyValues(std::forward<Args>(args)...);
	}

	~TclObject() { Tcl_DecrRefCount(obj); }

	// assignment operators
	TclObject& operator=(const TclObject& other) {
		if (&other != this) {
			Tcl_DecrRefCount(obj);
			init(other.obj);
		}
		return *this;
	}
	TclObject& operator=(TclObject&& other) noexcept {
		std::swap(obj, other.obj);
		return *this;
	}
	template<typename T>
	TclObject& operator=(T t) {
		if (Tcl_IsShared(obj)) {
			Tcl_DecrRefCount(obj);
			obj = newObj(t);
			Tcl_IncrRefCount(obj);
		} else {
			assign(t);
		}
		return *this;
	}

	// get underlying Tcl_Obj
	[[nodiscard]] Tcl_Obj* getTclObject() { return obj; }
	[[nodiscard]] Tcl_Obj* getTclObjectNonConst() const { return const_cast<Tcl_Obj*>(obj); }

	// add elements to a Tcl list
	template<typename T> void addListElement(T t) { addListElement(newObj(t)); }
	template<typename ITER> void addListElements(ITER first, ITER last) {
		addListElementsImpl(first, last,
		                typename std::iterator_traits<ITER>::iterator_category());
	}
	template<typename Range> void addListElements(Range&& range) {
		addListElements(std::begin(range), std::end(range));
	}
	template<typename... Args> void addListElement(Args&&... args) {
		addListElementsImpl({newObj(std::forward<Args>(args))...});
	}

	// add key-value pair(s) to a Tcl dict
	template<typename Key, typename Value>
	void addDictKeyValue(const Key& key, const Value& value) {
		addDictKeyValues({newObj(key), newObj(value)});
	}
	template<typename... Args> void addDictKeyValues(Args&&... args) {
		addDictKeyValues({newObj(std::forward<Args>(args))...});
	}

	// value getters
	[[nodiscard]] std::string_view getString() const;
	[[nodiscard]] int getInt      (Interpreter& interp) const;
	[[nodiscard]] bool getBoolean (Interpreter& interp) const;
	[[nodiscard]] double getDouble(Interpreter& interp) const;
	[[nodiscard]] span<const uint8_t> getBinary() const;
	[[nodiscard]] unsigned getListLength(Interpreter& interp) const;
	[[nodiscard]] TclObject getListIndex(Interpreter& interp, unsigned index) const;
	[[nodiscard]] TclObject getDictValue(Interpreter& interp, const TclObject& key) const;
	template<typename Key>
	[[nodiscard]] TclObject getDictValue(Interpreter& interp, const Key& key) const {
		return getDictValue(interp, TclObject(key));
	}
	[[nodiscard]] std::optional<int> getOptionalInt() const;

	// STL-like interface when interpreting this TclObject as a list of
	// strings. Invalid Tcl lists are silently interpreted as empty lists.
	[[nodiscard]] unsigned size() const { return getListLengthUnchecked(); }
	[[nodiscard]] bool empty() const { return size() == 0; }
	[[nodiscard]] auto begin() const { return iterator(*this, 0); }
	[[nodiscard]] auto end()   const { return iterator(*this, size()); }

	// expressions
	[[nodiscard]] bool evalBool(Interpreter& interp) const;

	/** Interpret this TclObject as a command and execute it.
	  * @param interp The Tcl interpreter
	  * @param compile Should the command be compiled to bytecode? The
	  *           bytecode is stored inside the TclObject can speed up
	  *           future invocations of the same command. Only set this
	  *           flag when the command will be executed more than once.
	  */
	TclObject executeCommand(Interpreter& interp, bool compile = false);

	[[nodiscard]] friend bool operator==(const TclObject& x, const TclObject& y) {
		return x.getString() == y.getString();
	}
	[[nodiscard]] friend bool operator==(const TclObject& x, std::string_view y) {
		return x.getString() == y;
	}
	[[nodiscard]] friend bool operator==(std::string_view x, const TclObject& y) {
		return x == y.getString();
	}

	[[nodiscard]] friend bool operator!=(const TclObject& x, const TclObject& y) { return !(x == y); }
	[[nodiscard]] friend bool operator!=(const TclObject& x, std::string_view y) { return !(x == y); }
	[[nodiscard]] friend bool operator!=(std::string_view x, const TclObject& y) { return !(x == y); }

private:
	void init(Tcl_Obj* obj_) noexcept {
		obj = obj_;
		Tcl_IncrRefCount(obj);
	}

	[[nodiscard]] static Tcl_Obj* newObj(std::string_view s) {
		return Tcl_NewStringObj(s.data(), int(s.size()));
	}
	[[nodiscard]] static Tcl_Obj* newObj(const char* s) {
		return Tcl_NewStringObj(s, int(strlen(s)));
	}
	[[nodiscard]] static Tcl_Obj* newObj(bool b) {
		return Tcl_NewBooleanObj(b);
	}
	[[nodiscard]] static Tcl_Obj* newObj(int i) {
		return Tcl_NewIntObj(i);
	}
	[[nodiscard]] static Tcl_Obj* newObj(unsigned u) {
		return Tcl_NewIntObj(u);
	}
	[[nodiscard]] static Tcl_Obj* newObj(float f) {
		return Tcl_NewDoubleObj(double(f));
	}
	[[nodiscard]] static Tcl_Obj* newObj(double d) {
		return Tcl_NewDoubleObj(d);
	}
	[[nodiscard]] static Tcl_Obj* newObj(span<const uint8_t> buf) {
		return Tcl_NewByteArrayObj(buf.data(), int(buf.size()));
	}
	[[nodiscard]] static Tcl_Obj* newObj(const TclObject& o) {
		return o.obj;
	}
	[[nodiscard]] static Tcl_Obj* newList(std::initializer_list<Tcl_Obj*> l) {
		return Tcl_NewListObj(int(l.size()), l.begin());
	}

	void assign(std::string_view s) {
		Tcl_SetStringObj(obj, s.data(), int(s.size()));
	}
	void assign(const char* s) {
		Tcl_SetStringObj(obj, s, int(strlen(s)));
	}
	void assign(bool b) {
		Tcl_SetBooleanObj(obj, b);
	}
	void assign(int i) {
		Tcl_SetIntObj(obj, i);
	}
	void assign(unsigned u) {
		Tcl_SetIntObj(obj, u);
	}
	void assign(float f) {
		Tcl_SetDoubleObj(obj, double(f));
	}
	void assign(double d) {
		Tcl_SetDoubleObj(obj, d);
	}
	void assign(span<const uint8_t> b) {
		Tcl_SetByteArrayObj(obj, b.data(), int(b.size()));
	}

	template<typename ITER>
	void addListElementsImpl(ITER first, ITER last, std::input_iterator_tag) {
		for (ITER it = first; it != last; ++it) {
			addListElement(*it);
		}
	}
	template<typename ITER>
	void addListElementsImpl(ITER first, ITER last, std::random_access_iterator_tag) {
		auto objc = last - first;
		VLA(Tcl_Obj*, objv, objc);
		std::transform(first, last, objv, [](const auto& t) { return newObj(t); });
		addListElementsImpl(objc, objv);
	}

	void addListElement(Tcl_Obj* element);
	void addListElementsImpl(int objc, Tcl_Obj* const* objv);
	void addListElementsImpl(std::initializer_list<Tcl_Obj*> l);
	void addDictKeyValues(std::initializer_list<Tcl_Obj*> keyValuePairs);
	[[nodiscard]] unsigned getListLengthUnchecked() const;
	[[nodiscard]] TclObject getListIndexUnchecked(unsigned index) const;

private:
	Tcl_Obj* obj;
};

// We want to be able to reinterpret_cast a Tcl_Obj* as a TclObject.
static_assert(sizeof(TclObject) == sizeof(Tcl_Obj*));

template<typename... Args>
[[nodiscard]] TclObject makeTclList(Args&&... args)
{
	return TclObject(TclObject::MakeListTag{}, std::forward<Args>(args)...);
}

template<typename... Args>
[[nodiscard]] TclObject makeTclDict(Args&&... args)
{
	return TclObject(TclObject::MakeDictTag{}, std::forward<Args>(args)...);
}

struct XXTclHasher {
	[[nodiscard]] uint32_t operator()(std::string_view str) const {
		return xxhash(str);
	}
	[[nodiscard]] uint32_t operator()(const TclObject& obj) const {
		return xxhash(obj.getString());
	}
};

} // namespace openmsx

#endif
