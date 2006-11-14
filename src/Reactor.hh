// $Id$

#ifndef REACTOR_HH
#define REACTOR_HH

#include "Observer.hh"
#include "EventListener.hh"
#include "Semaphore.hh"
#include "noncopyable.hh"
#include <string>
#include <memory>
#include <set>

namespace openmsx {

class EventDistributor;
class CommandController;
class GlobalCommandController;
class GlobalCliComm;
class Display;
class Mixer;
class CommandConsole;
class IconStatus;
class InputEventGenerator;
class DiskManipulator;
class FilePool;
class BooleanSetting;
class MSXMotherBoard;
class Setting;
class CommandLineParser;
class QuitCommand;
class MachineCommand;
class TestMachineCommand;
class ConfigInfo;
class GlobalSettings;
template <typename T> class EnumSetting;

/**
 * Contains the main loop of openMSX.
 * openMSX is almost single threaded: the main thread does most of the work,
 * we create additional threads only if we need blocking calls for
 * communicating with peripherals.
 * This class serializes all incoming requests so they can be handled by the
 * main thread.
 */
class Reactor : private Observer<Setting>, private EventListener,
                private noncopyable
{
public:
	Reactor();
	~Reactor();

	/**
	 * Main loop.
	 */
	void run(CommandLineParser& parser);

	void enterMainLoop();

	EventDistributor& getEventDistributor();
	GlobalCommandController& getGlobalCommandController();
	GlobalCliComm& getGlobalCliComm();
	InputEventGenerator& getInputEventGenerator();
	Display& getDisplay();
	Mixer& getMixer();
	CommandConsole& getCommandConsole();
	IconStatus& getIconStatus();
	DiskManipulator& getDiskManipulator();
	FilePool& getFilePool();
	EnumSetting<int>& getMachineSetting();

	void createMotherBoard(const std::string& machine);
	MSXMotherBoard* getMotherBoard() const;

	static void getHwConfigs(const std::string& type,
	                         std::set<std::string>& result);

	// convenience methods
	GlobalSettings& getGlobalSettings();
	CommandController& getCommandController();

private:
	void createMachineSetting();
	MSXMotherBoard& prepareMotherBoard(const std::string& machine);
	void switchMotherBoard(std::auto_ptr<MSXMotherBoard> mb);
	void deleteMotherBoard();

	// Observer<Setting>
	virtual void update(const Setting& setting);

	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	void block();
	void unblock();

	void unpause();
	void pause();

	bool paused;
	int blockedCounter;

	/**
	 * True iff the Reactor should keep running.
	 * When this is set to false, the Reactor will end the main loop after
	 * finishing the pending request(s).
	 */
	bool running;

	Semaphore mbSem;

	// note: order of auto_ptr's is important
	std::auto_ptr<EventDistributor> eventDistributor;
	std::auto_ptr<GlobalCommandController> globalCommandController;
	std::auto_ptr<GlobalCliComm> globalCliComm;
	std::auto_ptr<InputEventGenerator> inputEventGenerator;
	std::auto_ptr<Display> display;
	std::auto_ptr<Mixer> mixer;
	std::auto_ptr<CommandConsole> commandConsole;
	std::auto_ptr<IconStatus> iconStatus;
	std::auto_ptr<DiskManipulator> diskManipulator;
	std::auto_ptr<FilePool> filePool;

	BooleanSetting& pauseSetting;
	std::auto_ptr<EnumSetting<int> > machineSetting;

	const std::auto_ptr<QuitCommand> quitCommand;
	const std::auto_ptr<MachineCommand> machineCommand;
	const std::auto_ptr<TestMachineCommand> testMachineCommand;
	const std::auto_ptr<ConfigInfo> extensionInfo;
	const std::auto_ptr<ConfigInfo> machineInfo;

	std::auto_ptr<MSXMotherBoard> motherBoard;
	std::auto_ptr<MSXMotherBoard> newMotherBoard;

	friend class QuitCommand;
	friend class MachineCommand;
};

} // namespace openmsx

#endif // REACTOR_HH
