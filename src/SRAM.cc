// $Id$

#include "SRAM.hh"
#include "MSXConfig.hh"
#include "File.hh"


SRAM::SRAM(int size_, Config *config_, const char *header_)
{
	size = size_;
	config = config_;
	header = header_;
	sram = new byte[size];
	memset(sram, 0xFF, size);
	
	if (!config->hasParameter("loadsram") ||
	     config->getParameterAsBool("loadsram")) {
		const std::string &filename = config->getParameter("sramname");
		PRT_DEBUG("SRAM: read " << filename);
		try {
			bool headerOk = true;
			File file(config->getContext()->resolve(filename),
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
				PRT_INFO("Warning no correct SRAM file: " <<
				         filename);
			}
		} catch (FileException &e) {
			PRT_INFO("Couldn't load SRAM " << filename);
		}
	}
}

SRAM::~SRAM()
{
	if (!config->hasParameter("savesram") ||
	     config->getParameterAsBool("savesram")) {
		const std::string &filename = config->getParameter("sramname");
		PRT_DEBUG("SRAM: save " << filename);
		try {
			File file(config->getContext()->resolve(filename),
			          SAVE_PERSISTENT);
			if (header) {
				int length = strlen(header);
				file.write((const byte*)header, length);
			}
			file.write(sram, size);
		} catch (FileException &e) {
			PRT_INFO("Couldn't save SRAM " << filename);
		}
	}
	delete[] sram;
}
