// $Id$

#ifndef __FILEOPENER_HH__
#define __FILEOPENER_HH__

#include <string>
#include "openmsx.hh"
#include "MSXException.hh"

namespace openmsx {

class FileBase;


enum OpenMode {
	NORMAL,
	TRUNCATE,
	CREATE,
	LOAD_PERSISTENT,
	SAVE_PERSISTENT,
};

class FileException : public MSXException {
public:
	FileException(const string &mes) : MSXException(mes) {}
};


class File
{
public:
	/**
	 * Create file object and open underlying file.
	 * @param url Full URL or relative path of the file
	 *   that will be represented by this file object.
	 * @param mode Mode to open the file in:
	 */
	File(const string &url, OpenMode mode = NORMAL) throw(FileException);
	
	/**
	 * Destroy file object.
	 */
	~File();

	/**
	 * Read from file.
	 * @param buffer Destination address
	 * @param num Number of bytes to read
	 */
	void read(byte* buffer, unsigned num) throw(FileException);

	/**
	 * Write to file.
	 * @param buffer Source address
	 * @param num Number of bytes to write
	 */
	void write(const byte* buffer, unsigned num) throw(FileException);

	/**
	 * Map file in memory.
	 * @param writeBack Set to true if writes to the memory block
	 *              should also be written to the file. Note that
	 *              the file may only be updated when you munmap
	 *              again (may happen earlier but not guaranteed).
	 * @result Pointer to memory block.
	 */
	byte* mmap(bool writeBack = false) throw(FileException);

	/**
	 * Unmap file from memory. 
	 */
	void munmap() throw(FileException);

	/**
	 * Returns the size of this file
	 * @result The size of this file
	 */
	unsigned getSize() throw(FileException);

	/**
	 * Move read/write pointer to the specified position.
	 * @param pos Position in bytes from the beginning of the file.
	 */
	void seek(unsigned pos) throw(FileException);

	/**
	 * Get the current position of the read/write pointer.
	 * @result Position in bytes from the beginning of the file.
	 */
	unsigned getPos() throw(FileException);

	/**
	 * Returns the URL of this file object.
	 */
	const string getURL() const throw(FileException);

	/**
	 * Get a local filename for this object. Useful if this object
	 * refers to a HTTP or FTP file.
	 * @result Filename of a local file that is identical to the
	 *         file that this object refers to.
	 */
	const string getLocalName() const throw(FileException);

	/**
	 * Check if this file is readonly
	 * @result true iff file is readonly
	 */
	bool isReadOnly() const throw(FileException);

private:
	FileBase *file;
};

} // namespace openmsx

#endif

