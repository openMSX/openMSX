#ifndef MEMORY_HH
#define MEMORY_HH

#include <memory>
#include <utility>

// See this blog for a motivation for the make_unique() function:
//    http://herbsutter.com/gotw/_102/
// This function will almost certainly be added in the next revision of the
// c++ language.


// The above blog also gives an implementation for make_unique(). Unfortunately
// vs2012 doesn't support variadic templates yet, so for now we have to use
// the longer (and less general) version below.
#if 0
template<typename T, typename ...Args>
std::unique_ptr<T> make_unique(Args&& ...args)
{
	return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#else

// Emulate variadic templates for upto 13 arguments (yes we need a version
// with 13 parameters!).
template<typename T>
std::unique_ptr<T> make_unique()
{
	return std::unique_ptr<T>(new T());
}

template<typename T, typename P1>
std::unique_ptr<T> make_unique(P1&& p1)
{
	return std::unique_ptr<T>(new T(
		std::forward<P1>(p1)));
}

template<typename T, typename P1, typename P2>
std::unique_ptr<T> make_unique(P1&& p1, P2&& p2)
{
	return std::unique_ptr<T>(new T(
		std::forward<P1>(p1), std::forward<P2>(p2)));
}

template<typename T, typename P1, typename P2, typename P3>
std::unique_ptr<T> make_unique(P1&& p1, P2&& p2, P3&& p3)
{
	return std::unique_ptr<T>(new T(
		std::forward<P1>(p1), std::forward<P2>(p2),
		std::forward<P3>(p3)));
}

template<typename T, typename P1, typename P2, typename P3, typename P4>
std::unique_ptr<T> make_unique(P1&& p1, P2&& p2, P3&& p3, P4&& p4)
{
	return std::unique_ptr<T>(new T(
		std::forward<P1>(p1), std::forward<P2>(p2),
		std::forward<P3>(p3), std::forward<P4>(p4)));
}

template<typename T, typename P1, typename P2, typename P3, typename P4,
                     typename P5>
std::unique_ptr<T> make_unique(P1&& p1, P2&& p2, P3&& p3, P4&& p4, P5&& p5)
{
	return std::unique_ptr<T>(new T(
		std::forward<P1>(p1), std::forward<P2>(p2),
		std::forward<P3>(p3), std::forward<P4>(p4),
		std::forward<P5>(p5)));
}

template<typename T, typename P1, typename P2, typename P3, typename P4,
                     typename P5, typename P6>
std::unique_ptr<T> make_unique(P1&& p1, P2&& p2, P3&& p3, P4&& p4, P5&& p5,
                               P6&& p6)
{
	return std::unique_ptr<T>(new T(
		std::forward<P1>(p1), std::forward<P2>(p2),
		std::forward<P3>(p3), std::forward<P4>(p4),
		std::forward<P5>(p5), std::forward<P6>(p6)));
}

template<typename T, typename P1, typename P2, typename P3, typename P4,
                     typename P5, typename P6, typename P7>
std::unique_ptr<T> make_unique(P1&& p1, P2&& p2, P3&& p3, P4&& p4, P5&& p5,
                               P6&& p6, P7&& p7)
{
	return std::unique_ptr<T>(new T(
		std::forward<P1>(p1), std::forward<P2>(p2),
		std::forward<P3>(p3), std::forward<P4>(p4),
		std::forward<P5>(p5), std::forward<P6>(p6),
		std::forward<P7>(p7)));
}

template<typename T, typename P1, typename P2, typename P3, typename P4,
                     typename P5, typename P6, typename P7, typename P8>
std::unique_ptr<T> make_unique(P1&& p1, P2&& p2, P3&& p3, P4&& p4, P5&& p5,
                               P6&& p6, P7&& p7, P8&& p8)
{
	return std::unique_ptr<T>(new T(
		std::forward<P1>(p1), std::forward<P2>(p2),
		std::forward<P3>(p3), std::forward<P4>(p4),
		std::forward<P5>(p5), std::forward<P6>(p6),
		std::forward<P7>(p7), std::forward<P8>(p8)));
}

template<typename T, typename P1, typename P2, typename P3, typename P4,
                     typename P5, typename P6, typename P7, typename P8,
                     typename P9>
std::unique_ptr<T> make_unique(P1&& p1, P2&& p2, P3&& p3, P4&& p4, P5&& p5,
                               P6&& p6, P7&& p7, P8&& p8, P9&& p9)
{
	return std::unique_ptr<T>(new T(
		std::forward<P1>(p1), std::forward<P2>(p2),
		std::forward<P3>(p3), std::forward<P4>(p4),
		std::forward<P5>(p5), std::forward<P6>(p6),
		std::forward<P7>(p7), std::forward<P8>(p8),
		std::forward<P9>(p9)));
}

template<typename T, typename P1, typename P2, typename P3, typename P4,
                     typename P5, typename P6, typename P7, typename P8,
                     typename P9, typename P10>
std::unique_ptr<T> make_unique(P1&& p1, P2&& p2, P3&& p3, P4&& p4, P5&&  p5,
                               P6&& p6, P7&& p7, P8&& p8, P9&& p9, P10&& p10)
{
	return std::unique_ptr<T>(new T(
		std::forward<P1>(p1), std::forward<P2 >(p2 ),
		std::forward<P3>(p3), std::forward<P4 >(p4 ),
		std::forward<P5>(p5), std::forward<P6 >(p6 ),
		std::forward<P7>(p7), std::forward<P8 >(p8 ),
		std::forward<P9>(p9), std::forward<P10>(p10)));
}

template<typename T, typename P1, typename P2,  typename P3, typename P4,
                     typename P5, typename P6,  typename P7, typename P8,
                     typename P9, typename P10, typename P11>
std::unique_ptr<T> make_unique(P1&&  p1, P2&& p2, P3&& p3, P4&& p4, P5&&  p5,
                               P6&&  p6, P7&& p7, P8&& p8, P9&& p9, P10&& p10,
                               P11&& p11)
{
	return std::unique_ptr<T>(new T(
		std::forward<P1 >(p1 ), std::forward<P2 >(p2 ),
		std::forward<P3 >(p3 ), std::forward<P4 >(p4 ),
		std::forward<P5 >(p5 ), std::forward<P6 >(p6 ),
		std::forward<P7 >(p7 ), std::forward<P8 >(p8 ),
		std::forward<P9 >(p9 ), std::forward<P10>(p10),
		std::forward<P11>(p11)));
}

template<typename T, typename P1, typename P2,  typename P3,  typename P4,
                     typename P5, typename P6,  typename P7,  typename P8,
                     typename P9, typename P10, typename P11, typename P12>
std::unique_ptr<T> make_unique(P1&&  p1,  P2&&  p2, P3&& p3, P4&& p4, P5&&  p5,
                               P6&&  p6,  P7&&  p7, P8&& p8, P9&& p9, P10&& p10,
                               P11&& p11, P12&& p12)
{
	return std::unique_ptr<T>(new T(
		std::forward<P1 >(p1 ), std::forward<P2 >(p2 ),
		std::forward<P3 >(p3 ), std::forward<P4 >(p4 ),
		std::forward<P5 >(p5 ), std::forward<P6 >(p6 ),
		std::forward<P7 >(p7 ), std::forward<P8 >(p8 ),
		std::forward<P9 >(p9 ), std::forward<P10>(p10),
		std::forward<P11>(p11), std::forward<P12>(p12)));
}

template<typename T, typename P1, typename P2,  typename P3,  typename P4,
                     typename P5, typename P6,  typename P7,  typename P8,
                     typename P9, typename P10, typename P11, typename P12,
                     typename P13>
std::unique_ptr<T> make_unique(P1&&  p1,  P2&&  p2,  P3&&  p3, P4&& p4, P5&&  p5,
                               P6&&  p6,  P7&&  p7,  P8&&  p8, P9&& p9, P10&& p10,
                               P11&& p11, P12&& p12, P13&& p13)
{
	return std::unique_ptr<T>(new T(
		std::forward<P1 >(p1 ), std::forward<P2 >(p2 ),
		std::forward<P3 >(p3 ), std::forward<P4 >(p4 ),
		std::forward<P5 >(p5 ), std::forward<P6 >(p6 ),
		std::forward<P7 >(p7 ), std::forward<P8 >(p8 ),
		std::forward<P9 >(p9 ), std::forward<P10>(p10),
		std::forward<P11>(p11), std::forward<P12>(p12),
		std::forward<P13>(p13)));
}

#endif

#endif
