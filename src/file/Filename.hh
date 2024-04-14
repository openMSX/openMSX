#ifndef FILENAME_HH
#define FILENAME_HH

#include "FileContext.hh"
#include "FileOperations.hh"

#include <concepts>
#include <string>

namespace openmsx {

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
	// dummy constructor, to be able to serialize vector<Filename>
	Filename() = default;

	template<typename String>
		requires(!std::same_as<Filename, std::remove_cvref_t<String>>) // don't block copy-constructor
	explicit Filename(String&& filename)
		: originalFilename(std::forward<String>(filename))
		, resolvedFilename(originalFilename) {}

	template<typename String>
	Filename(String&& filename, const FileContext& context)
		: originalFilename(std::forward<String>(filename))
		, resolvedFilename(FileOperations::getAbsolutePath(
			context.resolve(originalFilename))) {}

	[[nodiscard]] const std::string& getOriginal() const { return originalFilename; }
	[[nodiscard]] const std::string& getResolved() const & { return           resolvedFilename; }
	[[nodiscard]]       std::string  getResolved() &&      { return std::move(resolvedFilename); }

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
	[[nodiscard]] bool empty() const;

	/** Change the resolved part of this filename
	 * E.g. on loadstate when we didn't find the original file, but another
	 * file with a matching checksum.
	 */
	void setResolved(std::string resolved) {
		resolvedFilename = std::move(resolved);
	}

	// Do both Filename objects point to the same file?
	[[nodiscard]] bool operator==(const Filename& other) const {
		return resolvedFilename == other.resolvedFilename;
	}

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
