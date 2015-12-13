#ifndef MSXCLICOMM_HH
#define MSXCLICOMM_HH

#include "CliComm.hh"
#include "hash_map.hh"
#include "xxhash.hh"

namespace openmsx {

class MSXMotherBoard;
class GlobalCliComm;

class MSXCliComm final : public CliComm
{
public:
	MSXCliComm(MSXMotherBoard& motherBoard, GlobalCliComm& cliComm);

	void log(LogLevel level, string_ref message) override;
	void update(UpdateType type, string_ref name,
	            string_ref value) override;

private:
	MSXMotherBoard& motherBoard;
	GlobalCliComm& cliComm;
	hash_map<std::string, std::string, XXHasher> prevValues[NUM_UPDATES];
};

} // namespace openmsx

#endif
