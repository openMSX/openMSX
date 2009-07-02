// $Id$

#ifndef MEMBUFFER_HH
#define MEMBUFFER_HH

#include "likely.hh"
#include <cstring>
#include <cassert>

namespace openmsx {

/** Memory output buffer
 *
  * Acts as a replacement for std::vector<char>. You can insert data in the
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
	void insert(const void* __restrict data, unsigned len)
	{
		char* newEnd = end + len;
		if (likely(newEnd <= finish)) {
			memcpy(end, data, len);
			end = newEnd;
		} else {
			insertGrow(data, len);
		}
	}

	/** Insert data at a given position. This will overwrite the old data.
	  * It's not possible to grow the buffer via this method (so the buffer
	  * must already be big enough to hold the new data).
	  */
	void insertAt(unsigned pos, const void* __restrict data, unsigned len)
	{
		assert(begin + pos + len <= finish);
		memcpy(begin + pos, data, len);
	}

	/** Get the current size of the buffer.
	 */
	unsigned getPosition() const
	{
		return end - begin;
	}

private:
	void insertGrow(const void* data, unsigned len);

	char* begin;   // begin of allocated memory
	char* end;     // points right after the last used byte
	               // so   end - begin == size
	char* finish;  // points right after the last allocated byte
	               // so   finish - begin == capacity

	friend class MemBuffer; // to 'steal' the buffer
};


/** Memory buffer.
  * This class steals the data buffer from an OutputBuffer object, takes
  * ownership of that buffer and gives a read-only view on it.
  */
class MemBuffer
{
public:
	/** Construct MemBuffer. This will steal the buffer from the OutputBuffer
	 * object (so deleting the OutputBuffer won't free the buffer anymore),
	 * and take ownership over it.
	 */
	explicit MemBuffer(OutputBuffer& buffer);

	/** Free the memory buffer. */
	~MemBuffer();

	/** Returns pointer to the start of the memory buffer. */
	const char* getData() const { return data; }

	/** Returns size of the memory buffer. */
	unsigned getLength()  const { return len;  }

private:
	char* data;
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
	InputBuffer(const char* data, unsigned size);

	/** Read the given number of bytes.
	  * This 'consumes' the read bytes, so a future read() will continue
	  * where this read stopped.
	  */
	void read(void* __restrict result, unsigned len)
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

private:
	const char* buf;
#ifndef NDEBUG
	const char* finish; // only used to check asserts
#endif
};

} // namespace openmsx

#endif
