#ifndef GLOBALCLICOMM_HH
#define GLOBALCLICOMM_HH

#include "CliComm.hh"
#include "hash_map.hh"
#include "xxhash.hh"
#include <memory>
#include <mutex>
#include <vector>

namespace openmsx {

class CliListener;

class GlobalCliComm final : public CliComm
{
public:
	GlobalCliComm(const GlobalCliComm&) = delete;
	GlobalCliComm& operator=(const GlobalCliComm&) = delete;

	GlobalCliComm() = default;
	~GlobalCliComm();

	void addListener(std::unique_ptr<CliListener> listener);
	std::unique_ptr<CliListener> removeListener(CliListener& listener);

	// Before this method has been called commands send over external
	// connections are not yet processed (but they keep pending).
	void setAllowExternalCommands();

	// CliComm
	void log(LogLevel level, std::string_view message) override;
	void update(UpdateType type, std::string_view name,
	            std::string_view value) override;

private:
	void updateHelper(UpdateType type, std::string_view machine,
	                  std::string_view name, std::string_view value);

private:
	hash_map<std::string, std::string, XXHasher> prevValues[NUM_UPDATES];

	std::vector<std::unique_ptr<CliListener>> listeners; // unordered
	std::mutex mutex; // lock access to listeners member
	bool delivering = false;
	bool allowExternalCommands = false;

	friend class MSXCliComm;
};

} // namespace openmsx

#endif
