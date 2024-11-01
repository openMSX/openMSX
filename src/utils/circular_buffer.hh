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

	cb_iterator() = default;
	cb_iterator(const cb_iterator& it) = default;
	cb_iterator(const BUF* buf_, T* p_) : buf(buf_), p(p_) {}

	cb_iterator& operator=(const cb_iterator& it) = default;

	[[nodiscard]] T& operator*()  const { return *p; }
	T* operator->() const { return  p; }

	[[nodiscard]] friend difference_type operator-(const cb_iterator& l, const cb_iterator& r) {
		return r.index(r.p) - l.index(l.p);
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

	[[nodiscard]] friend cb_iterator operator+(cb_iterator it, difference_type n) { it += n; return it; }
	[[nodiscard]] friend cb_iterator operator+(difference_type n, cb_iterator it) { it += n; return it; }
	[[nodiscard]] friend cb_iterator operator-(cb_iterator it, difference_type n) { it -= n; return it; }

	[[nodiscard]] T& operator[](difference_type n) const { return *(*this + n); }

	[[nodiscard]] bool operator== (const cb_iterator& it) const { return p == it.p; }
	[[nodiscard]] auto operator<=>(const cb_iterator& it) const { return index(p) <=> index(it.p); }

private:
	[[nodiscard]] size_t index(const T* q) const {
		return q ? buf->index(q) : buf->size();
	}

	const BUF* buf = nullptr;
	T* p = nullptr; // invariant: end-iterator    -> nullptr,
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
	static_assert(std::random_access_iterator<iterator>);

	circular_buffer() = default;

	explicit circular_buffer(size_t buffer_capacity)
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

	[[nodiscard]] auto begin() {
		return iterator(this, empty() ? nullptr : first);
	}
	[[nodiscard]] auto begin() const {
		return const_iterator(this, empty() ? nullptr : first);
	}
	[[nodiscard]] auto end()          { return iterator      (this, nullptr); }
	[[nodiscard]] auto end()    const { return const_iterator(this, nullptr); }
	[[nodiscard]] auto rbegin()       { return       reverse_iterator(end()); }
	[[nodiscard]] auto rbegin() const { return const_reverse_iterator(end()); }
	[[nodiscard]] auto rend()         { return       reverse_iterator(begin()); }
	[[nodiscard]] auto rend()   const { return const_reverse_iterator(begin()); }

	[[nodiscard]] auto& operator[](size_t i)       { return *add(first, i); }
	[[nodiscard]] auto& operator[](size_t i) const { return *add(first, i); }

	[[nodiscard]] auto& front()       { return *first; }
	[[nodiscard]] auto& front() const { return *first; }
	[[nodiscard]] auto& back()       { return *((last == buf ? stop : last) - 1); }
	[[nodiscard]] auto& back() const { return *((last == buf ? stop : last) - 1); }

	[[nodiscard]] size_t size() const { return siz; }
	[[nodiscard]] bool empty() const { return size() == 0; }
	[[nodiscard]] bool full() const { return capacity() == size(); }
	[[nodiscard]] size_t reserve() const { return capacity() - size(); }
	[[nodiscard]] size_t capacity() const { return stop - buf; }

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

	template<typename T2>
	void push_back(T2&& t) {
		::new (last) T(std::forward<T2>(t));
		increment(last);
		++siz;
	}

	template<typename T2>
	void push_front(T2&& t) {
		try {
			decrement(first);
			::new (first) T(std::forward<T2>(t));
			++siz;
		} catch (...) {
			increment(first);
			throw;
		}
	}

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
	[[nodiscard]] T* uninitialized_copy(const_iterator b, const_iterator e, T* dst) {
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

	[[nodiscard]] T* uninitialized_move_n(iterator src, size_t n, T* dst) {
		while (n) {
			::new (dst) T(std::move(*src));
			++src; ++dst; --n;
		}
		return dst;
	}

	void increment(auto*& p) const {
		if (++p == stop) p = buf;
	}
	void decrement(auto*& p) const {
		if (p == buf) p = stop;
		--p;
	}
	[[nodiscard]] auto* add(auto* p, difference_type n) const {
		p += n;
		if (p >= stop) p -= capacity();
		return p;
	}
	[[nodiscard]] auto* sub(auto* p, difference_type n) const {
		p -= n;
		if (p < buf) p += capacity();
		return p;
	}

	[[nodiscard]] size_t index(const T* p) const {
		return (p >= first)
			? (p - first)
			: (stop - first) + (p - buf);
	}

	[[nodiscard]] T* allocate(size_t n) {
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
	T* buf = nullptr;   // start of allocated area
	T* stop = nullptr;  // end of allocated area (exclusive)
	T* first = nullptr; // position of the 1st element in the buffer
	T* last = nullptr;  // position past the last element
	                    // note: both for a full or empty buffer first==last
	size_t siz = 0; // number of elements in the buffer

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

	cb_queue() = default;
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

	[[nodiscard]] const T& front() const { return buf.front(); }
	[[nodiscard]] const T& back() const  { return buf.back();  }
	[[nodiscard]] const T& operator[](size_t i) const { return buf[i]; }

	[[nodiscard]] auto  begin()       { return buf.begin();  }
	[[nodiscard]] auto  end()         { return buf.end();    }
	[[nodiscard]] auto  begin() const { return buf.begin();  }
	[[nodiscard]] auto  end()   const { return buf.end();    }
	[[nodiscard]] auto rbegin()       { return buf.rbegin(); }
	[[nodiscard]] auto rbegin() const { return buf.rbegin(); }
	[[nodiscard]] auto rend()         { return buf.rend();   }
	[[nodiscard]] auto rend()   const { return buf.rend();   }

	[[nodiscard]] size_t size() const { return buf.size(); }
	[[nodiscard]] bool empty() const { return buf.empty(); }
	void clear() { buf.clear(); }

	[[nodiscard]] auto& getBuffer()       { return buf; }
	[[nodiscard]] auto& getBuffer() const { return buf; }

private:
	void checkGrow() {
		if (buf.full()) {
			buf.set_capacity(std::max(size_t(4), buf.capacity() * 2));
		}
	}

	circular_buffer<T> buf;
};

#endif
