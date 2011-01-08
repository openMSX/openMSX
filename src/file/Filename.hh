// $Id$

#ifndef FILENAME_HH
#define FILENAME_HH

#include <string>

namespace openmsx {

class FileContext;
class CommandController;

/** This class represents a filename.
 * A filename is resolved in a certain context.
 * It is possible to query both the resolved and unresolved filename.
 *
 * Most file operations will want the resolved name, but for example
 * for savestates we (also) want the unresolved name.
 */
class Filename
{
public:
	Filename();
	explicit Filename(const std::string& filename);
	Filename(const std::string& filename, const FileContext& context);

	const std::string& getOriginal() const;
	const std::string& getResolved() const;

	/** After a loadstate we prefer to use the exact same file as before
	  * savestate. But if that file is not available (possibly because
	  * snapshot is loaded on a different host machine), we fallback to
	  * the original filename.
	  */
	void updateAfterLoadState();

	/** Convenience method to test for empty filename.
	 * In any case getOriginal().empty() and getResolved().empty() return
	 * the same result. This method is a shortcut to either of these.
	 */
	bool empty() const;

	/** Change the resolved part of this filename
	 * E.g. on loadstate when we didn't find the original file, but another
	 * file with a matching checksum.
	 */
	void setResolved(const std::string& resolved);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// non-const because we want this class to be assignable
	// (to be able to store them in std::vector)
	std::string originalFilename;
	std::string resolvedFilename;
};

} // namespace openmsx

#endif
