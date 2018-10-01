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

	GlobalCliComm();
	~GlobalCliComm();

	void addListener(std::unique_ptr<CliListener> listener);
	std::unique_ptr<CliListener> removeListener(CliListener& listener);

	// Before this method has been called commands send over external
	// connections are not yet processed (but they keep pending).
	void setAllowExternalCommands();

	// CliComm
	void log(LogLevel level, string_view message) override;
	void update(UpdateType type, string_view name,
	            string_view value) override;

private:
	void updateHelper(UpdateType type, string_view machine,
	                  string_view name, string_view value);

	hash_map<std::string, std::string, XXHasher> prevValues[NUM_UPDATES];

	std::vector<std::unique_ptr<CliListener>> listeners; // unordered
	std::mutex mutex; // lock access to listeners member
	bool delivering;
	bool allowExternalCommands;

	friend class MSXCliComm;
};

} // namespace openmsx

#endif
