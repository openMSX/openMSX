// $Id$

#include "SRAM.hh"
#include "File.hh"


SRAM::SRAM(int size_, MSXConfig::Device *config_, const char *header_)
{
	size = size_;
	config = config_;
	header = header_;
	sram = new byte[size];
	memset(sram, 0xFF, size);
	
	if (config->getParameterAsBool("loadsram")) {
		std::string filename = config->getParameter("sramname");
		PRT_DEBUG("SRAM: read " << filename);
		try {
			bool headerOk = true;
			File file(filename, STATE);
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
			}
		} catch (FileException &e) {
			PRT_INFO("Couldn't load SRAM " << filename);
		}
	}
}

SRAM::~SRAM()
{
	if (config->getParameterAsBool("savesram")) {
		std::string filename = config->getParameter("sramname");
		PRT_DEBUG("SRAM: save " << filename);
		try {
			File file(filename, STATE, TRUNCATE);
			if (header) {
				int length = strlen(header);
				file.write((byte*)header, length);
			}
			file.write(sram, size);
		} catch (FileException &e) {
			PRT_INFO("Couldn't save SRAM " << filename);
		}
	}
	delete[] sram;
}
