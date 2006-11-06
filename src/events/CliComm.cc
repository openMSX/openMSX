// $Id$

#include "CliComm.hh"

namespace openmsx {

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

} // namespace openmsx
