// $Id$

#ifndef CASSETTEPLAYER_HH
#define CASSETTEPLAYER_HH

#include "EventListener.hh"
#include "CassetteDevice.hh"
#include "SoundDevice.hh"
#include "EmuTime.hh"
#include <string>
#include <memory>

namespace openmsx {

class CassetteImage;
class MSXMixer;
class Scheduler;
class MSXCliComm;
class WavWriter;
class LoadingIndicator;
class BooleanSetting;
class TapeCommand;
class MSXCommandController;
class MSXEventDistributor;
class EventDistributor;

class CassettePlayer : public CassetteDevice, public SoundDevice, private EventListener
{
public:
	CassettePlayer(MSXCommandController& msxCommandController,
	               MSXMixer& mixer, Scheduler& Scheduler,
	               MSXEventDistributor& msxEventDistributor,
	               EventDistributor& eventDistributor,
	               MSXCliComm& cliComm);
	virtual ~CassettePlayer();

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
	enum State { PLAY, RECORD, STOP };
	State getState() const;
	void setState(State newState, const EmuTime& time);
	static std::string getStateString(State state);
	void setImageName(const std::string& newImage);
	const std::string& getImageName() const;
	void checkInvariants() const;

	void playTape(const std::string& filename, const EmuTime& time);
	void removeTape(const EmuTime& time);
	void recordTape(const std::string& filename, const EmuTime& time);
	void rewind(const EmuTime& time);
	void setMotorControl(bool status, const EmuTime& time);
	bool isRolling() const;
	void updateLoadingState();
	void updateAll(const EmuTime& time);
	void updatePlayPosition(const EmuTime& time);
	short getSample(const EmuTime& time);
	void fillBuf(size_t length, double x);
	void flushOutput();
	void autoRun();

        // EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	State state;
	std::auto_ptr<CassetteImage> playImage;
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

	std::auto_ptr<WavWriter> recordImage;

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
