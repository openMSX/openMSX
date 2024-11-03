#ifndef CASSETTEPLAYER_HH
#define CASSETTEPLAYER_HH

#include "CassetteDevice.hh"
#include "ResampledSoundDevice.hh"
#include "MSXMotherBoard.hh"
#include "CassettePlayerCommand.hh"
#include "Schedulable.hh"
#include "ThrottleManager.hh"
#include "Filename.hh"
#include "EmuTime.hh"
#include "BooleanSetting.hh"
#include "outer.hh"
#include "serialize_meta.hh"
#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace openmsx {

class CassetteImage;
class HardwareConfig;
class Wav8Writer;

class CassettePlayer final : public CassetteDevice, public ResampledSoundDevice
                           , public MediaInfoProvider
{
public:
	static constexpr std::string_view TAPE_RECORDING_DIR = "taperecordings";
	static constexpr std::string_view TAPE_RECORDING_EXTENSION = ".wav";

public:
	explicit CassettePlayer(const HardwareConfig& hwConf);
	~CassettePlayer() override;

	// CassetteDevice
	void setMotor(bool status, EmuTime::param time) override;
	int16_t readSample(EmuTime::param time) override;
	void setSignal(bool output, EmuTime::param time) override;

	// Pluggable
	std::string_view getName() const override;
	std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	// SoundDevice
	void generateChannels(std::span<float*> buffers, unsigned num) override;
	float getAmplificationFactorImpl() const override;

	// MediaInfoProvider
	void getMediaInfo(TclObject& result) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	enum class State { PLAY, RECORD, STOP };

	// methods to query the status of the player
	const Filename& getImageName() const { return casImage; }
	[[nodiscard]] State getState() const { return state; }
	[[nodiscard]] bool isMotorControlEnabled() const { return motorControl; }

	/** Returns the position of the tape, in seconds from the
	  * beginning of the tape. */
	double getTapePos(EmuTime::param time);

	/** Returns the length of the tape in seconds.
	  * When no tape is inserted, this returns 0. While recording this
	  * returns the current position (so while recording, tape length grows
	  * continuously). */
	double getTapeLength(EmuTime::param time);

	friend class CassettePlayerCommand;

private:
	[[nodiscard]] std::string getStateString() const;
	void setState(State newState, const Filename& newImage,
	              EmuTime::param time);
	void setImageName(const Filename& newImage);
	void checkInvariants() const;

	/** Insert a tape for use in PLAY mode.
	 */
	void playTape(const Filename& filename, EmuTime::param time);
	void insertTape(const Filename& filename, EmuTime::param time);

	/** Removes tape (possibly stops recording). And go to STOP mode.
	 */
	void removeTape(EmuTime::param time);

	/** Goes to RECORD mode using the given filename as a new tape
	  * image. Finishes any old recording session.
	  */
	void recordTape(const Filename& filename, EmuTime::param time);

	/** Rewinds the tape. Also sets PLAY mode, because you can't record
	  * over an existing tape. (And it won't be useful to implement that
	  * anyway.)
	  */
	void rewind(EmuTime::param time);
	void wind(EmuTime::param time);

	/** Enable or disable motor control.
	 */
	void setMotorControl(bool status, EmuTime::param time);

	/** True when the tape is rolling: not in STOP mode, AND [ motorControl
	  * is disabled OR motor is on ].
	  */
	bool isRolling() const;

	/** If motor, motorControl or state is changed, this method should
	  * be called to update the end-of-tape syncPoint and the loading
	  * indicator.
	  */
	void updateLoadingState(EmuTime::param time);

	/** Set the position of the tape, in seconds from the
	  * beginning of the tape. Clipped to [0, tape-length]. */
	void setTapePos(EmuTime::param time, double newPos);

	void sync(EmuTime::param time);
	void updateTapePosition(EmuDuration::param duration, EmuTime::param time);
	void generateRecordOutput(EmuDuration::param duration);

	void fillBuf(size_t length, double x);
	void flushOutput();
	void autoRun();

	// Schedulable
	struct SyncEndOfTape final : Schedulable {
		friend class CassettePlayer;
		explicit SyncEndOfTape(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param time) override {
			auto& cp = OUTER(CassettePlayer, syncEndOfTape);
			cp.execEndOfTape(time);
		}
	} syncEndOfTape;
	struct SyncAudioEmu final : Schedulable {
		friend class CassettePlayer;
		explicit SyncAudioEmu(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime::param time) override {
			auto& cp = OUTER(CassettePlayer, syncAudioEmu);
			cp.execSyncAudioEmu(time);
		}
	} syncAudioEmu;

	void execEndOfTape(EmuTime::param time);
	void execSyncAudioEmu(EmuTime::param time);
	EmuTime::param getCurrentTime() const { return syncEndOfTape.getCurrentTime(); }

	std::array<uint8_t, 1024> buf;

	double lastX; // last unfiltered output
	double lastY; // last filtered output
	double partialOut;
	double partialInterval;

	/** The time in the world of the tape. Zero at the start of the tape. */
	EmuTime tapePos = EmuTime::zero();

	/** Last time the sync() method was called.
	  * Used to calculate EmuDuration since last sync. */
	EmuTime prevSyncTime = EmuTime::zero();

	// SoundDevice
	unsigned audioPos = 0;
	Filename casImage;

	MSXMotherBoard& motherBoard;

	CassettePlayerCommand cassettePlayerCommand;

	LoadingIndicator loadingIndicator;
	BooleanSetting autoRunSetting;
	std::unique_ptr<Wav8Writer> recordImage;
	std::unique_ptr<CassetteImage> playImage;

	size_t sampCnt = 0;
	State state = State::STOP;
	bool lastOutput = false;
	bool motor = false, motorControl = true;
	bool syncScheduled = false;
};
SERIALIZE_CLASS_VERSION(CassettePlayer, 2);

} // namespace openmsx

#endif
