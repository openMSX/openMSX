// $Id$

#include "CliComm.hh"

namespace openmsx {

const char* const CliComm::levelStr[CliComm::NUM_LEVELS] = {
	"info", "warning", "error", "progress"
};

const char* const CliComm::updateStr[CliComm::NUM_UPDATES] = {
	"led", "setting", "setting-info", "hardware", "plug", "unplug",
	"media", "status", "extension", "sounddevice", "connector"
};


CliComm::CliComm()
{
}

CliComm::~CliComm()
{
}

void CliComm::printInfo(const std::string& message)
{
	log(INFO, message);
}

void CliComm::printWarning(const std::string& message)
{
	log(WARNING, message);
}

void CliComm::printError(const std::string& message)
{
	log(LOGLEVEL_ERROR, message);
}

void CliComm::printProgress(const std::string& message)
{
	log(PROGRESS, message);
}

} // namespace openmsx
