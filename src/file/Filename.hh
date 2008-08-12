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
	explicit Filename(const std::string& filename);
	Filename(const std::string& filename, CommandController& controller);
	Filename(const std::string& filename, const FileContext& context);
	Filename(const std::string& filename, const FileContext& context,
	         CommandController& controller);

	const std::string& getOriginal() const;
	const std::string getResolved() const;

private:
	const std::string originalFilename;
	const std::string resolvedFilename;
};

} // namespace openmsx

#endif
