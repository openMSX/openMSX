#ifndef CASSETTEPLAYER_HH
#define CASSETTEPLAYER_HH

#include "CassetteDevice.hh"
#include "CassettePlayerCommand.hh"

#include "BooleanSetting.hh"
#include "EmuTime.hh"
#include "Filename.hh"
#include "MSXMotherBoard.hh"
#include "ResampledSoundDevice.hh"
#include "Schedulable.hh"
#include "ThrottleManager.hh"

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
                           , public MediaProvider
{
public:
	static constexpr std::string_view TAPE_RECORDING_DIR = "taperecordings";
	static constexpr std::string_view TAPE_RECORDING_EXTENSION = ".wav";

public:
	explicit CassettePlayer(HardwareConfig& hwConf);
	~CassettePlayer() override;

	// CassetteDevice
	void setMotor(bool status, EmuTime time) override;
	int16_t readSample(EmuTime time) override;
	void setSignal(bool output, EmuTime time) override;

	// Pluggable
	zstring_view getName() const override;
	zstring_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;

	// SoundDevice
	void generateChannels(std::span<float*> buffers, unsigned num) override;
	float getAmplificationFactorImpl() const override;

	// MediaInfoProvider
	void getMediaInfo(TclObject& result) override;
	void setMedia(const TclObject& info, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	enum class State : uint8_t { PLAY, RECORD, STOP };

	// methods to query the status of the player
	const Filename& getImageName() const { return casImage; }
	[[nodiscard]] State getState() const { return state; }
	[[nodiscard]] bool isMotorControlEnabled() const { return motorControl; }

	/** Returns the position of the tape, in seconds from the
	  * beginning of the tape. */
	double getTapePos(EmuTime time);

	/** Returns the length of the tape in seconds.
	  * When no tape is inserted, this returns 0. While recording this
	  * returns the current position (so while recording, tape length grows
	  * continuously). */
	double getTapeLength(EmuTime time);

	friend class CassettePlayerCommand;

private:
	[[nodiscard]] std::string getStateString() const;
	void setState(State newState, const Filename& newImage,
	              EmuTime time);
	void setImageName(const Filename& newImage);
	void checkInvariants() const;

	/** Insert a tape for use in PLAY mode.
	 */
	void playTape(const Filename& filename, EmuTime time);
	void insertTape(const Filename& filename, EmuTime time);

	/** Removes tape (possibly stops recording). And go to STOP mode.
	 */
	void removeTape(EmuTime time);

	/** Goes to RECORD mode using the given filename as a new tape
	  * image. Finishes any old recording session.
	  */
	void recordTape(const Filename& filename, EmuTime time);

	/** Rewinds the tape. Also sets PLAY mode, because you can't record
	  * over an existing tape. (And it won't be useful to implement that
	  * anyway.)
	  */
	void rewind(EmuTime time);
	void wind(EmuTime time);

	/** Enable or disable motor control.
	 */
	void setMotorControl(bool status, EmuTime time);

	/** True when the tape is rolling: not in STOP mode, AND [ motorControl
	  * is disabled OR motor is on ].
	  */
	bool isRolling() const;

	/** If motor, motorControl or state is changed, this method should
	  * be called to update the end-of-tape syncPoint and the loading
	  * indicator.
	  */
	void updateLoadingState(EmuTime time);

	/** Set the position of the tape, in seconds from the
	  * beginning of the tape. Clipped to [0, tape-length]. */
	void setTapePos(EmuTime time, double newPos);

	void sync(EmuTime time);
	void updateTapePosition(EmuDuration duration, EmuTime time);
	void generateRecordOutput(EmuDuration duration);

	void fillBuf(size_t length, double x);
	void flushOutput();
	void autoRun();

	// Schedulable
	struct SyncEndOfTape final : Schedulable {
		friend class CassettePlayer;
		explicit SyncEndOfTape(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime time) override {
			auto& cp = OUTER(CassettePlayer, syncEndOfTape);
			cp.execEndOfTape(time);
		}
	} syncEndOfTape;
	struct SyncAudioEmu final : Schedulable {
		friend class CassettePlayer;
		explicit SyncAudioEmu(Scheduler& s) : Schedulable(s) {}
		void executeUntil(EmuTime time) override {
			auto& cp = OUTER(CassettePlayer, syncAudioEmu);
			cp.execSyncAudioEmu(time);
		}
	} syncAudioEmu;

	void execEndOfTape(EmuTime time);
	void execSyncAudioEmu(EmuTime time);
	EmuTime getCurrentTime() const { return syncEndOfTape.getCurrentTime(); }

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
