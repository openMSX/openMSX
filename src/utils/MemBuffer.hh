// $Id$

#ifndef MEMBUFFER_HH
#define MEMBUFFER_HH

#include "openmsx.hh"
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

	/** Delete the buffer again.
	  * The data may not be used anymore, but see comment in 'MemBuffer'
	  * constructor about buffer 'stealing'.
	  */
	~OutputBuffer();

	/** Insert data at the end of this buffer.
	  * This will automatically grow this buffer.
	  */
	void insert(const void* __restrict data, unsigned len) __restrict
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
	template<unsigned N> void insertN(const void* __restrict data);
#endif
	void insertN(const void* __restrict data, unsigned len);

	/** Insert data at a given position. This will overwrite the old data.
	  * It's not possible to grow the buffer via this method (so the buffer
	  * must already be big enough to hold the new data).
	  */
	void insertAt(unsigned pos, const void* __restrict data, unsigned len) __restrict
	{
		assert(begin + pos + len <= finish);
		memcpy(begin + pos, data, len);
	}

	/** Reserve space to insert the given number of bytes.
	  * The returned pointer is only valid until the next internal
	  * reallocate, so till the next call to insert() or allocate().
	  *
	  * If you don't know yet exactly how much memory to allocate (e.g.
	  * when the buffer will be used for gzip output data), you can request
	  * the maximum size and deallocate the unused space later.
	  */
	byte* allocate(unsigned len);

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
	void deallocate(byte* pos);

	/** Get the current size of the buffer.
	 */
	unsigned getPosition() const
	{
		return end - begin;
	}

private:
	void insertGrow(const void* __restrict data, unsigned len);
	byte* allocateGrow(unsigned len);

	byte* begin;   // begin of allocated memory
	byte* end;     // points right after the last used byte
	               // so   end - begin == size
	byte* finish;  // points right after the last allocated byte
	               // so   finish - begin == capacity

	friend class MemBuffer; // to 'steal' the buffer
};


/** Memory buffer.
  * This class manages the lifetime of a block of memory.
  * The memory block can read/written, its length can be queried and it can
  * be grown/shrunk in size.
  * It's also possible to 'steal' the data buffer from an OutputBuffer object,
  * and manage that buffer using an object of this class.
  */
class MemBuffer
{
public:
	/** Construct an empty MemBuffer (getLength() == 0).
	 */
	MemBuffer();

	/** Construct MemBuffer. This will steal the buffer from the OutputBuffer
	 * object (so deleting the OutputBuffer won't free the buffer anymore),
	 * and take ownership over it.
	 */
	explicit MemBuffer(OutputBuffer& buffer);

	/** Free the memory buffer. */
	~MemBuffer();

	/** Returns pointer to the start of the memory buffer. */
	const byte* getData() const { return data; }
	      byte* getData()       { return data; }

	/** Returns size of the memory buffer. */
	unsigned getLength()  const { return len;  }

	/** Grow or shrink the memory block.
	  * In case of growing, the extra space is left uninitialized.
	  * It is possible (even likely) that the memory buffer is copied
	  * to a new location after this call, so getData() returns a different
	  * value.
	  */
	void realloc(unsigned newSize);

private:
	byte* data;
	unsigned len;
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
	InputBuffer(const byte* data, unsigned size);

	/** Read the given number of bytes.
	  * This 'consumes' the read bytes, so a future read() will continue
	  * where this read stopped.
	  */
	void read(void* __restrict result, unsigned len) __restrict
	{
		memcpy(result, buf, len);
		buf += len;
		assert(buf <= finish);
	}

	/** Skip the given number of bytes.
	  * This is similar to a read(), but it will only consume the data, not
	  * copy it.
	  */
	void skip(unsigned len)
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
