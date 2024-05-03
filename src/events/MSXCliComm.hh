#ifndef MSXCLICOMM_HH
#define MSXCLICOMM_HH

#include "CliComm.hh"

#include "hash_map.hh"
#include "stl.hh"
#include "xxhash.hh"

#include <array>

namespace openmsx {

class MSXMotherBoard;
class GlobalCliComm;

class MSXCliComm final : public CliComm
{
public:
	MSXCliComm(MSXMotherBoard& motherBoard, GlobalCliComm& cliComm);

	void log(LogLevel level, std::string_view message, float fraction) override;
	void update(UpdateType type, std::string_view name,
	            std::string_view value) override;
	void updateFiltered(UpdateType type, std::string_view name,
	            std::string_view value) override;

	// enable/disable message suppression
	void setSuppressMessages(bool enable);

private:
	MSXMotherBoard& motherBoard;
	GlobalCliComm& cliComm;
	array_with_enum_index<CliComm::UpdateType, hash_map<std::string, std::string, XXHasher>> prevValues;
	bool suppressMessages = false;
};

} // namespace openmsx

#endif
