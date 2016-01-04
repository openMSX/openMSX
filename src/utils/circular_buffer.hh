/**
 * This code is heavily based on boost circular_buffer:
 *    http://www.boost.org/doc/libs/1_55_0/doc/html/circular_buffer.html
 * The interface of this version and the original boost version is (as much as
 * possible identical). I did leave out some of the more fancy methods like
 * insert/remove in the middle of the buffer or linearize the buffer. If we
 * ever need those we can add them of course.
 * So (for now) this class represents a dynamically sized circular buffer with
 * (efficient) {push,pop}_{front,back} and operator[] methods. It also offers
 * random access iterators. For much more details check the boost
 * documentation.
 */
#ifndef CIRCULAR_BUFFER_HH
#define CIRCULAR_BUFFER_HH

#include <algorithm>
#include <iterator>
#include <utility>
#include <cstddef>
#include <cstdlib>

/** Random access iterator for circular_buffer. */
template<typename BUF, typename T> class cb_iterator
{
public:
	using value_type = T;
	using pointer    = T*;
	using reference  = T&;
	using difference_type = ptrdiff_t;
	using iterator_category = std::random_access_iterator_tag;

	cb_iterator() : buf(nullptr), p(nullptr) {}
	cb_iterator(const cb_iterator& it) : buf(it.buf), p(it.p) {}
	cb_iterator(const BUF* buf_, T* p_) : buf(buf_), p(p_) {}

	cb_iterator& operator=(const cb_iterator& it) {
		buf = it.buf; p = it.p; return *this;
	}

	T& operator*()  const { return *p; }
	T* operator->() const { return  p; }

	difference_type operator-(const cb_iterator& it) const {
		return index(p) - index(it.p);
	}

	cb_iterator& operator++() {
		buf->increment(p);
		if (p == buf->last) p = nullptr;
		return *this;
	}
	cb_iterator& operator--() {
		if (p == nullptr) p = buf->last;
		buf->decrement(p);
		return *this;
	}
	cb_iterator operator++(int) { auto tmp = *this; ++*this; return tmp; }
	cb_iterator operator--(int) { auto tmp = *this; --*this; return tmp; }

	cb_iterator& operator+=(difference_type n) {
		if (n > 0) {
			p = buf->add(p,  n);
			if (p == buf->last) p = nullptr;
		} else if (n < 0) {
			if (p == nullptr) p = buf->last;
			p = buf->sub(p, -n);
		} else {
			// nothing, but _must_ be handled separately
		}
		return *this;
	}
	cb_iterator& operator-=(difference_type n) { *this += -n; return *this; }

	cb_iterator operator+(difference_type n) { return cb_iterator(*this) += n; }
	cb_iterator operator-(difference_type n) { return cb_iterator(*this) -= n; }

	T& operator[](difference_type n) const { return *(*this + n); }

	bool operator==(const cb_iterator& it) const { return p == it.p; }
	bool operator!=(const cb_iterator& it) const { return p != it.p; }

	bool operator<(const cb_iterator& it) const {
		return index(p) < index(it.p);
	}
	bool operator> (const cb_iterator& it) const { return   it < *this;  }
	bool operator<=(const cb_iterator& it) const { return !(it < *this); }
	bool operator>=(const cb_iterator& it) const { return !(*this < it); }

private:
	size_t index(const T* q) const {
		return q ? buf->index(q) : buf->size();
	}

	const BUF* buf;
	T* p; // invariant: end-iterator    -> nullptr,
	      //            other iterators -> pointer to element
};

