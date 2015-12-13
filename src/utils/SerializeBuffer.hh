#ifndef SERIALIZEBUFFER_HH
#define SERIALIZEBUFFER_HH

#include "MemBuffer.hh"
#include "openmsx.hh"
#include <algorithm>
#include <cstring>
#include <cassert>

namespace openmsx {

/** Memory output buffer
 *
  * Acts as a replacement for std::vector<byte>. You can insert data in the
  * buffer and the buffer will automatically grow. Like std::vector it manages
  * an internal memory buffer that will automatically reallocate and grow
  * exponentially.
  *
  * This class is much less general than std::vector and optimized for the case
  * of lots of (small) inserts at the end of the buffer (the main use case of
  * in-memory savestates). This makes it more efficient than std::vector.
  * std::vector is far from inefficient, but for savestates this is used A LOT,
  * so even small improvements matter a lot.
  */
class OutputBuffer
{
public:
	/** Create an empty output buffer.
	 */
	OutputBuffer();

	/** Insert data at the end of this buffer.
	  * This will automatically grow this buffer.
	  */
	void insert(const void* __restrict data, size_t len)
	{
#ifdef __GNUC__
		if (__builtin_constant_p(len)) {
			if        (len == 1) {
				insertN<1>(data); return;
			} else if (len == 2) {
				insertN<2>(data); return;
			} else if (len == 4) {
				insertN<4>(data); return;
			} else if (len == 8) {
				insertN<8>(data); return;
			}
		}
#endif
		insertN(data, len);
	}
#ifdef __GNUC__
	template<size_t N> void insertN(const void* __restrict data);
#endif
	void insertN(const void* __restrict data, size_t len);

	/** Insert data at a given position. This will overwrite the old data.
	  * It's not possible to grow the buffer via this method (so the buffer
	  * must already be big enough to hold the new data).
	  */
	void insertAt(size_t pos, const void* __restrict data, size_t len)
	{
		assert(buf.data() + pos + len <= finish);
		memcpy(buf.data() + pos, data, len);
	}

	/** Reserve space to insert the given number of bytes.
	  * The returned pointer is only valid until the next internal
	  * reallocate, so till the next call to insert() or allocate().
	  *
	  * If you don't know yet exactly how much memory to allocate (e.g.
	  * when the buffer will be used for gzip output data), you can request
	  * the maximum size and deallocate the unused space later.
	  */
	byte* allocate(size_t len)
	{
		byte* newEnd = end + len;
		// Make sure the next OutputBuffer will start with an initial size
		// that can hold this much space plus some slack.
		size_t newSize = newEnd - buf.data();
		lastSize = std::max(lastSize, newSize + 1000);
		if (newEnd <= finish) {
			byte* result = end;
			end = newEnd;
			return result;
		} else {
			return allocateGrow(len);
		}
	}

	/** Free part of a previously allocated buffer.
	 *
	  * The parameter must point right after the last byte of the used
	  * portion of the buffer. So it must be in the range [buf, buf+num]
	  * with buf and num respectively the return value and the parameter
	  * of the last allocate() call.
	  *
	  * See comment in allocate(). This call must be done right after the
	  * allocate() call, there cannot be any other (non-const) call to this
	  * object in between.
	  */
	void deallocate(byte* pos)
	{
		assert(buf.data() <= pos);
		assert(pos <= end);
		end = pos;
	}

	/** Get the current size of the buffer.
	 */
	size_t getPosition() const
	{
		return end - buf.data();
	}

	/** Release ownership of the buffer.
	 * Returns both the buffer and its size.
	 */
	MemBuffer<byte> release(size_t& size);

private:
	void insertGrow(const void* __restrict data, size_t len);
	byte* allocateGrow(size_t len);

	MemBuffer<byte> buf; // begin of allocated memory
	byte* end;           // points right after the last used byte
	                     // so   end - buf == size
	byte* finish;        // points right after the last allocated byte
	                     // so   finish - buf == capacity

	static size_t lastSize;
};


/** This class is complementary to the OutputBuffer class.
  * Instead of filling an initially empty buffer it starts from a filled buffer
  * and allows to retrieve items starting from the start of the buffer.
  */
class InputBuffer
{
public:
	/** Construct new InputBuffer, typically the data and size parameters
	  * will come from a MemBuffer object.
	  */
	InputBuffer(const byte* data, size_t size);

	/** Read the given number of bytes.
	  * This 'consumes' the read bytes, so a future read() will continue
	  * where this read stopped.
	  */
	void read(void* __restrict result, size_t len) __restrict
	{
		memcpy(result, buf, len);
		buf += len;
		assert(buf <= finish);
	}

	/** Skip the given number of bytes.
	  * This is similar to a read(), but it will only consume the data, not
	  * copy it.
	  */
	void skip(size_t len)
	{
		buf += len;
		assert(buf <= finish);
	}

	/** Return a pointer to the current position in the buffer.
	  * This is useful if you don't want to copy the data, but e.g. use it
	  * as input for an uncompress algorithm. You can later use skip() to
	  * actually consume the data.
	  */
	const byte* getCurrentPos() const { return buf; }

private:
	const byte* buf;
#ifndef NDEBUG
	const byte* finish; // only used to check asserts
#endif
};

} // namespace openmsx

#endif
