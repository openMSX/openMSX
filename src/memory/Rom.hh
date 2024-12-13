#ifndef ROM_HH
#define ROM_HH

#include "File.hh"
#include "MemBuffer.hh"
#include "sha1.hh"
#include "static_string_view.hh"
#include "openmsx.hh"

#include <cassert>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace openmsx {

class MSXMotherBoard;
class XMLElement;
class DeviceConfig;
class FileContext;
class RomDebuggable;
class TclObject;

class Rom final
{
public:
	Rom(std::string name, static_string_view description,
	    const DeviceConfig& config, std::string_view id = {});
	Rom(Rom&& other) noexcept;
	~Rom();

	[[nodiscard]] const byte& operator[](size_t address) const {
		assert(address < rom.size());
		return rom[address];
	}
	[[nodiscard]] auto size()  const { return rom.size(); }
	[[nodiscard]] auto begin() const { return rom.begin(); }
	[[nodiscard]] auto end()   const { return rom.end(); }

	[[nodiscard]] std::string_view getFilename() const;
	[[nodiscard]] const std::string& getName() const { return name; }
	[[nodiscard]] std::string_view getDescription() const { return description; }
	[[nodiscard]] const Sha1Sum& getOriginalSHA1() const;
	[[nodiscard]] const Sha1Sum& getSHA1() const;

	void addPadding(size_t newSize, byte filler = 0xff);

	/**
	 * Add dict values with info to result
	 */
	void getInfo(TclObject& result) const;

private:
	void init(MSXMotherBoard& motherBoard, const XMLElement& config,
	          const FileContext& context);
	[[nodiscard]] bool checkSHA1(const XMLElement& config) const;

private:
	// !! update the move constructor when changing these members !!
	std::span<const byte> rom;
	MemBuffer<byte> extendedRom;

	File file; // can be a closed file

	mutable Sha1Sum originalSha1;
	mutable Sha1Sum actualSha1;
	std::string name;
	/*const*/ static_string_view description; // not const to allow move

	// This must come after 'name':
	//   the destructor of RomDebuggable calls Rom::getName(), which still
	//   needs the Rom::name member.
	std::unique_ptr<RomDebuggable> romDebuggable; // can be nullptr
};

} // namespace openmsx

#endif
