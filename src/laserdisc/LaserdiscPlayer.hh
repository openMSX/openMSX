// $Id$

#ifndef LASERDISCPLAYER_HH
#define LASERDISCPLAYER_HH

#include "BooleanSetting.hh"
#include "SoundDevice.hh"
#include "Resample.hh"
#include "EmuTime.hh"
#include "Schedulable.hh"
#include "DynamicClock.hh"
#include "EventDistributor.hh"
#include "Clock.hh"
#include "Filename.hh"
#include "VideoSystemChangeListener.hh"
#include "EventListener.hh"

namespace openmsx {

class LaserdiscCommand;
class PioneerLDControl;
class MSXMotherBoard;
class OggReader;
class BooleanSetting;
struct AudioFragment;
class LDRenderer;
class RawFrame;
class LoadingIndicator;
class ThrottleManager;
class EventDistributor;

class LaserdiscPlayer : public SoundDevice
		      , public Schedulable
		      , private Resample
		      , private EventListener
		      , private VideoSystemChangeListener
{
public:
	LaserdiscPlayer(MSXMotherBoard& motherBoard,
			PioneerLDControl& ldcontrol,
			ThrottleManager& throttleManager);
	~LaserdiscPlayer();

	// Called from CassettePort
	short readSample(EmuTime::param time);

	// Called from PioneerLDControl
	void setMuting(bool left, bool right, EmuTime::param time);
	bool extAck(EmuTime::param time) const;
	void extControl(bool bit, EmuTime::param time);
	const RawFrame* getRawFrame() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// video interface
	MSXMotherBoard& getMotherBoard() { return motherBoard; }

	enum RemoteState {
		REMOTE_IDLE,
		REMOTE_HEADER_PULSE,
		NEC_HEADER_SPACE,
		NEC_BITS_PULSE,
		NEC_BITS_SPACE,
		NEC_REPEAT_PULSE,
		LD1100_GAP,
		LD1100_SEEN_GAP,
		LD1100_BITS_SPACE,
		LD1100_BITS_PULSE
	};

	enum PlayerState {
		PLAYER_STOPPED,
		PLAYER_PLAYING,
		PLAYER_MULTISPEED,
		PLAYER_PAUSED,
		PLAYER_STILL
	};

	enum SeekState {
		SEEK_NONE,
		SEEK_CHAPTER,
		SEEK_FRAME,
		SEEK_WAIT,
	};

	enum StereoMode {
		LEFT,
		RIGHT,
		STEREO
	};

	enum RemoteProtocol {
		IR_NONE,
		IR_NEC,
		IR_LD1100
	};
private:
	void setImageName(const std::string& newImage, EmuTime::param time);
	virtual int signalEvent(shared_ptr<const Event> event);
	void autoRun();

	/** Laserdisc player commands
	  */
	void play(EmuTime::param time);
	void pause(EmuTime::param time);
	void stop(EmuTime::param time);
	void eject(EmuTime::param time);
	void seekFrame(int frame, EmuTime::param time);
	void stepFrame(bool);
	void seekChapter(int chapter, EmuTime::param time);

	// Control from MSX

	/** Is video output being generated?
	  */
	void scheduleDisplayStart(EmuTime::param time);
	bool isVideoOutputAvailable(EmuTime::param time);
	void remoteButtonLD1100(unsigned code, EmuTime::param time);
	void remoteButtonNEC(unsigned code, EmuTime::param time);
	void submitRemote(RemoteProtocol protocol, unsigned code);
	void buttonRepeat(EmuTime::param time);
	void setAck(EmuTime::param time, int wait);
	unsigned getCurrentSample(EmuTime::param time);
	void createRenderer();

	// SoundDevice
	void setOutputRate(unsigned sampleRate);
	void generateChannels(int** bufs, unsigned num);
	bool updateBuffer(unsigned length, int* buffer,
	                  EmuTime::param time, EmuDuration::param sampDur);
	bool generateInput(int* buffer, unsigned num);

	// Schedulable
	void executeUntil(EmuTime::param time, int userData);

	// VideoSystemChangeListener interface:
	void preVideoSystemChange();
	void postVideoSystemChange();

	MSXMotherBoard& motherBoard;
	PioneerLDControl& ldcontrol;
	EventDistributor& eventDistributor;

	const std::auto_ptr<LaserdiscCommand> laserdiscCommand;
	std::auto_ptr<OggReader> video;
	Filename oggImage;
	std::auto_ptr<LDRenderer> renderer;

	void nextFrame(EmuTime::param time);
	void setFrameStep();
	int currentFrame;
	int frameStep;

	// Audio state
	DynamicClock sampleClock;
	EmuTime start;
	unsigned outputRate;
	unsigned playingFromSample;
	unsigned lastPlayedSample;
	bool muteLeft, muteRight;
	StereoMode stereoMode;

	enum SyncType {
		FRAME,
		VBLANK,
		ACK
	};

	// Ext Control
	RemoteState remoteState;
	EmuTime remoteLastEdge;
	unsigned remoteBitNr;
	unsigned remoteBits;
	bool remoteLastBit;
	RemoteProtocol remoteProtocol;
	unsigned remoteCode;
	bool remoteExecuteDelayed;
	// Number of v-blank since code was sent
	int remoteVblanksBack;

	/* We need to maintain some state for seeking */
	SeekState seekState;

	/* frame the MSX has requested to wait for */
	int waitFrame;

	/* The specific frame or chapter we are seeking to */
	int seekNum;

	// For ack
	bool ack;

	// State of the video itself
	bool seeking;

	PlayerState playerState;

	enum PlayingSpeed {
		SPEED_STEP3 = -5,	// Each frame is repeated 90 times
		SPEED_STEP1 = -4,	// Each frame is repeated 30 times
		SPEED_1IN16 = -3,	// Each frame is repeated 16 times
		SPEED_1IN8 = -2,	// Each frame is repeated 8 times
		SPEED_1IN4 = -1,	// Each frame is repeated 4 times
		SPEED_1IN2 = 0,
		SPEED_X1 = 1,
		SPEED_X2 = 2,
		SPEED_X3 = 3
	};
	int playingSpeed;

	// Loading indicator
	const std::auto_ptr<BooleanSetting> autoRunSetting;
	const std::auto_ptr<LoadingIndicator> loadingIndicator;
	int sampleReads;

	friend class LaserdiscCommand;
};

} // namespace openmsx

#endif
