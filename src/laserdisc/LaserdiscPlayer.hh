#ifndef LASERDISCPLAYER_HH
#define LASERDISCPLAYER_HH

#include "ResampledSoundDevice.hh"
#include "MSXMotherBoard.hh"
#include "BooleanSetting.hh"
#include "RecordedCommand.hh"
#include "EmuTime.hh"
#include "Schedulable.hh"
#include "DynamicClock.hh"
#include "Filename.hh"
#include "OggReader.hh"
#include "VideoSystemChangeListener.hh"
#include "EventListener.hh"
#include "ThrottleManager.hh"
#include "outer.hh"
#include <memory>
#include <optional>

namespace openmsx {

class PioneerLDControl;
class HardwareConfig;
class LDRenderer;
class RawFrame;

class LaserdiscPlayer final : public ResampledSoundDevice
                            , private EventListener
                            , private VideoSystemChangeListener
                            , public MediaInfoProvider
{
public:
	LaserdiscPlayer(const HardwareConfig& hwConf,
			PioneerLDControl& ldControl);
	~LaserdiscPlayer();

	// Called from CassettePort
	[[nodiscard]] int16_t readSample(EmuTime::param time);

	// Called from PioneerLDControl
	void setMuting(bool left, bool right, EmuTime::param time);
	[[nodiscard]] bool extAck(EmuTime::param /*time*/) const { return ack; }
	void extControl(bool bit, EmuTime::param time);
	[[nodiscard]] const RawFrame* getRawFrame() const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	// video interface
	[[nodiscard]] MSXMotherBoard& getMotherBoard() { return motherBoard; }

	// MediaInfoProvider
	void getMediaInfo(TclObject& result) override;

	enum class RemoteState {
		IDLE,
		HEADER_PULSE,
		NEC_HEADER_SPACE,
		NEC_BITS_PULSE,
		NEC_BITS_SPACE,
	};

	enum class PlayerState {
		STOPPED,
		PLAYING,
		MULTI_SPEED,
		PAUSED,
		STILL
	};

	enum class SeekState {
		NONE,
		CHAPTER,
		FRAME,
		WAIT,
	};

	enum class StereoMode {
		LEFT,
		RIGHT,
		STEREO
	};

	enum class RemoteProtocol {
		NONE,
		NEC,
	};
private:
	std::string getStateString() const;
	void setImageName(std::string newImage, EmuTime::param time);
	[[nodiscard]] const Filename& getImageName() const { return oggImage; }
	void autoRun();

	/** Laserdisc player commands
	  */
	void play(EmuTime::param time);
	void pause(EmuTime::param time);
	void stop(EmuTime::param time);
	void eject(EmuTime::param time);
	void seekFrame(size_t frame, EmuTime::param time);
	void stepFrame(bool forwards);
	void seekChapter(int chapter, EmuTime::param time);

	// Control from MSX

	/** Is video output being generated?
	  */
	void scheduleDisplayStart(EmuTime::param time);
	[[nodiscard]] bool isVideoOutputAvailable(EmuTime::param time);
	void remoteButtonNEC(uint8_t code, EmuTime::param time);
	void submitRemote(RemoteProtocol protocol, uint8_t code);
	void setAck(EmuTime::param time, int wait);
	[[nodiscard]] size_t getCurrentSample(EmuTime::param time);
	void createRenderer();

	// SoundDevice
	void generateChannels(std::span<float*> buffers, unsigned num) override;
	bool updateBuffer(size_t length, float* buffer,
	                  EmuTime::param time) override;
	[[nodiscard]] float getAmplificationFactorImpl() const override;

	// Schedulable
	struct SyncAck final : public Schedulable {
		friend class LaserdiscPlayer;
		explicit SyncAck(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param time) override {
			auto& player = OUTER(LaserdiscPlayer, syncAck);
			player.execSyncAck(time);
		}
	} syncAck;
	struct SyncOdd final : public Schedulable {
		friend class LaserdiscPlayer;
		explicit SyncOdd(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param time) override {
			auto& player = OUTER(LaserdiscPlayer, syncOdd);
			player.execSyncFrame(time, true);
		}
	} syncOdd;
	struct SyncEven final : public Schedulable {
		friend class LaserdiscPlayer;
		explicit SyncEven(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param time) override {
			auto& player = OUTER(LaserdiscPlayer, syncEven);
			player.execSyncFrame(time, false);
		}
	} syncEven;

	void execSyncAck(EmuTime::param time);
	void execSyncFrame(EmuTime::param time, bool odd);
	[[nodiscard]] EmuTime::param getCurrentTime() const { return syncAck.getCurrentTime(); }

	// EventListener
	bool signalEvent(const Event& event) override;

	// VideoSystemChangeListener interface:
	void preVideoSystemChange() noexcept override;
	void postVideoSystemChange() noexcept override;

	MSXMotherBoard& motherBoard;
	PioneerLDControl& ldControl;

	struct Command final : RecordedCommand {
		Command(CommandController& commandController,
		        StateChangeDistributor& stateChangeDistributor,
		        Scheduler& scheduler);
		void execute(std::span<const TclObject> tokens, TclObject& result,
			     EmuTime::param time) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} laserdiscCommand;

	std::optional<OggReader> video;
	Filename oggImage;
	std::unique_ptr<LDRenderer> renderer;

	void nextFrame(EmuTime::param time);
	void setFrameStep();
	size_t currentFrame;
	int frameStep;

	// Audio state
	DynamicClock sampleClock{EmuTime::zero()};
	EmuTime start = EmuTime::zero();
	size_t playingFromSample;
	size_t lastPlayedSample;
	bool muteLeft = false, muteRight = false;
	StereoMode stereoMode;

	// Ext Control
	RemoteState remoteState = RemoteState::IDLE;
	EmuTime remoteLastEdge = EmuTime::zero();
	unsigned remoteBitNr;
	unsigned remoteBits;
	bool remoteLastBit = false;
	RemoteProtocol remoteProtocol = RemoteProtocol::NONE;
	uint8_t remoteCode;
	bool remoteExecuteDelayed;
	// Number of v-blank since code was sent
	int remoteVblanksBack;

	/* We need to maintain some state for seeking */
	SeekState seekState;

	/* frame the MSX has requested to wait for */
	size_t waitFrame;

	// pause playing back on reaching wait frame
	bool stillOnWaitFrame;

	/* The specific frame or chapter we are seeking to */
	int seekNum;

	// For ack
	bool ack = false;

	// State of the video itself
	bool seeking = false;

	PlayerState playerState = PlayerState::STOPPED;

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
	BooleanSetting autoRunSetting;
	LoadingIndicator loadingIndicator;
	int sampleReads = 0;
};

SERIALIZE_CLASS_VERSION(LaserdiscPlayer, 4);

} // namespace openmsx

#endif
