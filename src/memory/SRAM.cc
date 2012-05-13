// $Id$

#include "SRAM.hh"
#include "XMLElement.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "FileNotFoundException.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "CliComm.hh"
#include "AlarmEvent.hh"
#include "serialize.hh"
#include "openmsx.hh"
#include "vla.hh"
#include <cstring>

using std::string;

namespace openmsx {

// class SRAM

/* Creates a SRAM that is not loaded from or saved to a file.
 * The only reason to use this (instead of a plain Ram object) is when you
 * dynamically need to decide whether load/save is needed.
 */
SRAM::SRAM(MSXMotherBoard& motherBoard, const std::string& name,
           const std::string& description, int size)
	: ram(motherBoard, name, description, size)
	, config(NULL)
	, header(NULL) // not used
	, cliComm(motherBoard.getMSXCliComm()) // not used
	, sramSync(new AlarmEvent(motherBoard.getReactor().getEventDistributor(),
	                          *this, OPENMSX_SAVE_SRAM)) // used, but not needed
{
}

SRAM::SRAM(MSXMotherBoard& motherBoard, const string& name, int size,
           const XMLElement& config_, const char* header_, bool* loaded)
	: ram(motherBoard, name, "sram", size)
	, config(&config_)
	, header(header_)
	, cliComm(motherBoard.getMSXCliComm())
	, sramSync(new AlarmEvent(motherBoard.getReactor().getEventDistributor(),
	                          *this, OPENMSX_SAVE_SRAM))
{
	load(loaded);
}

SRAM::SRAM(MSXMotherBoard& motherBoard, const string& name,
           const string& description, int size,
	   const XMLElement& config_, const char* header_, bool* loaded)
	: ram(motherBoard, name, description, size)
	, config(&config_)
	, header(header_)
	, cliComm(motherBoard.getMSXCliComm())
	, sramSync(new AlarmEvent(motherBoard.getReactor().getEventDistributor(),
	                          *this, OPENMSX_SAVE_SRAM))
{
	load(loaded);
}

SRAM::~SRAM()
{
	save();
}

void SRAM::write(unsigned addr, byte value)
{
	if (!sramSync->pending()) {
		sramSync->schedule(5000000); // sync to disk after 5s
	}
	assert(addr < getSize());
	ram[addr] = value;
}

void SRAM::memset(unsigned addr, byte c, unsigned size)
{
	if (!sramSync->pending()) {
		sramSync->schedule(5000000); // sync to disk after 5s
	}
	assert((addr + size) <= getSize());
	::memset(&ram[addr], c, size);
}

void SRAM::load(bool* loaded)
{
	assert(config);
	if (loaded) *loaded = false;
	const string& filename = config->getChildData("sramname");
	try {
		bool headerOk = true;
		File file(config->getFileContext().resolveCreate(filename),
			  File::LOAD_PERSISTENT);
		if (header) {
			int length = int(strlen(header));
			VLA(char, temp, length);
			file.read(temp, length);
			if (strncmp(temp, header, length) != 0) {
				headerOk = false;
			}
		}
		if (headerOk) {
			file.read(&ram[0], getSize());
			if (loaded) *loaded = true;
		} else {
			cliComm.printWarning(
				"Warning no correct SRAM file: " + filename);
		}
	} catch (FileNotFoundException& /*e*/) {
		cliComm.printInfo("SRAM file " + filename + " not found" +
		                  ", assuming blank SRAM content.");
	} catch (FileException& e) {
		cliComm.printWarning("Couldn't load SRAM " + filename +
		                     " (" + e.getMessage() + ").");
	}
}

void SRAM::save()
{
	if (!config) return;
	const string& filename = config->getChildData("sramname");
	try {
		File file(config->getFileContext().resolveCreate(filename),
			  File::SAVE_PERSISTENT);
		if (header) {
			int length = int(strlen(header));
			file.write(header, length);
		}
		file.write(&ram[0], getSize());
	} catch (FileException& e) {
		cliComm.printWarning("Couldn't save SRAM " + filename +
		                     " (" + e.getMessage() + ").");
	}
}

int SRAM::signalEvent(const shared_ptr<const Event>& event)
{
	// Event is received by all SRAM instances, so it will trigger a save
	// of all SRAMs. Could be optimized if it turns out to be a problem.
	assert(event->getType() == OPENMSX_SAVE_SRAM);
	(void)event;

	save();
	return 0;
}

template<typename Archive>
void SRAM::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("ram", ram);
}
INSTANTIATE_SERIALIZE_METHODS(SRAM);

} // namespace openmsx
