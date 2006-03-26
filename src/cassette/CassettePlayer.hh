// $Id$

#ifndef CASSETTEPLAYER_HH
#define CASSETTEPLAYER_HH

#include "CommandLineParser.hh"
#include "CassetteDevice.hh"
#include "SoundDevice.hh"
#include "EmuTime.hh"
#include <memory>

namespace openmsx {

class CassetteImage;
class XMLElement;
class Mixer;
class Scheduler;
class CliComm;
class WavWriter;
class ThrottleManager;
class BooleanSetting;
class TapeCommand;

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
	CommandLineParser& commandLineParser;
};


class CassettePlayer : public CassetteDevice, public SoundDevice
{
public:
	CassettePlayer(CommandController& commandController, Mixer& mixer,
	               Scheduler& Scheduler);
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

	std::auto_ptr<CassetteImage> cassette;
	bool motor, motorControl;
	bool isLoading; // for the ThrottleManager
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

	CommandController& commandController;
	Scheduler& scheduler;
	const std::auto_ptr<TapeCommand> tapeCommand;

	// SoundDevice
	int volume;
	EmuDuration delta;
	EmuTime playTapeTime;
	XMLElement* playerElem;

	CliComm& cliComm;
	ThrottleManager& throttleManager;

	std::auto_ptr<BooleanSetting> autoRunSetting;

	friend class TapeCommand;
};

} // namespace openmsx

#endif
