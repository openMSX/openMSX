// $Id$

#ifndef __FILEOPENER_HH__
#define __FILEOPENER_HH__

#include <string>
#include <list>
#include "openmsx.hh"
#include "MSXException.hh"

class FileBase;
class FileContext;


enum OpenMode {
	NORMAL,
	TRUNCATE,
	LOAD_PERSISTENT,
	SAVE_PERSISTENT,
};

class FileException : public MSXException {
	public:
		FileException(const std::string &mes) : MSXException(mes) {}
};


class File
{
	public:
		/**
		 * Create file object and open underlying file.
		 * @param context Base URL for relative path.
		 * @param url Full URL or relative path of the file
		 *   that will be represented by this file object.
		 * @param mode Mode to open the file in:
		 */
		File(const FileContext *context, const std::string &url,
		     OpenMode mode = NORMAL);
		
		/**
		 * Destroy file object.
		 */
		~File();

		/**
		 * Read from file.
		 * @param buffer Destination address
		 * @param num Number of bytes to read
		 */
		void read(byte* buffer, int num);

		/**
		 * Write to file.
		 * @param buffer Source address
		 * @param num Number of bytes to write
		 */
		void write(const byte* buffer, int num);

		/**
		 * Map file in memory.
		 * @param writeBack Set to true if writes to the memory block
		 *              should also be written to the file. Note that
		 *              the file may only be updated when you munmap
		 *              again (may happen earlier but not guaranteed).
		 * @result Pointer to memory block.
		 */
		byte* mmap(bool writeBack = false);

		/**
		 * Unmap file from memory. If you have mmap'ed a file, you
		 * also must munmap it before. This is not done automatically
		 * when this object is destructed.
		 */
		void munmap();

		/**
		 * Returns the size of this file
		 * @result The size of this file
		 */
		int getSize();

		/**
		 * Move read/write pointer to the specified position.
		 * @param pos Position in bytes from the beginning of the file.
		 */
		void seek(int pos);

		/**
		 * Get the current position of the read/write pointer.
		 * @result Position in bytes from the beginning of the file.
		 */
		int getPos();

		/**
		 * Returns the URL of this file object.
		 * All context substitution have already been done on this URL
		 */
		const std::string getURL() const;

		/**
		 * Get a local filename for this object. Useful if this object
		 * refers to a HTTP or FTP file.
		 * @result Filename of a local file that is identical to the
		 *         file that this object refers to.
		 */
		const std::string getLocalName() const;

	private:
		void open(const std::string &url, OpenMode mode);
		
		FileBase *file;
};

#endif

