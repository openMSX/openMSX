#include "CliComm.hh"

namespace openmsx {

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

void CliComm::printProgress(std::string_view message, float fraction)
{
	log(PROGRESS, message, fraction);
}

} // namespace openmsx
