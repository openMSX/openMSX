#include "SRAM.hh"

#include "DeviceConfig.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "FileNotFoundException.hh"
#include "MSXCliComm.hh"
#include "Reactor.hh"
#include "openmsx.hh"

#include "serialize.hh"
#include "small_buffer.hh"

namespace openmsx {

// class SRAM

// Like the constructor below, but doesn't create a debuggable.
// For use in unit-tests.
SRAM::SRAM(size_t size, const XMLElement& xml, DontLoadTag)
	: ram(xml, size)
{
}

/* Creates a SRAM that is not loaded from or saved to a file.
 * The only reason to use this (instead of a plain Ram object) is when you
 * dynamically need to decide whether load/save is needed.
 */
SRAM::SRAM(const std::string& name, static_string_view description,
           size_t size, const DeviceConfig& config_, DontLoadTag)
	: ram(config_, name, description, size)
{
}

SRAM::SRAM(const std::string& name, size_t size,
           const DeviceConfig& config_, const char* header_, bool* loaded)
	: schedulable(std::in_place, config_.getReactor().getRTScheduler(), *this)
	, config(config_)
	, ram(config, name, "sram", size)
	, header(header_)
{
	load(loaded);
}

SRAM::SRAM(const std::string& name, static_string_view description, size_t size,
	   const DeviceConfig& config_, const char* header_, bool* loaded)
	: schedulable(std::in_place, config_.getReactor().getRTScheduler(), *this)
	, config(config_)
	, ram(config, name, description, size)
	, header(header_)
{
	load(loaded);
}

SRAM::~SRAM()
{
	if (schedulable && schedulable->isPendingRT()) {
		schedulable->cancelRT();
		save();
	}
}

void SRAM::write(size_t addr, byte value)
{
	if (schedulable && !schedulable->isPendingRT()) {
		schedulable->scheduleRT(5000000); // sync to disk after 5s
	}
	assert(addr < size());
	ram.write(addr, value);
}

void SRAM::memset(size_t addr, byte c, size_t aSize)
{
	if (schedulable && !schedulable->isPendingRT()) {
		schedulable->scheduleRT(5000000); // sync to disk after 5s
	}
	assert((addr + aSize) <= size());
	ranges::fill(ram.getWriteBackdoor().subspan(addr, aSize), c);
}

void SRAM::load(bool* loaded)
{
	assert(config.getXML());
	if (loaded) *loaded = false;
	const auto& filename = config.getChildData("sramname");
	try {
		bool headerOk = true;
		File file(config.getFileContext().resolveCreate(filename),
			  File::OpenMode::LOAD_PERSISTENT);
		if (header) {
			size_t length = strlen(header);
			small_buffer<char, 64> buf(uninitialized_tag{}, length);
			file.read(std::span{buf});
			headerOk = ranges::equal(buf, std::span{header, length});
		}
		if (headerOk) {
			file.read(ram.getWriteBackdoor());
			if (loaded) *loaded = true;
		} else {
			config.getCliComm().printWarning(
				"Warning no correct SRAM file: ", filename);
		}
	} catch (FileNotFoundException& /*e*/) {
		// SRAM file not found, assuming blank SRAM content.
	} catch (FileException& e) {
		config.getCliComm().printWarning(
			"Couldn't load SRAM ", filename,
			" (", e.getMessage(), ").");
	}
}

void SRAM::save() const
{
	assert(config.getXML());
	const auto& filename = config.getChildData("sramname");
	try {
		File file(config.getFileContext().resolveCreate(filename),
			  File::OpenMode::SAVE_PERSISTENT);
		if (header) {
			auto length = strlen(header);
			file.write(std::span{header, length});
		}
		//file.write(std::span{ram}); // TODO error with clang-15/libc++
		file.write(std::span{ram.begin(), ram.end()});
	} catch (FileException& e) {
		config.getCliComm().printWarning(
			"Couldn't save SRAM ", filename,
			" (", e.getMessage(), ").");
	}
}

void SRAM::SRAMSchedulable::executeRT()
{
	sram.save();
}

template<typename Archive>
void SRAM::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("ram", ram);
}
INSTANTIATE_SERIALIZE_METHODS(SRAM);

} // namespace openmsx
