#ifndef GLOBALCLICOMM_HH
#define GLOBALCLICOMM_HH

#include "CliComm.hh"
#include "Semaphore.hh"
#include "StringMap.hh"
#include "noncopyable.hh"
#include <memory>
#include <vector>

namespace openmsx {

class CliListener;

class GlobalCliComm final : public CliComm, private noncopyable
{
public:
	GlobalCliComm();
	~GlobalCliComm();

	void addListener(std::unique_ptr<CliListener> listener);
	std::unique_ptr<CliListener> removeListener(CliListener& listener);

	// Before this method has been called commands send over external
	// connections are not yet processed (but they keep pending).
	void setAllowExternalCommands();

	// CliComm
	void log(LogLevel level, string_ref message) override;
	void update(UpdateType type, string_ref name,
	            string_ref value) override;

private:
	void updateHelper(UpdateType type, string_ref machine,
	                  string_ref name, string_ref value);

	StringMap<std::string> prevValues[NUM_UPDATES];

	std::vector<std::unique_ptr<CliListener>> listeners;
	Semaphore sem; // lock access to listeners member
	bool delivering;
	bool allowExternalCommands;

	friend class MSXCliComm;
};

} // namespace openmsx

#endif
