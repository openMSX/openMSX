// $Id$

// Make a class non-inheritable (a final class). When you do inherit from
// a non-inheritable class and you try to instantiate an object of this class,
// you get a compilation error
// 
// Usage: 
//    class Foo : NON_INHERITABLE(Foo) { ... };
//
// There is a small size penalty when you use this feature. However when
// the NDEBUG macro is defined the checking is disabled (and the penalty
// is gone).


#ifndef __NON_INHERITABLE_HH__
#define __NON_INHERITABLE_HH__


#ifndef NDEBUG

//
// Development version, performs checking at the cost of increasing the object
// size with one pointer (4 bytes).
//

#define NON_INHERITABLE(T) virtual private NonInheritable<T>

template <typename T>
class NonInheritable {
private:
  ~NonInheritable() { }
  
  // friend class T;   // doesn't work on gcc-3.x
  struct FriendMaker {
    typedef T FriendType;
  };
  friend class FriendMaker::FriendType;
};

#else

//
// Dummy version, no checking, no overhead.
//

#define NON_INHERITABLE(T) private NonInheritable

class NonInheritable {
};

#endif // NDEBUG


#endif // __NON_INHERITABLE_HH__
