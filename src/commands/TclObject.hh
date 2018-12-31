#ifndef TCLOBJECT_HH
#define TCLOBJECT_HH

#include "string_view.hh"
#include "span.hh"
#include "xxhash.hh"
#include <tcl.h>
#include <iterator>
#include <cassert>
#include <cstdint>

struct Tcl_Obj;

namespace openmsx {

class Interpreter;

class TclObject
{
	// For STL interface, see below
	struct iterator {
		using value_type        = string_view;
		using reference         = string_view;
		using pointer           = string_view*;
		using difference_type   = ptrdiff_t;
		using iterator_category = std::bidirectional_iterator_tag;

		iterator(const TclObject& obj_, unsigned i_)
			: obj(&obj_), i(i_) {}

		bool operator==(const iterator& other) const {
			assert(obj == other.obj);
			return i == other.i;
		}
		bool operator!=(const iterator& other) const {
			return !(*this == other);
		}

		string_view operator*() const {
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
	TclObject()                               { init(Tcl_NewObj()); }
	explicit TclObject(Tcl_Obj* o)            { init(o); }
	explicit TclObject(string_view s)         { init(newObj(s)); }
	explicit TclObject(const char* s)         { init(newObj(s)); }
	explicit TclObject(bool b)                { init(newObj(b)); }
	explicit TclObject(int i)                 { init(newObj(i)); }
	explicit TclObject(double d)              { init(newObj(d)); }
	explicit TclObject(span<const uint8_t> b) { init(newObj(b)); }
	TclObject(const TclObject&  o)            { init(newObj(o)); }
	TclObject(      TclObject&& o) noexcept   { init(newObj(o)); }

	~TclObject() { Tcl_DecrRefCount(obj); }

	// assignment operator so we can use vector<TclObject>
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

	// get underlying Tcl_Obj
	Tcl_Obj* getTclObject() { return obj; }
	Tcl_Obj* getTclObjectNonConst() const { return const_cast<Tcl_Obj*>(obj); }

	// value setters
	void setString(string_view value);
	void setBoolean(bool value);
	void setInt(int value);
	void setDouble(double value);
	void setBinary(span<const uint8_t> buf);

	void addListElement(string_view s)         { addListElement(newObj(s)); }
	void addListElement(const char* s)         { addListElement(newObj(s)); }
	void addListElement(bool b)                { addListElement(newObj(b)); }
	void addListElement(int i)                 { addListElement(newObj(i)); }
	void addListElement(double d)              { addListElement(newObj(d)); }
	void addListElement(span<const uint8_t> b) { addListElement(newObj(b)); }
	void addListElement(const TclObject& o)    { addListElement(newObj(o)); }
	template<typename ITER> void addListElements(ITER first, ITER last);
	template<typename Range> void addListElements(Range&& range);

	// value getters
	string_view getString() const;
	int getInt      (Interpreter& interp) const;
	bool getBoolean (Interpreter& interp) const;
	double getDouble(Interpreter& interp) const;
	span<const uint8_t> getBinary() const;
	unsigned getListLength(Interpreter& interp) const;
	TclObject getListIndex(Interpreter& interp, unsigned index) const;
	TclObject getDictValue(Interpreter& interp, const TclObject& key) const;

	// STL-like interface when interpreting this TclObject as a list of
	// strings. Invalid Tcl lists are silently interpreted as empty lists.
	unsigned size() const { return getListLengthUnchecked(); }
	bool empty() const { return size() == 0; }
	auto begin() const { return iterator(*this, 0); }
	auto end()   const { return iterator(*this, size()); }

	// expressions
	bool evalBool(Interpreter& interp) const;

	/** Interpret this TclObject as a command and execute it.
	  * @param interp The Tcl interpreter
	  * @param compile Should the command be compiled to bytecode? The
	  *           bytecode is stored inside the TclObject can speed up
	  *           future invocations of the same command. Only set this
	  *           flag when the command will be executed more than once.
	  */
	TclObject executeCommand(Interpreter& interp, bool compile = false);

	friend bool operator==(const TclObject& x, const TclObject& y) {
		return x.getString() == y.getString();
	}
	friend bool operator==(const TclObject& x, string_view y) {
		return x.getString() == y;
	}
	friend bool operator==(string_view x, const TclObject& y) {
		return x == y.getString();
	}

	friend bool operator!=(const TclObject& x, const TclObject& y) { return !(x == y); }
	friend bool operator!=(const TclObject& x, string_view       y) { return !(x == y); }
	friend bool operator!=(string_view       x, const TclObject& y) { return !(x == y); }

private:
	void init(Tcl_Obj* obj_) noexcept {
		obj = obj_;
		Tcl_IncrRefCount(obj);
	}

	static Tcl_Obj* newObj(string_view s) {
		return Tcl_NewStringObj(s.data(), int(s.size()));
	}
	static Tcl_Obj* newObj(const char* s) {
		return Tcl_NewStringObj(s, strlen(s));
	}
	static Tcl_Obj* newObj(bool b) {
		return Tcl_NewBooleanObj(b);
	}
	static Tcl_Obj* newObj(int i) {
		return Tcl_NewIntObj(i);
	}
	static Tcl_Obj* newObj(double d) {
		return Tcl_NewDoubleObj(d);
	}
	static Tcl_Obj* newObj(span<const uint8_t> buf) {
		return Tcl_NewByteArrayObj(buf.data(), buf.size());
	}
	static Tcl_Obj* newObj(const TclObject& o) {
		return o.obj;
	}

	void addListElement(Tcl_Obj* element);
	unsigned getListLengthUnchecked() const;
	TclObject getListIndexUnchecked(unsigned index) const;

	Tcl_Obj* obj;
};

template <typename ITER>
void TclObject::addListElements(ITER first, ITER last)
{
	for (ITER it = first; it != last; ++it) {
		addListElement(*it);
	}
}

template <typename Range>
void TclObject::addListElements(Range&& range)
{
	addListElements(std::begin(range), std::end(range));
}

// We want to be able to reinterpret_cast a Tcl_Obj* as a TclObject.
static_assert(sizeof(TclObject) == sizeof(Tcl_Obj*), "");


struct XXTclHasher {
	uint32_t operator()(string_view str) const {
		return xxhash(str);
	}
	uint32_t operator()(const TclObject& obj) const {
		return xxhash(obj.getString());
	}
};

} // namespace openmsx

#endif
