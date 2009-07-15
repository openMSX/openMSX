// $Id$

#ifndef LASERDISCPLAYER_HH
#define LASERDISCPLAYER_HH

#include "SoundDevice.hh"
#include "Resample.hh"
#include "Filename.hh"
#include "EmuTime.hh"
#include "Schedulable.hh"
#include "DynamicClock.hh"
#include "Clock.hh"
#include "IRQHelper.hh"
#include "LDRenderer.hh"
#include "VideoSystemChangeListener.hh"
#include "OggReader.hh"

namespace openmsx {

class LaserdiscCommand;
class PioneerLDControl;
class MSXMotherBoard;

class LaserdiscPlayer : public SoundDevice
		      , public Schedulable
		      , private Resample
		      , private VideoSystemChangeListener
{
public:
	LaserdiscPlayer(MSXMotherBoard& motherBoard_, PioneerLDControl& ldcontrol_);
	~LaserdiscPlayer();

	// Called from CassettePort
	short readSample(EmuTime::param time);

	const std::string& getName() const;
	const std::string& getDescription() const;

	// SoundDevice
	void setOutputRate(unsigned sampleRate);
	void generateChannels(int** bufs, unsigned num);
	bool updateBuffer(unsigned length, int *buffer,
		EmuTime::param time, EmuDuration::param sampDur);
	bool generateInput(int *buffer, unsigned num);

	void sync(EmuTime::param time);

	// Schedulable
	const std::string& schedName() const;
	void executeUntil(openmsx::EmuTime, int);

	// video interface
	MSXMotherBoard& getMotherBoard() { return motherBoard; }

private:
	/** Mute the left and/or right audio channel
	  */
	void setMuting(bool left, bool right, EmuTime::param time);
	void setImageName(const Filename& newImage, EmuTime::param time);
	const Filename& getImageName() const;

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
	bool extack(EmuTime::param time);
	void extcontrol(bool bit, EmuTime::param time);
	bool extint(EmuTime::param time);

	void button(unsigned custom, unsigned code, EmuTime::param time);
	void button_repeat(EmuTime::param time);
	void setack(EmuTime::param time, int wait);
	unsigned getCurrentSample(EmuTime::param time);
	void createRenderer();

	bool mute_left, mute_right;
	Filename filename;

	const std::auto_ptr<LaserdiscCommand> laserdiscCommand;
	std::auto_ptr<OggReader> video;

	friend class LaserdiscCommand;
	friend class PioneerLDControl;

	// VideoSystemChangeListener interface:
	MSXMotherBoard& motherBoard;
	PioneerLDControl& ldcontrol;

	void preVideoSystemChange();
	void postVideoSystemChange();
	std::auto_ptr<LDRenderer> renderer;

	// Audio state
	short tapein;
	unsigned outputRate;
	unsigned playingFromSample;
	DynamicClock sampleClock;
	Clock<30000, 1001> frameClock;

	enum SyncType {
		FRAME,
		ACK
	};

	// Ext Control
	enum RemoteState {
		REMOTE_GAP,
		REMOTE_HEADER_PULSE,
		REMOTE_HEADER_SPACE,
		REMOTE_BITS_PULSE,
		REMOTE_BITS_SPACE,
		REMOTE_REPEAT_PULSE
	} remote_state;
	bool remote_last_bit;
	unsigned remote_bitno;
	unsigned remote_bits;
	EmuTime remote_last_edge;
	
	/* We need to maintain some state for seeking */
	enum PioneerSeekState {
		SEEK_NONE,
		SEEK_CHAPTER_BEGIN,
		SEEK_CHAPTER_END,
		SEEK_FRAME_BEGIN,
		SEEK_FRAME_END
	} seek_state;

	/* The specific frame or chapter we are seeking to */
	unsigned seek_no;

	// For ack
	bool ack;

	// State of the video itself
	bool seeking;

	enum PlayerState {
		PLAYER_STOPPED,
		PLAYER_PLAYING,
		PLAYER_PAUSED,
		PLAYER_FROZEN
	} player_state;
};

} // namespace openmsx

#endif
