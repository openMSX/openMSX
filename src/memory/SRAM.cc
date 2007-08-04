// $Id$

#include "SRAM.hh"
#include "XMLElement.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "MSXMotherBoard.hh"
#include "MSXCliComm.hh"
#include "Alarm.hh"

using std::string;

namespace openmsx {

class SRAMSync : public Alarm
{
public:
	explicit SRAMSync(SRAM& sram);
	~SRAMSync();
private:
	virtual bool alarm();
	SRAM& sram;
};


// class SRAM

SRAM::SRAM(MSXMotherBoard& motherBoard, const string& name, int size,
           const XMLElement& config_, const char* header_)
	: ram(motherBoard, name, "sram", size)
	, config(config_)
	, header(header_)
	, cliComm(motherBoard.getMSXCliComm())
	, sramSync(new SRAMSync(*this))
{
	load();
}

SRAM::SRAM(MSXMotherBoard& motherBoard, const string& name,
           const string& description, int size,
	   const XMLElement& config_, const char* header_)
	: ram(motherBoard, name, description, size)
	, config(config_)
	, header(header_)
	, cliComm(motherBoard.getMSXCliComm())
{
	load();
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

void SRAM::load()
{
	const string& filename = config.getChildData("sramname");
	try {
		bool headerOk = true;
		File file(config.getFileContext().resolveCreate(filename),
			  File::LOAD_PERSISTENT);
		if (header) {
			int length = strlen(header);
			char temp[length];
			file.read(temp, length);
			if (strncmp(temp, header, length) != 0) {
				headerOk = false;
			}
		}
		if (headerOk) {
			file.read(&ram[0], getSize());
		} else {
			cliComm.printWarning(
				"Warning no correct SRAM file: " + filename);
		}
	} catch (FileException &e) {
		cliComm.printWarning("Couldn't load SRAM " + filename +
		                     " (" + e.getMessage() + ").");
	}
}

void SRAM::save()
{
	const string& filename = config.getChildData("sramname");
	try {
		File file(config.getFileContext().resolveCreate(filename),
			  File::SAVE_PERSISTENT);
		if (header) {
			int length = strlen(header);
			file.write(header, length);
		}
		file.write(&ram[0], getSize());
	} catch (FileException& e) {
		cliComm.printWarning("Couldn't save SRAM " + filename +
		                     " (" + e.getMessage() + ").");
	}
}


// class SRAMSync

SRAMSync::SRAMSync(SRAM& sram_)
	: sram(sram_)
{
}

SRAMSync::~SRAMSync()
{
	cancel(); // cancel pending alarm
}

bool SRAMSync::alarm()
{
	sram.save();
	return false; // don't reschedule
}

} // namespace openmsx