/** Circular buffer class, based on boost::circular_buffer/ */
template<typename T> class circular_buffer
{
public:
	using               iterator = cb_iterator<circular_buffer<T>,       T>;
	using         const_iterator = cb_iterator<circular_buffer<T>, const T>;
	using       reverse_iterator = std::reverse_iterator<      iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;
	using value_type = T;
	using pointer    = T*;
	using reference  = T&;
	using difference_type = ptrdiff_t;
	using size_type       = size_t;

	circular_buffer()
		: buf(nullptr), stop(nullptr)
		, first(nullptr), last(nullptr), siz(0) {}

	explicit circular_buffer(size_t buffer_capacity)
		: siz(0)
	{
		buf = allocate(buffer_capacity);
		stop = buf + buffer_capacity;
		first = last = buf;
	}

	circular_buffer(const circular_buffer& cb)
		: siz(cb.size())
	{
		buf = allocate(cb.capacity());
		stop = buf + cb.capacity();
		first = buf;
		try {
			last = uninitialized_copy(cb.begin(), cb.end(), buf);
		} catch (...) {
			free(buf);
			throw;
		}
		if (last == stop) last = buf;
	}

	circular_buffer(circular_buffer&& cb) noexcept
		: buf(nullptr), stop(nullptr)
		, first(nullptr), last(nullptr), siz(0)
	{
		cb.swap(*this);
	}

	~circular_buffer() {
		destroy();
	}

	circular_buffer& operator=(const circular_buffer& cb) {
		if (this == &cb) return *this;
		T* buff = allocate(cb.capacity());
		try {
			reset(buff,
			      uninitialized_copy(cb.begin(), cb.end(), buff),
			      cb.capacity());
		} catch (...) {
			free(buff);
			throw;
		}
		return *this;
	}

	circular_buffer& operator=(circular_buffer&& cb) noexcept {
		cb.swap(*this);
		circular_buffer().swap(cb);
		return *this;
	}

	void swap(circular_buffer& cb) noexcept {
		std::swap(buf,   cb.buf);
		std::swap(stop,  cb.stop);
		std::swap(first, cb.first);
		std::swap(last,  cb.last);
		std::swap(siz,   cb.siz);
	}

	iterator begin() {
		return iterator(this, empty() ? nullptr : first);
	}
	const_iterator begin() const {
		return const_iterator(this, empty() ? nullptr : first);
	}
	iterator       end()       { return iterator      (this, nullptr); }
	const_iterator end() const { return const_iterator(this, nullptr); }
	      reverse_iterator rbegin()       { return       reverse_iterator(end()); }
	const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
	      reverse_iterator rend()       { return       reverse_iterator(begin()); }
	const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }

	      T& operator[](size_t i)       { return *add(first, i); }
	const T& operator[](size_t i) const { return *add(first, i); }

	      T& front()       { return *first; }
	const T& front() const { return *first; }
	      T& back()       { return *((last == buf ? stop : last) - 1); }
	const T& back() const { return *((last == buf ? stop : last) - 1); }

	size_t size() const { return siz; }
	bool empty() const { return size() == 0; }
	bool full() const { return capacity() == size(); }
	size_t reserve() const { return capacity() - size(); }
	size_t capacity() const { return stop - buf; }

	void set_capacity(size_t new_capacity) {
		if (new_capacity == capacity()) return;
		T* new_buf = allocate(new_capacity);
		iterator b = begin();
		try {
			reset(new_buf,
			      uninitialized_move_n(b, std::min(new_capacity, size()),
			                           new_buf),
			      new_capacity);
		} catch (...) {
			free(new_buf);
			throw;
		}
	}

	void push_back (const T&  t) { push_back_impl <const T& >(          t ); }
	void push_back (      T&& t) { push_back_impl <      T&&>(std::move(t)); }
	void push_front(const T&  t) { push_front_impl<const T& >(          t ); }
	void push_front(      T&& t) { push_front_impl<      T&&>(std::move(t)); }

	void push_back(std::initializer_list<T> list) {
		for (auto& e : list) push_back(e);
	}

	void pop_back() {
		decrement(last);
		last->~T();
		--siz;
	}

	void pop_front() {
		first->~T();
		increment(first);
		--siz;
	}

	void clear() {
		for (size_t i = 0; i < size(); ++i, increment(first)) {
			first->~T();
		}
		siz = 0;
	}

