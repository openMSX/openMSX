// $Id$

#ifndef __CIRCULARBUFFER_HH__
#define __CIRCULARBUFFER_HH__

#include <cassert>


namespace openmsx {

template<class T, unsigned MAXSIZE>
class CircularBuffer
{
	public:
		CircularBuffer() {
			first = last = 0;
		}
		void addFront(const T& element) {
			assert(!isFull());
			first = prev(first);
			buffer[first] = element;
		}
		void addBack(const T& element) {
			assert(!isFull());
			unsigned tmp = last;
			last = next(last);
			buffer[tmp] = element;
		}
		T& removeFront() {
			assert(!isEmpty());
			unsigned tmp = first;
			first = next(first);
			return buffer[tmp];
		}
		T& removeBack() {
			assert(!isEmpty());
			last = prev(last);
			return buffer[last];
		}
		const T& operator[](unsigned pos) const {
			assert(pos < MAXSIZE);
			unsigned tmp = first + pos;
			if (tmp > MAXSIZE) {
				tmp -= (MAXSIZE + 1);
			}
			return buffer[tmp];
		}
		T& operator[](unsigned pos) {
			assert(pos < MAXSIZE);
			unsigned tmp = first + pos;
			if (tmp > MAXSIZE) {
				tmp -= (MAXSIZE + 1);
			}
			return buffer[tmp];
		}
		bool isEmpty() const {
			return (first == last);
		}
		bool isFull() const {
			return (first == next(last));
		}
		unsigned size() const {
			if (first > last) {
				return MAXSIZE + 1 - first + last;
			} else {
				return last - first;
			}
		}

	private:
		inline unsigned next(unsigned a) const {
			return (a == MAXSIZE) ? 0 : a + 1;
		}
		inline unsigned prev(unsigned a) const {
			return (a == 0) ? MAXSIZE : a - 1;
		}
		
		// one extra to be able to distinguish full and empty
		T buffer[MAXSIZE + 1];
		unsigned first, last;
};

} // namespace openmsx

#endif
