// $Id$

#include "SRAM.hh"
#include "Config.hh"
#include "File.hh"
#include "FileContext.hh"
#include "CliCommOutput.hh"

namespace openmsx {

SRAM::SRAM(int size_, Config* config_, const char* header_)
{
	size = size_;
	config = config_;
	header = header_;
	sram = new byte[size];
	memset(sram, 0xFF, size);
	
	const string& filename = config->getParameter("sramname");
	PRT_DEBUG("SRAM: read " << filename);
	try {
		bool headerOk = true;
		File file(config->getContext().resolveSave(filename),
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
			file.read(sram, size);
		} else {
			CliCommOutput::instance().printWarning(
				"Warning no correct SRAM file: " + filename);
		}
	} catch (FileException &e) {
		CliCommOutput::instance().printWarning(
			"Couldn't load SRAM " + filename);
	}
}

SRAM::~SRAM()
{
	const string& filename = config->getParameter("sramname");
	PRT_DEBUG("SRAM: save " << filename);
	try {
		File file(config->getContext().resolveSave(filename),
			  SAVE_PERSISTENT);
		if (header) {
			int length = strlen(header);
			file.write((const byte*)header, length);
		}
		file.write(sram, size);
	} catch (FileException& e) {
		CliCommOutput::instance().printWarning(
			"Couldn't save SRAM " + filename);
	}
	delete[] sram;
}

} // namespace openmsx
