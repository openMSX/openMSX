// $Id$

#ifndef CASSETTEPLAYER_HH
#define CASSETTEPLAYER_HH

#include "EventListener.hh"
#include "CassetteDevice.hh"
#include "SoundDevice.hh"
#include "Resample.hh"
#include "Schedulable.hh"
#include "Filename.hh"
#include "EmuTime.hh"
#include "serialize_meta.hh"
#include <string>
#include <memory>

namespace openmsx {

class CassetteImage;
class MSXMixer;
class Scheduler;
class CliComm;
class WavWriter;
class LoadingIndicator;
class BooleanSetting;
class TapeCommand;
class CommandController;
class MSXEventDistributor;
class EventDistributor;

class CassettePlayer : public CassetteDevice, public SoundDevice
                     , private Resample, private EventListener
                     , private Schedulable
{
public:
	CassettePlayer(CommandController& commandController,
	               MSXMixer& mixer, Scheduler& Scheduler,
	               MSXEventDistributor& msxEventDistributor,
	               EventDistributor& eventDistributor,
	               CliComm& cliComm);
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
	virtual void setOutputRate(unsigned sampleRate);
	virtual void generateChannels(int** bufs, unsigned num);
	virtual bool updateBuffer(unsigned length, int* buffer,
		const EmuTime& time, const EmuDuration& sampDur);

	// Resample
	virtual bool generateInput(int* buffer, unsigned num);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	enum State { PLAY, RECORD, STOP }; // public for serialization

private:
	State getState() const;
	std::string getStateString() const;
	void setState(State newState, const Filename& newImage,
	              const EmuTime& time);
	void setImageName(const Filename& newImage);
	const Filename& getImageName() const;
	void checkInvariants() const;

	/** Insert a tape for use in PLAY mode.
	 */
	void playTape(const Filename& filename, const EmuTime& time);
	void insertTape(const Filename& filename);

	/** Removes tape (possibly stops recording). And go to STOP mode.
	 */
	void removeTape(const EmuTime& time);

	/** Goes to RECORD mode using the given filename as a new tape
	  * image. Finishes any old recording session.
	  */
	void recordTape(const Filename& filename, const EmuTime& time);

	/** Rewinds the tape. Also sets PLAY mode, because you can't record
	  * over an existing tape. (And it won't be useful to implement that
	  * anyway.)
	  */
	void rewind(const EmuTime& time);

	/** Enable or disable motor control.
	 */
	void setMotorControl(bool status, const EmuTime& time);

	/** True when the tape is rolling: not in STOP mode, AND [ motorcontrol
	  * is disabled OR motor is on ].
	  */
	bool isRolling() const;

	/** If motor, motorControl or state is changed, this method should
	  * be called to update the end-of-tape syncpoint and the loading
	  * indicator.
	  */
	void updateLoadingState(const EmuTime& time);

	/** Returns the position of the tape, in seconds from the
	  * beginning of the tape. */
	double getTapePos(const EmuTime& time);

	/** Returns the length of the tape in seconds.
	  * When no tape is inserted, this returns 0. While recording this
	  * returns the current position (so while recording, tape length grows
	  * continuously). */
	double getTapeLength(const EmuTime& time);

	void sync(const EmuTime& time);
	void updateTapePosition(const EmuDuration& duration, const EmuTime& time);
	void generateRecordOutput(const EmuDuration& duration);

	void fillBuf(size_t length, double x);
	void flushOutput();
	void autoRun();

	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	static const size_t BUF_SIZE = 1024;
	unsigned char buf[BUF_SIZE];

	double lastX; // last unfiltered output
	double lastY; // last filtered output
	double partialOut;
	double partialInterval;

	/** The time in the world of the tape. Zero at the start of the tape. */
	EmuTime tapePos;

	/** Last time the sync() method was called.
	  * Used to calculate EmuDuration since last sync. */
	EmuTime prevSyncTime;

	// SoundDevice
	unsigned audioPos;
	unsigned outputRate;
	Filename casImage;

	CommandController& commandController;
	CliComm& cliComm;
	EventDistributor& eventDistributor;

	const std::auto_ptr<TapeCommand> tapeCommand;
	const std::auto_ptr<LoadingIndicator> loadingIndicator;
	const std::auto_ptr<BooleanSetting> autoRunSetting;
	std::auto_ptr<WavWriter> recordImage;
	std::auto_ptr<CassetteImage> playImage;

	size_t sampcnt;
	State state;
	bool lastOutput;
	bool motor, motorControl;
	bool syncScheduled;

	friend class TapeCommand;
};

REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, CassettePlayer, "CassettePlayer");

} // namespace openmsx

#endif
