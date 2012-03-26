// $Id$

#ifndef ROM_HH
#define ROM_HH

#include "MemBuffer.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include <string>
#include <memory>
#include <cassert>

namespace openmsx {

class MSXMotherBoard;
class XMLElement;
class File;
class CliComm;
class RomDebuggable;

class Rom : private noncopyable
{
public:
	Rom(MSXMotherBoard& motherBoard, const std::string& name,
	    const std::string& description, const XMLElement& config,
	    const std::string& id = "");
	virtual ~Rom();

	const byte& operator[](unsigned address) const {
		assert(address < size);
		return rom[address];
	}
	unsigned getSize() const { return size; }

	std::string getFilename() const;
	const std::string& getName() const;
	const std::string& getDescription() const;
	const std::string& getOriginalSHA1() const;
	const std::string& getPatchedSHA1() const;

private:
	void init(MSXMotherBoard& motherBoard, const XMLElement& config);
	bool checkSHA1(const XMLElement& config);

	const byte* rom;
	MemBuffer<byte> extendedRom;

	std::auto_ptr<File> file;

	mutable std::string originalSha1;
	std::string patchedSha1;
	std::string name;
	const std::string description;
	unsigned size;

	// This must come after 'name':
	//   the destructor of RomDebuggable calls Rom::getName(), which still
	//   needs the Rom::name member.
	std::auto_ptr<RomDebuggable> romDebuggable;
};

} // namespace openmsx

#endif
