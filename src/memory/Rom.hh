#ifndef ROM_HH
#define ROM_HH

#include "File.hh"
#include "MemBuffer.hh"
#include "sha1.hh"
#include "openmsx.hh"
#include <string>
#include <memory>
#include <cassert>

namespace openmsx {

class MSXMotherBoard;
class XMLElement;
class DeviceConfig;
class FileContext;
class RomDebuggable;

class Rom final
{
public:
	Rom(const std::string& name, const std::string& description,
	    const DeviceConfig& config, const std::string& id = "");
	Rom(Rom&& other) noexcept;
	~Rom();

	const byte& operator[](unsigned address) const {
		assert(address < size);
		return rom[address];
	}
	unsigned getSize() const { return size; }

	std::string getFilename() const;
	const std::string& getName() const { return name; }
	const std::string& getDescription() const { return description; }
	const Sha1Sum& getOriginalSHA1() const;

private:
	void init(MSXMotherBoard& motherBoard, const XMLElement& config,
	          const FileContext& context);
	bool checkSHA1(const XMLElement& config);

private:
	// !! update the move constructor when changing these members !!
	const byte* rom;
	MemBuffer<byte> extendedRom;

	File file; // can be a closed file

	mutable Sha1Sum originalSha1;
	std::string name;
	const std::string description;
	unsigned size;

	// This must come after 'name':
	//   the destructor of RomDebuggable calls Rom::getName(), which still
	//   needs the Rom::name member.
	std::unique_ptr<RomDebuggable> romDebuggable; // can be nullptr
};

} // namespace openmsx

#endif
