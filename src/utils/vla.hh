// $Id:$

#ifndef VLA_HH
#define VLA_HH

// VLAs (Variable Length Array's) are part of C99, but not of C++98.
// Though gcc does support VLAs as an extension. For VC++ we have to emulate
// them via alloca.

#ifndef _MSC_VER

#define VLA(TYPE, NAME, LENGTH) \
	TYPE NAME[(LENGTH)]
#define VLA_ALIGNED(TYPE, NAME, LENGTH, ALIGNMENT) \
	TYPE NAME[(LENGTH)] __attribute__((__aligned__((ALIGNMENT))))

#else

#define VLA(TYPE, NAME, LENGTH) \
	TYPE* NAME = static_cast<TYPE*>(_alloca(sizeof(TYPE) * (LENGTH)))

// mfeingol: evil hack alert
#define VLA_ALIGNED(TYPE, NAME, LENGTH, ALIGNMENT)                               \
	size_t cbAlign = (ALIGNMENT);                                            \
	void* palloca = _alloca(sizeof(TYPE) * (LENGTH) + cbAlign);              \
	palloca = (void*)((size_t(palloca) + cbAlign - 1UL) & ~(cbAlign - 1UL)); \
	TYPE* NAME = static_cast<TYPE*>(palloca);                                \

#endif

#endif // VLA_HH
