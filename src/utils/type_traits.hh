#ifndef TYPE_TRAITS_HH
#define TYPE_TRAITS_HH

// if_<C, T, F>
template<bool C, class T, class F> struct if_c             : F {};
template<        class T, class F> struct if_c<true, T, F> : T {};
template<typename C, typename T, typename F> struct if_ : if_c<C::value, T, F> {};

#endif