private:
	T* uninitialized_copy(const_iterator b, const_iterator e, T* dst) {
		T* next = dst;
		try {
			while (b != e) {
				::new (dst) T(*b);
				++b; ++dst;
			}
		} catch (...) {
			while (next != dst) {
				next->~T();
				++next;
			}
			throw;
		}
		return dst;
	}

	T* uninitialized_move_n(iterator src, size_t n, T* dst) {
		while (n) {
			::new (dst) T(std::move(*src));
			++src; ++dst; --n;
		}
		return dst;
	}

	template<typename ValT> void push_back_impl(ValT t) {
		::new (last) T(static_cast<ValT>(t));
		increment(last);
		++siz;
	}

	template<typename ValT> void push_front_impl(ValT t) {
		try {
			decrement(first);
			::new (first) T(static_cast<ValT>(t));
			++siz;
		} catch (...) {
			increment(first);
			throw;
		}
	}

	template<typename Pointer> void increment(Pointer& p) const {
		if (++p == stop) p = buf;
	}
	template<typename Pointer> void decrement(Pointer& p) const {
		if (p == buf) p = stop;
		--p;
	}
	template<typename Pointer> Pointer add(Pointer p, difference_type n) const {
		p += n;
		if (p >= stop) p -= capacity();
		return p;
	}
	template<typename Pointer> Pointer sub(Pointer p, difference_type n) const {
		p -= n;
		if (p < buf) p += capacity();
		return p;
	}

	size_t index(const T* p) const {
		return (p >= first)
			? (p - first)
			: (stop - first) + (p - buf);
	}

	T* allocate(size_t n) {
		return (n == 0) ? nullptr
		                : static_cast<T*>(malloc(n * sizeof(T)));
	}

	void destroy() {
		clear();
		free(buf);
	}

	void reset(T* new_buf, T* new_last, size_t new_capacity) {
		destroy();
		siz = new_last - new_buf;
		first = buf = new_buf;
		stop = buf + new_capacity;
		last = new_last == stop ? buf : new_last;
	}

private:
	T* buf;   // start of allocated area
	T* stop;  // end of allocated area (exclusive)
	T* first; // position of the 1st element in the buffer
	T* last;  // position past the last element
	          // note: both for a full or empty buffer first==last
	size_t siz; // number of elements in the buffer

	template<typename BUF, typename T2> friend class cb_iterator;
};


/** This implements a queue on top of circular_buffer (not part of boost).
  * It will automatically grow the buffer when its capacity is too small
  * while inserting new elements. */
template<typename T> class cb_queue
{
public:
	using             value_type = typename circular_buffer<T>::value_type;
	using               iterator = typename circular_buffer<T>::iterator;
	using         const_iterator = typename circular_buffer<T>::const_iterator;
	using       reverse_iterator = typename circular_buffer<T>::reverse_iterator;
	using const_reverse_iterator = typename circular_buffer<T>::const_reverse_iterator;

	cb_queue() {}
	explicit cb_queue(size_t capacity)
		: buf(capacity) {}

	template<typename U>
	void push_back(U&& u) { checkGrow(); buf.push_back(std::forward<U>(u)); }

	template<typename U>
	void push_back(std::initializer_list<U> list) {
		for (auto& e : list) push_back(e);
	}

	T pop_front() {
		T t = std::move(buf.front());
		buf.pop_front();
		return t;
	}

	const T& front() const { return buf.front(); }
	const T& back() const  { return buf.back();  }
	const T& operator[](size_t i) const { return buf[i]; }

	      iterator          begin()       { return buf.begin();  }
	      iterator          end()         { return buf.end();    }
	const_iterator          begin() const { return buf.begin();  }
	const_iterator          end()   const { return buf.end();    }
	      reverse_iterator rbegin()       { return buf.rbegin(); }
	const_reverse_iterator rbegin() const { return buf.rbegin(); }
	      reverse_iterator rend()         { return buf.rend();   }
	const_reverse_iterator rend()   const { return buf.rend();   }

	size_t size() const { return buf.size(); }
	bool empty() const { return buf.empty(); }
	void clear() { buf.clear(); }

	      circular_buffer<T>& getBuffer()       { return buf; }
	const circular_buffer<T>& getBuffer() const { return buf; }

private:
	void checkGrow() {
		if (buf.full()) {
			buf.set_capacity(std::max(size_t(4), buf.capacity() * 2));
		}
	}

	circular_buffer<T> buf;
};

#endif
