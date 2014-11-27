#ifndef TCLOBJECT_HH
#define TCLOBJECT_HH

#include "string_ref.hh"
#include "openmsx.hh"
#include <iterator>
#include <cassert>

struct Tcl_Obj;

namespace openmsx {

class Interpreter;

class TclObject
{
	// For STL interface, see below
	struct iterator {
		iterator(const TclObject& obj_, unsigned i_)
			: obj(&obj_), i(i_) {}

		bool operator==(const iterator& other) const {
			assert(obj == other.obj);
			return i == other.i;
		}
		bool operator!=(const iterator& other) const {
			return !(*this == other);
		}

		string_ref operator*() const {
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
	TclObject();
	explicit TclObject(Tcl_Obj* object);
	explicit TclObject(string_ref value);
	explicit TclObject(int value);
	explicit TclObject(double value);
	TclObject(const TclObject& object);
	~TclObject();

	// assignment operator so we can use vector<TclObject>
	TclObject& operator=(const TclObject& other);

	// get underlying Tcl_Obj
	Tcl_Obj* getTclObject() { return obj; }

	// value setters
	void setString(string_ref value);
	void setInt(int value);
	void setBoolean(bool value);
	void setDouble(double value);
	void setBinary(byte* buf, unsigned length);
	void addListElement(string_ref element);
	void addListElement(int value);
	void addListElement(double value);
	void addListElement(const TclObject& element);
	template <typename ITER> void addListElements(ITER begin, ITER end);
	template <typename CONT> void addListElements(const CONT& container);

	// value getters
	string_ref getString() const;
	int getInt      (Interpreter& interp) const;
	bool getBoolean (Interpreter& interp) const;
	double getDouble(Interpreter& interp) const;
	const byte* getBinary(unsigned& length) const;
	unsigned getListLength(Interpreter& interp) const;
	TclObject getListIndex(Interpreter& interp, unsigned index) const;
	TclObject getDictValue(Interpreter& interp, const TclObject& key) const;

	// STL-like interface when interpreting this TclObject as a list of
	// strings. Invalid Tcl lists are silently interpreted as empty lists.
	unsigned size() const { return getListLengthUnchecked(); }
	bool empty() const { return size() == 0; }
	iterator begin() const { return iterator(*this, 0); }
	iterator end()   const { return iterator(*this, size()); }

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

	bool operator==(const TclObject& other) const {
		return getString() == other.getString();
	}
	bool operator!=(const TclObject& other) const {
		return !(*this == other);
	}

private:
	void init(Tcl_Obj* obj_);
	void addListElement(Tcl_Obj* element);
	unsigned getListLengthUnchecked() const;
	TclObject getListIndexUnchecked(unsigned index) const;

	Tcl_Obj* obj;
};

template <typename ITER>
void TclObject::addListElements(ITER begin, ITER end)
{
	for (ITER it = begin; it != end; ++it) {
		addListElement(*it);
	}
}

template <typename CONT>
void TclObject::addListElements(const CONT& container)
{
	addListElements(std::begin(container), std::end(container));
}

// We want to be able to reinterpret_cast a Tcl_Obj* as a TclObject.
static_assert(sizeof(TclObject) == sizeof(Tcl_Obj*), "");

} // namespace openmsx

#endif
