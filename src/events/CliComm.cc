#include "CliComm.hh"

namespace openmsx {

const char* const CliComm::levelStr[CliComm::NUM_LEVELS] = {
	"info", "warning", "error", "progress"
};

const char* const CliComm::updateStr[CliComm::NUM_UPDATES] = {
	"led", "setting", "setting-info", "hardware", "plug",
	"media", "status", "extension", "sounddevice", "connector"
};


void CliComm::printInfo(std::string_view message)
{
	log(INFO, message);
}

void CliComm::printWarning(std::string_view message)
{
	log(WARNING, message);
}

void CliComm::printError(std::string_view message)
{
	log(LOGLEVEL_ERROR, message);
}

void CliComm::printProgress(std::string_view message)
{
	log(PROGRESS, message);
}

} // namespace openmsx
