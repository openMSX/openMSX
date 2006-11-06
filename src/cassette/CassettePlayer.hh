// $Id$

#ifndef CASSETTEPLAYER_HH
#define CASSETTEPLAYER_HH

#include "EventListener.hh"
#include "CLIOption.hh"
#include "CassetteDevice.hh"
#include "SoundDevice.hh"
#include "EmuTime.hh"
#include <string>
#include <vector>
#include <memory>

namespace openmsx {

class CommandLineParser;
class CassetteImage;
class XMLElement;
class MSXMixer;
class Scheduler;
class MSXCliComm;
class WavWriter;
class ThrottleManager;
class LoadingIndicator;
class BooleanSetting;
class TapeCommand;
class CommandController;
class MSXCommandController;
class MSXEventDistributor;
class EventDistributor;

// TODO split in two files
class MSXCassettePlayerCLI : public CLIOption, public CLIFileType
{
public:
	explicit MSXCassettePlayerCLI(CommandLineParser& commandLineParser);
	virtual bool parseOption(const std::string& option,
	                         std::list<std::string>& cmdLine);
	virtual const std::string& optionHelp() const;
	virtual void parseFileType(const std::string& filename,
	                           std::list<std::string>& cmdLine);
	virtual const std::string& fileTypeHelp() const;

private:
	CommandController& commandController;
};


class CassettePlayer : public CassetteDevice, public SoundDevice, public EventListener
{
public:
	CassettePlayer(MSXCommandController& msxCommandController,
	               MSXMixer& mixer, Scheduler& Scheduler,
	               MSXEventDistributor& msxEventDistributor,
	               EventDistributor& eventDistributor,
	               MSXCliComm& cliComm);
	virtual ~CassettePlayer();

	void insertTape(const std::string& filename, const EmuTime& time);
	void removeTape(const EmuTime& time);

	// CassetteDevice
	virtual void setMotor(bool status, const EmuTime& time);
	virtual short readSample(const EmuTime& time);
	virtual void setSignal(bool output, const EmuTime& time);

	// Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

	// SoundDevice
	virtual void setVolume(int newVolume);
	virtual void setSampleRate(int sampleRate);
	virtual void updateBuffer(
		unsigned length, int* buffer, const EmuTime& start,
		const EmuDuration& sampDur);

private:
	bool isPlaying() const;
	void updateLoadingState();
	void updateAll(const EmuTime& time);
	void rewind(const EmuTime& time);
	void updatePosition(const EmuTime& time);
	short getSample(const EmuTime& time);
	void fillBuf(size_t length, double x);
	void flushOutput();
	void startRecording(const std::string& filename, const EmuTime& time);
	void reinitRecording(const EmuTime& time);
	void stopRecording(const EmuTime& time);
	void setMotorControl(bool status, const EmuTime& time);
	void autoRun();

        // EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	std::auto_ptr<CassetteImage> cassette;
	bool motor, motorControl;
	EmuTime tapeTime;
	EmuTime recTime;
	EmuTime prevTime;

	double lastX; // last unfiltered output
	double lastY; // last filtered output
	double partialOut;
	double partialInterval;
	bool lastOutput;
	size_t sampcnt;
	static const size_t BUF_SIZE = 1024;
	unsigned char buf[BUF_SIZE];

	std::auto_ptr<WavWriter> wavWriter;

	MSXCommandController& msxCommandController;
	Scheduler& scheduler;

	const std::auto_ptr<TapeCommand> tapeCommand;

	// SoundDevice
	int volume;
	EmuDuration delta;
	EmuTime playTapeTime;
	std::string casImage;

	MSXCliComm& cliComm;
	EventDistributor& eventDistributor;

	std::auto_ptr<LoadingIndicator> loadingIndicator;
	std::auto_ptr<BooleanSetting> autoRunSetting;

	friend class TapeCommand;
};

} // namespace openmsx

#endif
