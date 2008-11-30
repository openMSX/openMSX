#ifndef __MY_AUTO_PTR_HH__
#define __MY_AUTO_PTR_HH__
#include <algorithm>
#include <iostream>
#include <typeinfo>
#include <ctime>
#include <memory>
#include "openmsx.hh"

// This is debug code, you can use it to replace auto_ptr with, so that you
// can track when the destructor of the auto_ptr is called. It is certainly
// not 100% compatible with auto_ptr, you may need to use some temporary
// variables to get your code working again, after replacing auto_ptr with
// my_auto_ptr...
//
// PS: sorry for the ugly time stamping code.

template<typename T> class my_auto_ptr
{
public:
	my_auto_ptr(T* t_ = 0) : t(t_) {}
	my_auto_ptr(my_auto_ptr& other) : t(other.t) { other.t = 0; }
	~my_auto_ptr() { 
		time_t curr = std::time(0);
		if (t != NULL) PRT_DEBUG(std::ctime(&curr) << ": auto-destructing my_auto_ptr of type: " << typeid(t).name());
		delete t;
		curr = std::time(0);
	       	if (t != NULL) PRT_DEBUG(std::ctime(&curr) << "auto-destruction of my_auto_ptr of type " << typeid(t).name() << " done!"); 
	}
	//~my_auto_ptr() { t = 0; } // a version without actually deleting

	void reset(T* t_ = 0) { my_auto_ptr<T>(t_).swap(*this); }
	T* get() const { return t; }
	T* release() { T* res = t; t = 0; return res; }
	

	T& operator* () const { return *t; }
	T* operator->() const { return  t; }
	//my_auto_ptr& operator=(my_auto_ptr other) { swap(other); return *this; }
	my_auto_ptr& operator=(my_auto_ptr& other) { reset(other.release()); return *this; }
	template <typename T2> my_auto_ptr& operator=(my_auto_ptr<T2>& other) { reset(other.release()); return *this; }
	template <typename T2> my_auto_ptr& operator=(std::auto_ptr<T2>& other) { reset(other.release()); return *this; }

	void swap(my_auto_ptr& other) { std::swap(t, other.t); }

private:
	T* t;
};

#endif

//////////////////
/*
#include <cassert>

class A
{
public:
	int f() { return 42; }
};

int main()
{
	A* a = new A();
	my_auto_ptr<A> p1(a);
	assert(p1.get() == a);
	assert(&(*p1) == a);

	my_auto_ptr<A> p2(p1);
	assert(p1.get() == 0);
	assert(p2.get() == a);

	p1.swap(p2);
	assert(p1.get() == a);
	assert(p2.get() == 0);

	my_auto_ptr<A> p3;
	assert(p3.get() == 0);
	p3 = p1;
	assert(p1.get() == 0);
	assert(p3.get() == a);

	A* a2 = new A();
	p3.reset(a2);
	assert(p3.get() == a2);

	assert(p3->f() == 42);
}*/
