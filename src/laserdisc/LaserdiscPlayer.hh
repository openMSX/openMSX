// $Id$

#ifndef LASERDISCPLAYER_HH
#define LASERDISCPLAYER_HH

#include "SoundDevice.hh"
#include "Resample.hh"
#include "EmuTime.hh"
#include "Schedulable.hh"
#include "DynamicClock.hh"
#include "Clock.hh"
#include "VideoSystemChangeListener.hh"

namespace openmsx {

class LaserdiscCommand;
class PioneerLDControl;
class MSXMotherBoard;
class OggReader;
class AudioFragment;
class LDRenderer;
class RawFrame;

class LaserdiscPlayer : public SoundDevice
		      , public Schedulable
		      , private Resample
		      , private VideoSystemChangeListener
{
public:
	LaserdiscPlayer(MSXMotherBoard& motherBoard, PioneerLDControl& ldcontrol);
	~LaserdiscPlayer();

	// Called from CassettePort
	short readSample(EmuTime::param time);

	// Called from PioneerLDControl
	void setMuting(bool left, bool right, EmuTime::param time);
	bool extAck(EmuTime::param time) const;
	void extControl(bool bit, EmuTime::param time);
	const RawFrame* getRawFrame() const;

	// video interface
	MSXMotherBoard& getMotherBoard() { return motherBoard; }

private:
	void setImageName(const std::string& newImage, EmuTime::param time);

	/** Laserdisc player commands
	  */
	void play(EmuTime::param time);
	void pause(EmuTime::param time);
	void stop(EmuTime::param time);
	void seekFrame(int frame, EmuTime::param time);
	void seekChapter(int chapter, EmuTime::param time);

	// Control from MSX

	/** Is video output being generated?
	  */
	bool isVideoOutputAvailable(EmuTime::param time);
	bool extInt(EmuTime::param time);
	void remoteButtonLD1100(unsigned code, EmuTime::param time);
	void remoteButtonNEC(unsigned custom, unsigned code, EmuTime::param time);
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
	const std::string& schedName() const;
	void executeUntil(EmuTime::param time, int userData);

	// VideoSystemChangeListener interface:
	void preVideoSystemChange();
	void postVideoSystemChange();

	MSXMotherBoard& motherBoard;
	PioneerLDControl& ldcontrol;

	const std::auto_ptr<LaserdiscCommand> laserdiscCommand;
	std::auto_ptr<OggReader> video;
	std::auto_ptr<LDRenderer> renderer;

	void nextFrame(EmuTime::param time);
	void setFrameStep();
	unsigned currentFrame;
	unsigned frameStep;

	// Audio state
	DynamicClock sampleClock;
	EmuTime start;
	unsigned outputRate;
	unsigned playingFromSample;
	unsigned lastPlayedSample;
	bool muteLeft, muteRight;

	Clock<30000, 1001> frameClock;

	enum SyncType {
		FRAME,
		ACK
	};

	// Ext Control
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
	} remoteState;
	EmuTime remoteLastEdge;
	unsigned remoteBitNr;
	unsigned remoteBits;
	bool remoteLastBit;
	EmuTime lastNECButtonTime;
	unsigned lastNECButtonCode;

	/* We need to maintain some state for seeking */
	enum PioneerSeekState {
		SEEK_NONE,
		SEEK_CHAPTER,
		SEEK_FRAME,
		SEEK_WAIT,
	} seekState;

	/* frame the MSX has requested to wait for */
	unsigned waitFrame;

	/* The specific frame or chapter we are seeking to */
	unsigned seekNum;

	// For ack
	bool ack;
	bool foundFrame;

	// State of the video itself
	bool seeking;

	enum PlayerState {
		PLAYER_STOPPED,
		PLAYER_PLAYING,
		PLAYER_PLAYING_MULTISPEED,
		PLAYER_PAUSED,
		PLAYER_FROZEN
	} playerState;

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

	bool getFirstFrame;

	friend class LaserdiscCommand;
};

} // namespace openmsx

#endif
