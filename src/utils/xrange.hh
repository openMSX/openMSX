#ifndef XRANGE_HH
#define XRANGE_HH

// Utility to iterate over a range of numbers,
// modeled after python's xrange() function.
//
// Typically used as a more compact notation for some for-loop patterns:
//
// a) loop over [0, N)
//
//   auto num = expensiveFunctionCall();
//   for (decltype(num) i = 0; i < num; ++i) { ... }
//
//   for (auto i : xrange(expensiveFunctionCall()) { ... }
//
// b) loop over [B, E)
//
//   auto i   = func1();
//   auto end = func2();
//   for (/**/; i < end; ++i) { ... }
//
//   for (auto i : xrange(func1(), func2()) { ... }
//
// Note that when using the xrange utility you also don't need to worry about
// the correct type of the induction variable.
//
// Gcc is able to optimize the xrange() based loops to the same code as the
// equivalent 'old-style' for loop.
//
// In an earlier version of this utility I also implemented the possibility to
// specify a step size (currently the step size is always +1). Although I
// believe that code was correct I still removed it because it was quite tricky
// (getting the stop condition correct is not trivial) and we don't need it
// currently.

template<typename T> class XRange
{
public:
	class XRangeIter
	{
	public:
		using difference_type = ptrdiff_t;
		using value_type = T;
		using pointer    = T*;
		using reference  = T&;
		using iterator_category = std::random_access_iterator_tag;

		explicit XRangeIter(T x_)
			: x(x_)
		{
		}

		// ForwardIterator
		[[nodiscard]] T operator*() const
		{
			return x;
		}

		XRangeIter& operator++()
		{
			++x;
			return *this;
		}
		XRangeIter operator++(int)
		{
			auto copy = *this;
			++x;
			return copy;
		}

		[[nodiscard]] bool operator==(const XRangeIter& other) const
		{
			return x == other.x;
		}
		[[nodiscard]] bool operator!=(const XRangeIter& other) const
		{
			return x != other.x;
		}

		// BidirectionalIterator
		XRangeIter& operator--()
		{
			--x;
			return *this;
		}
		XRangeIter operator--(int)
		{
			auto copy = *this;
			--x;
			return copy;
		}

		// RandomAccessIterator
		XRangeIter& operator+=(difference_type n)
		{
			x += n;
			return *this;
		}
		XRangeIter& operator-=(difference_type n)
		{
			x -= n;
			return *this;
		}

		[[nodiscard]] friend XRangeIter operator+(XRangeIter i, difference_type n)
		{
			i += n;
			return i;
		}
		[[nodiscard]] friend XRangeIter operator+(difference_type n, XRangeIter i)
		{
			i += n;
			return i;
		}
		[[nodiscard]] friend XRangeIter operator-(XRangeIter i, difference_type n)
		{
			i -= n;
			return i;
		}

		[[nodiscard]] friend difference_type operator-(const XRangeIter& i, const XRangeIter& j)
		{
			return i.x - j.x;
		}

		[[nodiscard]] T operator[](difference_type n)
		{
			return *(*this + n);
		}

		[[nodiscard]] friend bool operator<(const XRangeIter& i, const XRangeIter& j)
		{
			return i.x < j.x;
		}
		[[nodiscard]] friend bool operator<=(const XRangeIter& i, const XRangeIter& j)
		{
			return i.x <= j.x;
		}
		[[nodiscard]] friend bool operator>(const XRangeIter& i, const XRangeIter& j)
		{
			return i.x > j.x;
		}
		[[nodiscard]] friend bool operator>=(const XRangeIter& i, const XRangeIter& j)
		{
			return i.x >= j.x;
		}

	private:
		T x;
	};

	explicit XRange(T e_)
		: b(0), e(e_)
	{
	}

	XRange(T b_, T e_)
		: b(b_), e(e_)
	{
	}

	[[nodiscard]] auto begin() const { return XRangeIter(b); }
	[[nodiscard]] auto end()   const { return XRangeIter(e); }

private:
	const T b;
	const T e;
};

template<typename T> [[nodiscard]] inline auto xrange(T e)
{
	return XRange<T>(e);
}
template<typename T> [[nodiscard]] inline auto xrange(T b, T e)
{
	return XRange<T>(b, e);
}

#endif
