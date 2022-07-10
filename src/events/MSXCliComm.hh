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

	void log(LogLevel level, std::string_view message) override;
	void update(UpdateType type, std::string_view name,
	            std::string_view value) override;
	void updateFiltered(UpdateType type, std::string_view name,
	            std::string_view value) override;

private:
	MSXMotherBoard& motherBoard;
	GlobalCliComm& cliComm;
	hash_map<std::string, std::string, XXHasher> prevValues[NUM_UPDATES];
};

} // namespace openmsx

#endif
