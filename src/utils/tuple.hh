// $Id$

#ifndef TUPLE_HH
#define TUPLE_HH

struct TupleNoArg {};
struct TupleBase {};

template<typename T1 = TupleNoArg, typename T2 = TupleNoArg, typename T3 = TupleNoArg>
struct Tuple : TupleBase
{
	static const int NUM = 3;
	typedef T1 type1;
	typedef T2 type2;
	typedef T3 type3;
	T1 t1;
	T2 t2;
	T3 t3;
	Tuple(T1 t1_, T2 t2_, T3 t3_) : t1(t1_), t2(t2_), t3(t3_) {}
};
template<typename T1, typename T2>
struct Tuple<T1, T2, TupleNoArg> : TupleBase
{
	static const int NUM = 2;
	typedef T1 type1;
	typedef T2 type2;
	T1 t1;
	T2 t2;
	Tuple(T1 t1_, T2 t2_) : t1(t1_), t2(t2_) {}
};
template<typename T1>
struct Tuple<T1, TupleNoArg, TupleNoArg> : TupleBase
{
	static const int NUM = 1;
	typedef T1 type1;
	T1 t1;
	Tuple(T1 t1_) : t1(t1_) {}
};
template<>
struct Tuple<TupleNoArg, TupleNoArg, TupleNoArg> : TupleBase
{
	static const int NUM = 0;
};


template<typename T1, typename T2, typename T3>
inline Tuple<T1, T2, T3> make_tuple(T1 t1, T2 t2, T3 t3)
{
	return Tuple<T1, T2, T3>(t1, t2, t3);
}
template<typename T1, typename T2>
inline Tuple<T1, T2> make_tuple(T1 t1, T2 t2)
{
	return Tuple<T1, T2>(t1, t2);
}
template<typename T1>
inline Tuple<T1> make_tuple(T1 t1)
{
	return Tuple<T1>(t1);
}
inline Tuple<> make_tuple()
{
	return Tuple<>();
}

template<int N1, int N2, typename TUPLE1, typename TUPLE2> struct MergerImpl;
template<typename TP1, typename TP2> struct MergerImpl<0, 0, TP1, TP2> {
	typedef Tuple<> Result;
	Result operator()(TP1 /*tp1*/, TP2 /*tp2*/) const {
		return make_tuple();
	}
};
template<typename TP1, typename TP2> struct MergerImpl<0, 1, TP1, TP2> {
	typedef Tuple<typename TP2::type1> Result;
	Result operator()(TP1 /*tp1*/, TP2 tp2) const {
		return make_tuple(tp2.t1);
	}
};
template<typename TP1, typename TP2> struct MergerImpl<0, 2, TP1, TP2> {
	typedef Tuple<typename TP2::type1, typename TP2::type2> Result;
	Result operator()(TP1 /*tp1*/, TP2 tp2) const {
		return make_tuple(tp2.t1, tp2.t2);
	}
};
template<typename TP1, typename TP2> struct MergerImpl<0, 3, TP1, TP2> {
	typedef Tuple<typename TP2::type1, typename TP2::type2, typename TP2::type3> Result;
	Result operator()(TP1 /*tp1*/, TP2 tp2) const {
		return make_tuple(tp2.t1, tp2.t2, tp2.t3);
	}
};
template<typename TP1, typename TP2> struct MergerImpl<1, 0, TP1, TP2> {
	typedef Tuple<typename TP1::type1> Result;
	Result operator()(TP1 tp1, TP2 /*tp2*/) const {
		return make_tuple(tp1.t1);
	}
};
template<typename TP1, typename TP2> struct MergerImpl<1, 1, TP1, TP2> {
	typedef Tuple<typename TP1::type1, typename TP2::type1> Result;
	Result operator()(TP1 tp1, TP2 tp2) const {
		return ::make_tuple(tp1.t1, tp2.t1);
	}
};
template<typename TP1, typename TP2> struct MergerImpl<1, 2, TP1, TP2> {
	typedef Tuple<typename TP1::type1, typename TP2::type1, typename TP2::type2> Result;
	Result operator()(TP1 tp1, TP2 tp2) const {
		return make_tuple(tp1.t1, tp2.t1, tp2.t2);
	}
};
template<typename TP1, typename TP2> struct MergerImpl<2, 0, TP1, TP2> {
	typedef Tuple<typename TP1::type1, typename TP1::type2> Result;
	Result operator()(TP1 tp1, TP2 /*tp2*/) const {
		return make_tuple(tp1.t1, tp1.t2);
	}
};
template<typename TP1, typename TP2> struct MergerImpl<2, 1, TP1, TP2> {
	typedef Tuple<typename TP1::type1, typename TP1::type2, typename TP2::type1> Result;
	Result operator()(TP1 tp1, TP2 tp2) const {
		return make_tuple(tp1.t1, tp1.t2, tp2.t1);
	}
};
template<typename TP1, typename TP2> struct MergerImpl<3, 0, TP1, TP2> {
	typedef Tuple<typename TP1::type1, typename TP1::type2, typename TP1::type3> Result;
	Result operator()(TP1 tp1, TP2 /*tp2*/) const {
		return make_tuple(tp1.t1, tp1.t2, tp1.t3);
	}
};

template<typename TUPLE1, typename TUPLE2> struct TupleMerger
{
	typedef MergerImpl<TUPLE1::NUM, TUPLE2::NUM, TUPLE1, TUPLE2> Impl;
	typedef typename Impl::Result type;

	type operator()(TUPLE1 tuple1, TUPLE2 tuple2) {
		Impl impl;
		return impl(tuple1, tuple2);
	}
};

#endif
