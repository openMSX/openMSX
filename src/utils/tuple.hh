// $Id$

#ifndef TUPLE_HH
#define TUPLE_HH

#include <tuple>

// C++11 has a class std::tuple with associated helper functions. One of those
// helper functions is called tuple_cat(). Apparently in visual studio 2010,
// the tuple class already exist, but the tuple_cat() function does not. This
// file implements the missing function (in a very limited way). As soon as we
// drop vs2010 support, we can also drop this file.

namespace openmsx {

template<int N1, int N2, typename TP1, typename TP2> struct TupleCatImpl;

template<typename TP1, typename TP2> struct TupleCatImpl<0, 0, TP1, TP2> {
	typedef std::tuple<> type;
	type operator()(TP1 /*tp1*/, TP2 /*tp2*/) const {
		return std::make_tuple();
	}
};

template<typename TP1, typename TP2> struct TupleCatImpl<1, 0, TP1, TP2> {
	typedef std::tuple<typename std::tuple_element<0, TP1>::type> type;
	type operator()(TP1 tp1, TP2 /*tp2*/) const {
		return std::make_tuple(std::get<0>(tp1));
	}
};
template<typename TP1, typename TP2> struct TupleCatImpl<0, 1, TP1, TP2> {
	typedef std::tuple<typename std::tuple_element<0, TP2>::type> type;
	type operator()(TP1 /*tp1*/, TP2 tp2) const {
		return std::make_tuple(std::get<0>(tp2));
	}
};

template<typename TP1, typename TP2> struct TupleCatImpl<2, 0, TP1, TP2> {
	typedef std::tuple<typename std::tuple_element<0, TP1>::type,
	                   typename std::tuple_element<1, TP1>::type> type;
	type operator()(TP1 tp1, TP2 /*tp2*/) const {
		return std::make_tuple(std::get<0>(tp1),
		                       std::get<1>(tp1));
	}
};
template<typename TP1, typename TP2> struct TupleCatImpl<1, 1, TP1, TP2> {
	typedef std::tuple<typename std::tuple_element<0, TP1>::type,
	                   typename std::tuple_element<0, TP2>::type> type;
	type operator()(TP1 tp1, TP2 tp2) const {
		return std::make_tuple(std::get<0>(tp1),
		                       std::get<0>(tp2));
	}
};
template<typename TP1, typename TP2> struct TupleCatImpl<0, 2, TP1, TP2> {
	typedef std::tuple<typename std::tuple_element<0, TP2>::type,
	                   typename std::tuple_element<1, TP2>::type> type;
	type operator()(TP1 /*tp1*/, TP2 tp2) const {
		return std::make_tuple(std::get<0>(tp2),
		                       std::get<1>(tp2));
	}
};

template<typename TP1, typename TP2> struct TupleCatImpl<3, 0, TP1, TP2> {
	typedef std::tuple<typename std::tuple_element<0, TP1>::type,
	                   typename std::tuple_element<1, TP1>::type,
	                   typename std::tuple_element<2, TP1>::type> type;
	type operator()(TP1 tp1, TP2 /*tp2*/) const {
		return std::make_tuple(std::get<0>(tp1),
		                       std::get<1>(tp1),
		                       std::get<2>(tp1));
	}
};
template<typename TP1, typename TP2> struct TupleCatImpl<2, 1, TP1, TP2> {
	typedef std::tuple<typename std::tuple_element<0, TP1>::type,
	                   typename std::tuple_element<1, TP1>::type,
	                   typename std::tuple_element<0, TP2>::type> type;
	type operator()(TP1 tp1, TP2 tp2) const {
		return std::make_tuple(std::get<0>(tp1),
		                       std::get<1>(tp1),
		                       std::get<0>(tp2));
	}
};
template<typename TP1, typename TP2> struct TupleCatImpl<1, 2, TP1, TP2> {
	typedef std::tuple<typename std::tuple_element<0, TP1>::type,
	                   typename std::tuple_element<0, TP2>::type,
	                   typename std::tuple_element<1, TP2>::type> type;
	type operator()(TP1 tp1, TP2 tp2) const {
		return std::make_tuple(std::get<0>(tp1),
		                       std::get<0>(tp2),
		                       std::get<1>(tp2));
	}
};
template<typename TP1, typename TP2> struct TupleCatImpl<0, 3, TP1, TP2> {
	typedef std::tuple<typename std::tuple_element<0, TP2>::type,
	                   typename std::tuple_element<1, TP2>::type,
	                   typename std::tuple_element<2, TP2>::type> type;
	type operator()(TP1 /*tp1*/, TP2 tp2) const {
		return std::make_tuple(std::get<0>(tp2),
		                       std::get<1>(tp2),
		                       std::get<2>(tp2));
	}
};

template<typename TP1, typename TP2>
struct TupleCat : TupleCatImpl<std::tuple_size<TP1>::value,
                               std::tuple_size<TP2>::value,
                               TP1,
                               TP2> {};

template<typename TP1, typename TP2>
typename TupleCat<TP1, TP2>::type tuple_cat(TP1 tp1, TP2 tp2)
{
	TupleCat<TP1, TP2> impl;
	return impl(tp1, tp2);
}

} // namespace openmsx

#endif
