// $Id$

#include "SRAM.hh"
#include "XMLElement.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "CliComm.hh"

using std::string;

namespace openmsx {

SRAM::SRAM(const string& name, int size,
           const XMLElement& config_, const char* header_)
	: Ram(name, "sram", size), config(config_), header(header_)
{
	init();
}

SRAM::SRAM(const string& name, const string& description, int size,
	   const XMLElement& config_, const char* header_)
	: Ram(name, description, size), config(config_), header(header_)
{
	init();
}

void SRAM::init()
{
	const string& filename = config.getChildData("sramname");
	PRT_DEBUG("SRAM: read " << filename);
	try {
		bool headerOk = true;
		File file(config.getFileContext().resolveCreate(filename),
			  LOAD_PERSISTENT);
		if (header) {
			int length = strlen(header);
			byte* temp = new byte[length];
			file.read(temp, length);
			if (strncmp((char*)temp, header, length) != 0) {
				headerOk = false;
			}
			delete[] temp;
		}
		if (headerOk) {
			file.read(&(this->operator[](0)), getSize());
		} else {
			CliComm::instance().printWarning(
				"Warning no correct SRAM file: " + filename);
		}
	} catch (FileException &e) {
		CliComm::instance().printWarning(
			"Couldn't load SRAM " + filename +
			" (" + e.getMessage() + ").");
	}
}

SRAM::~SRAM()
{
	const string& filename = config.getChildData("sramname");
	PRT_DEBUG("SRAM: save " << filename);
	try {
		File file(config.getFileContext().resolveCreate(filename),
			  SAVE_PERSISTENT);
		if (header) {
			int length = strlen(header);
			file.write((const byte*)header, length);
		}
		file.write(&(this->operator[](0)), getSize());
	} catch (FileException& e) {
		CliComm::instance().printWarning(
			"Couldn't save SRAM " + filename +
			" (" + e.getMessage() + ").");
	}
}

} // namespace openmsx
