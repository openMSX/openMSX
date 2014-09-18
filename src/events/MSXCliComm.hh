#ifndef MSXCLICOMM_HH
#define MSXCLICOMM_HH

#include "CliComm.hh"
#include "StringMap.hh"
#include "noncopyable.hh"

namespace openmsx {

class MSXMotherBoard;
class GlobalCliComm;

class MSXCliComm final : public CliComm, private noncopyable
{
public:
	MSXCliComm(MSXMotherBoard& motherBoard, GlobalCliComm& cliComm);

	virtual void log(LogLevel level, string_ref message);
	virtual void update(UpdateType type, string_ref name,
	                    string_ref value);

private:
	MSXMotherBoard& motherBoard;
	GlobalCliComm& cliComm;
	StringMap<std::string> prevValues[NUM_UPDATES];
};

} // namespace openmsx

#endif
