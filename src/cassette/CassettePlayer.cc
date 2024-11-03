// TODO:
// - improve consistency when a reset occurs: tape is removed when you were
//   recording, but it is not removed when you were playing
// - specify prefix for auto file name generation when recording (setting?)
// - append to existing wav files when recording (record command), but this is
//   basically a special case (pointer at the end) of:
// - (partly) overwrite an existing wav file from any given time index
// - seek in cassette images for the next and previous file (using empty space?)
// - (partly) overwrite existing wav files with new tape data (not very hi prio)
// - handle read-only cassette images (e.g.: CAS images or WAV files with a RO
//   flag): refuse to go to record mode when those are selected
// - smartly auto-set the position of tapes: if you insert an existing WAV
//   file, it will have the position at the start, assuming PLAY mode by
//   default.  When specifying record mode at insert (somehow), it should be
//   at the back.
//   Alternatively, we could remember the index in tape images by storing the
//   index in some persistent data file with its SHA1 sum as it was as we last
//   saw it. When there are write actions to the tape, the hash has to be
//   recalculated and replaced in the data file. An optimization would be to
//   first simply check on the length of the file and fall back to SHA1 if that
//   results in multiple matches.

#include "CassettePlayer.hh"
#include "Connector.hh"
#include "CassettePort.hh"
#include "CommandController.hh"
#include "DeviceConfig.hh"
#include "HardwareConfig.hh"
#include "XMLElement.hh"
#include "FileContext.hh"
#include "FilePool.hh"
#include "File.hh"
#include "ReverseManager.hh"
#include "WavImage.hh"
#include "CasImage.hh"
#include "MSXCliComm.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "GlobalSettings.hh"
#include "CommandException.hh"
#include "FileOperations.hh"
#include "WavWriter.hh"
#include "TclObject.hh"
#include "DynamicClock.hh"
#include "EmuDuration.hh"
#include "checked_cast.hh"
#include "narrow.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <algorithm>
#include <cassert>
#include <memory>

using std::string;

namespace openmsx {

// TODO: this description is not entirely accurate, but it is used
// as an identifier for this audio device in e.g. Catapult. We should
// use another way to identify audio devices A.S.A.P.!
static constexpr static_string_view DESCRIPTION = "Cassetteplayer, use to read .cas or .wav files.";

static constexpr unsigned DUMMY_INPUT_RATE = 44100; // actual rate depends on .cas/.wav file
static constexpr unsigned RECORD_FREQ = 44100;
static constexpr double RECIP_RECORD_FREQ = 1.0 / RECORD_FREQ;
static constexpr double OUTPUT_AMP = 60.0;

static std::string_view getCassettePlayerName()
{
	return "cassetteplayer";
}

CassettePlayer::CassettePlayer(const HardwareConfig& hwConf)
	: ResampledSoundDevice(hwConf.getMotherBoard(), getCassettePlayerName(), DESCRIPTION, 1, DUMMY_INPUT_RATE, false)
	, syncEndOfTape(hwConf.getMotherBoard().getScheduler())
	, syncAudioEmu (hwConf.getMotherBoard().getScheduler())
	, motherBoard(hwConf.getMotherBoard())
	, cassettePlayerCommand(
		this,
		motherBoard.getCommandController(),
		motherBoard.getStateChangeDistributor(),
		motherBoard.getScheduler())
	, loadingIndicator(
		motherBoard.getReactor().getGlobalSettings().getThrottleManager())
	, autoRunSetting(
		motherBoard.getCommandController(),
		"autoruncassettes", "automatically try to run cassettes", true)
{
	static const XMLElement* xml = [] {
		auto& doc = XMLDocument::getStaticDocument();
		XMLElement* result = doc.allocateElement("cassetteplayer");
		result->setFirstChild(doc.allocateElement("sound"))
		      ->setFirstChild(doc.allocateElement("volume", "5000"));
		return result;
	}();
	registerSound(DeviceConfig(hwConf, *xml));

	motherBoard.registerMediaInfo(getCassettePlayerName(), *this);
	motherBoard.getMSXCliComm().update(CliComm::UpdateType::HARDWARE, getCassettePlayerName(), "add");

	removeTape(EmuTime::zero());
}

CassettePlayer::~CassettePlayer()
{
	unregisterSound();
	if (auto* c = getConnector()) {
		c->unplug(getCurrentTime());
	}
	motherBoard.unregisterMediaInfo(*this);
	motherBoard.getMSXCliComm().update(CliComm::UpdateType::HARDWARE, getCassettePlayerName(), "remove");
}

void CassettePlayer::getMediaInfo(TclObject& result)
{
	result.addDictKeyValues("target", getImageName().getResolved(),
	                        "state", getStateString(),
	                        "position", getTapePos(getCurrentTime()),
	                        "length", getTapeLength(getCurrentTime()),
	                        "motorcontrol", motorControl);
}

void CassettePlayer::autoRun()
{
	if (!playImage) return;
	if (motherBoard.getReverseManager().isReplaying()) {
		// Don't execute the loading commands (keyboard type commands)
		// when we're replaying a recording. Because the recording
		// already contains those commands.
		return;
	}

	// try to automatically run the tape, if that's set
	CassetteImage::FileType type = playImage->getFirstFileType();
	if (!autoRunSetting.getBoolean() || type == CassetteImage::UNKNOWN) {
		return;
	}
	bool is_SVI = motherBoard.getMachineType() == "SVI"; // assume all other are 'MSX*' (might not be correct for 'Coleco')
	string H_READ = is_SVI ? "0xFE8E" : "0xFF07"; // Hook for Ready
	string H_MAIN = is_SVI ? "0xFE94" : "0xFF0C"; // Hook for Main Loop
	string instr1, instr2;
	switch (type) {
		case CassetteImage::ASCII:
			instr1 = R"({RUN\"CAS:\"\r})";
			break;
		case CassetteImage::BINARY:
			instr1 = R"({BLOAD\"CAS:\",R\r})";
			break;
		case CassetteImage::BASIC:
			// Note that CLOAD:RUN won't work: BASIC ignores stuff
			// after the CLOAD command. That's why it's split in two.
			instr1 = "{CLOAD\\r}";
			instr2 = "{RUN\\r}";
			break;
		default:
			UNREACHABLE; // Shouldn't be possible
	}
	string command = strCat(
		"namespace eval ::openmsx {\n"
		"  variable auto_run_bp\n"

		"  proc auto_run_cb {args} {\n"
		"    variable auto_run_bp\n"
		"    debug remove_bp $auto_run_bp\n"
		"    unset auto_run_bp\n"

		// Without the 0.2s delay here, the type command gets messed up
		// on MSX1 machines for some reason (starting to type too early?)
		// When using 0.1s delay only, the typing works, but still some
		// things go wrong on some machines with some games (see #1509
		// for instance)
		"    after time 0.2 \"type [lindex $args 0]\"\n"

		"    set next [lrange $args 1 end]\n"
		"    if {[llength $next] == 0} return\n"

		// H_READ is used by some firmwares; we need to hook the
		// H_MAIN that happens immediately after H_READ.
		"    set cmd \"openmsx::auto_run_cb $next\"\n"
		"    set openmsx::auto_run_bp [debug set_bp ", H_MAIN, " 1 \"$cmd\"]\n"
		"  }\n"

		"  if {[info exists auto_run_bp]} {debug remove_bp $auto_run_bp\n}\n"
		"  set auto_run_bp [debug set_bp ", H_READ, " 1 {\n"
		"    openmsx::auto_run_cb {{}} ", instr1, ' ', instr2, "\n"
		"  }]\n"

		// re-trigger hook(s), needed when already booted in BASIC
		"  type_via_keyboard \'\\r\n"
		"}");
	try {
		motherBoard.getCommandController().executeCommand(command);
	} catch (CommandException& e) {
		motherBoard.getMSXCliComm().printWarning(
			"Error executing loading instruction using command \"",
			command, "\" for AutoRun: ",
			e.getMessage(), "\n Please report a bug.");
	}
}

string CassettePlayer::getStateString() const
{
	switch (getState()) {
		using enum State;
		case PLAY:   return "play";
		case RECORD: return "record";
		case STOP:   return "stop";
	}
	UNREACHABLE;
}

bool CassettePlayer::isRolling() const
{
	// Is the tape 'rolling'?
	// is true when:
	//  not in stop mode (there is a tape inserted and not at end-of-tape)
	//  AND [ user forced playing (motorControl=off) OR motor enabled by
	//        software (motor=on) ]
	return (getState() != State::STOP) && (motor || !motorControl);
}

double CassettePlayer::getTapePos(EmuTime::param time)
{
	sync(time);
	if (getState() == State::RECORD) {
		// we record 8-bit mono, so bytes == samples
		return (double(recordImage->getBytes()) + partialInterval) * RECIP_RECORD_FREQ;
	} else {
		return (tapePos - EmuTime::zero()).toDouble();
	}
}

void CassettePlayer::setTapePos(EmuTime::param time, double newPos)
{
	assert(getState() != State::RECORD);
	sync(time);
	auto pos = std::clamp(newPos, 0.0, getTapeLength(time));
	tapePos = EmuTime::zero() + EmuDuration(pos);
	wind(time);
}

double CassettePlayer::getTapeLength(EmuTime::param time)
{
	if (playImage) {
		return (playImage->getEndTime() - EmuTime::zero()).toDouble();
	} else if (getState() == State::RECORD) {
		return getTapePos(time);
	} else {
		return 0.0;
	}
}

void CassettePlayer::checkInvariants() const
{
	switch (getState()) {
	case State::STOP:
		assert(!recordImage);
		if (playImage) {
			// we're at end-of tape
			assert(!getImageName().empty());
		} else {
			// no tape inserted, imageName may or may not be empty
		}
		break;
	case State::PLAY:
		assert(!getImageName().empty());
		assert(!recordImage);
		assert(playImage);
		break;
	case State::RECORD:
		assert(!getImageName().empty());
		assert(recordImage);
		assert(!playImage);
		break;
	default:
		UNREACHABLE;
	}
}

void CassettePlayer::setState(State newState, const Filename& newImage,
                              EmuTime::param time)
{
	sync(time);

	// set new state if different from old state
	State oldState = getState();
	if (oldState == newState) return;

	// cannot directly switch from PLAY to RECORD or vice-versa,
	// (should always go via STOP)
	assert(!((oldState == State::PLAY)   && (newState == State::RECORD)));
	assert(!((oldState == State::RECORD) && (newState == State::PLAY)));

	// stuff for leaving the old state
	//  'recordImage==nullptr' can happen in case of loadstate.
	if ((oldState == State::RECORD) && recordImage) {
		flushOutput();
		bool empty = recordImage->isEmpty();
		recordImage.reset();
		if (empty) {
			// delete the created WAV file, as it is useless
			FileOperations::unlink(getImageName().getResolved()); // ignore errors
			setImageName(Filename());
		}
	}

	// actually switch state
	state = newState;
	setImageName(newImage);

	// stuff for entering the new state
	if (newState == State::RECORD) {
		partialOut = 0.0;
		partialInterval = 0.0;
		lastX = lastOutput ? OUTPUT_AMP : -OUTPUT_AMP;
		lastY = 0.0;
	}
	motherBoard.getMSXCliComm().update(
		CliComm::UpdateType::STATUS, "cassetteplayer", getStateString());

	updateLoadingState(time); // sets SP for tape-end detection

	checkInvariants();
}

void CassettePlayer::updateLoadingState(EmuTime::param time)
{
	assert(prevSyncTime == time); // sync() must be called
	// TODO also set loadingIndicator for RECORD?
	// note: we don't use isRolling()
	loadingIndicator.update(motor && (getState() == State::PLAY));

	syncEndOfTape.removeSyncPoint();
	if (isRolling() && (getState() == State::PLAY)) {
		syncEndOfTape.setSyncPoint(time + (playImage->getEndTime() - tapePos));
	}
}

void CassettePlayer::setImageName(const Filename& newImage)
{
	casImage = newImage;
	motherBoard.getMSXCliComm().update(
		CliComm::UpdateType::MEDIA, "cassetteplayer", casImage.getResolved());
}

void CassettePlayer::insertTape(const Filename& filename, EmuTime::param time)
{
	if (!filename.empty()) {
		FilePool& filePool = motherBoard.getReactor().getFilePool();
		try {
			// first try WAV
			playImage = std::make_unique<WavImage>(filename, filePool);
		} catch (MSXException& e) {
			try {
				// if that fails use CAS
				playImage = std::make_unique<CasImage>(
					filename, filePool,
					motherBoard.getMSXCliComm());
			} catch (MSXException& e2) {
				throw MSXException(
					"Failed to insert WAV image: \"",
					e.getMessage(),
					"\" and also failed to insert CAS image: \"",
					e2.getMessage(), '\"');
			}
		}
	} else {
		// This is a bit tricky, consider this scenario: we switch from
		// RECORD->PLAY, but we didn't actually record anything: The
		// removeTape() call above (indirectly) deletes the empty
		// recorded wav image and also clears imageName. Now because
		// the 'filename' parameter is passed by reference, and because
		// getImageName() returns a reference, this 'filename'
		// parameter now also is an empty string.
	}

	// possibly recreate resampler
	if (unsigned inputRate = playImage ? playImage->getFrequency() : 44100;
	    inputRate != getInputRate()) {
		setInputRate(inputRate);
		createResampler();
	}

	// trigger (re-)query of getAmplificationFactorImpl()
	setSoftwareVolume(1.0f, time);

	setImageName(filename);
}

void CassettePlayer::playTape(const Filename& filename, EmuTime::param time)
{
	// Temporally go to STOP state:
	// RECORD: First close the recorded image. Otherwise it goes wrong
	//         if you switch from RECORD->PLAY on the same image.
	// PLAY: Go to stop because we temporally violate some invariants
	//       (tapePos can be beyond end-of-tape).
	setState(State::STOP, getImageName(), time); // keep current image
	insertTape(filename, time);
	rewind(time); // sets PLAY mode
}

void CassettePlayer::rewind(EmuTime::param time)
{
	sync(time); // before tapePos changes
	assert(getState() != State::RECORD);
	tapePos = EmuTime::zero();
	audioPos = 0;
	wind(time);
	autoRun();
}

void CassettePlayer::wind(EmuTime::param time)
{
	if (getImageName().empty()) {
		// no image inserted, do nothing
		assert(getState() == State::STOP);
	} else {
		// keep current image
		setState(State::PLAY, getImageName(), time);
	}
	updateLoadingState(time);
}

void CassettePlayer::recordTape(const Filename& filename, EmuTime::param time)
{
	removeTape(time); // flush (possible) previous recording
	recordImage = std::make_unique<Wav8Writer>(filename, 1, RECORD_FREQ);
	tapePos = EmuTime::zero();
	setState(State::RECORD, filename, time);
}

void CassettePlayer::removeTape(EmuTime::param time)
{
	// first stop with tape still inserted
	setState(State::STOP, getImageName(), time);
	// then remove the tape
	playImage.reset();
	tapePos = EmuTime::zero();
	setImageName({});
}

void CassettePlayer::setMotor(bool status, EmuTime::param time)
{
	if (status != motor) {
		sync(time);
		motor = status;
		updateLoadingState(time);
	}
}

void CassettePlayer::setMotorControl(bool status, EmuTime::param time)
{
	if (status != motorControl) {
		sync(time);
		motorControl = status;
		updateLoadingState(time);
	}
}

int16_t CassettePlayer::readSample(EmuTime::param time)
{
	if (getState() == State::PLAY) {
		// playing
		sync(time);
		return isRolling() ? playImage->getSampleAt(tapePos) : int16_t(0);
	} else {
		// record or stop
		return 0;
	}
}

void CassettePlayer::setSignal(bool output, EmuTime::param time)
{
	sync(time);
	lastOutput = output;
}

void CassettePlayer::sync(EmuTime::param time)
{
	EmuDuration duration = time - prevSyncTime;
	prevSyncTime = time;

	updateTapePosition(duration, time);
	generateRecordOutput(duration);
}

void CassettePlayer::updateTapePosition(
	EmuDuration::param duration, EmuTime::param time)
{
	if (!isRolling() || (getState() != State::PLAY)) return;

	tapePos += duration;
	assert(tapePos <= playImage->getEndTime());

	// synchronize audio with actual tape position
	if (!syncScheduled) {
		// don't sync too often, this improves sound quality
		syncScheduled = true;
		syncAudioEmu.setSyncPoint(time + EmuDuration::sec(1));
	}
}

void CassettePlayer::generateRecordOutput(EmuDuration::param duration)
{
	if (!recordImage || !isRolling()) return;

	double out = lastOutput ? OUTPUT_AMP : -OUTPUT_AMP;
	double samples = duration.toDouble() * RECORD_FREQ;
	if (auto rest = 1.0 - partialInterval; rest <= samples) {
		// enough to fill next interval
		partialOut += out * rest;
		fillBuf(1, partialOut);
		samples -= rest;

		// fill complete intervals
		auto count = int(samples);
		if (count > 0) {
			fillBuf(count, out);
		}
		samples -= count;
		assert(samples < 1.0);

		// partial last interval
		partialOut = samples * out;
		partialInterval = samples;
	} else {
		assert(samples < 1.0);
		partialOut += samples * out;
		partialInterval += samples;
	}
	assert(partialInterval < 1.0);
}

void CassettePlayer::fillBuf(size_t length, double x)
{
	assert(recordImage);
	static constexpr double A = 252.0 / 256.0;

	double y = lastY + (x - lastX);

	while (length) {
		size_t len = std::min(length, buf.size() - sampCnt);
		repeat(len, [&] {
			buf[sampCnt++] = narrow<uint8_t>(int(y) + 128);
			y *= A;
		});
		length -= len;
		assert(sampCnt <= buf.size());
		if (sampCnt == buf.size()) {
			flushOutput();
		}
	}
	lastY = y;
	lastX = x;
}

void CassettePlayer::flushOutput()
{
	try {
		recordImage->write(subspan(buf, 0, sampCnt));
		sampCnt = 0;
		recordImage->flush(); // update wav header
	} catch (MSXException& e) {
		motherBoard.getMSXCliComm().printWarning(
			"Failed to write to tape: ", e.getMessage());
	}
}


std::string_view CassettePlayer::getName() const
{
	return getCassettePlayerName();
}

std::string_view CassettePlayer::getDescription() const
{
	return DESCRIPTION;
}

void CassettePlayer::plugHelper(Connector& conn, EmuTime::param time)
{
	sync(time);
	lastOutput = checked_cast<CassettePort&>(conn).lastOut();
}

void CassettePlayer::unplugHelper(EmuTime::param time)
{
	// note: may not throw exceptions
	setState(State::STOP, getImageName(), time); // keep current image
}


void CassettePlayer::generateChannels(std::span<float*> buffers, unsigned num)
{
	// Single channel device: replace content of buffers[0] (not add to it).
	assert(buffers.size() == 1);
	if ((getState() != State::PLAY) || !isRolling()) {
		buffers[0] = nullptr;
		return;
	}
	assert(buffers.size() == 1);
	playImage->fillBuffer(audioPos, buffers.first<1>(), num);
	audioPos += num;
}

float CassettePlayer::getAmplificationFactorImpl() const
{
	return playImage ? playImage->getAmplificationFactorImpl() : 1.0f;
}

void CassettePlayer::execEndOfTape(EmuTime::param time)
{
	// tape ended
	sync(time);
	assert(tapePos == playImage->getEndTime());
	motherBoard.getMSXCliComm().printWarning(
		"Tape end reached... stopping. "
		"You may need to insert another tape image "
		"that contains side B. (Or you used the wrong "
		"loading command.)");
	setState(State::STOP, getImageName(), time); // keep current image
}

void CassettePlayer::execSyncAudioEmu(EmuTime::param time)
{
	if (getState() == State::PLAY) {
		updateStream(time);
		sync(time);
		DynamicClock clk(EmuTime::zero());
		clk.setFreq(playImage->getFrequency());
		audioPos = clk.getTicksTill(tapePos);
	}
	syncScheduled = false;
}

static constexpr std::initializer_list<enum_string<CassettePlayer::State>> stateInfo = {
	{ "PLAY",   CassettePlayer::State::PLAY   },
	{ "RECORD", CassettePlayer::State::RECORD },
	{ "STOP",   CassettePlayer::State::STOP   }
};
SERIALIZE_ENUM(CassettePlayer::State, stateInfo);

// version 1: initial version
// version 2: added checksum
template<typename Archive>
void CassettePlayer::serialize(Archive& ar, unsigned version)
{
	if (recordImage) {
		// buf, sampCnt
		flushOutput();
	}

	ar.serialize("casImage", casImage);

	Sha1Sum oldChecksum;
	if constexpr (!Archive::IS_LOADER) {
		if (playImage) {
			oldChecksum = playImage->getSha1Sum();
		}
	}
	if (ar.versionAtLeast(version, 2)) {
		string oldChecksumStr = oldChecksum.empty()
		                      ? string{}
		                      : oldChecksum.toString();
		ar.serialize("checksum", oldChecksumStr);
		oldChecksum = oldChecksumStr.empty()
		            ? Sha1Sum()
		            : Sha1Sum(oldChecksumStr);
	}

	if constexpr (Archive::IS_LOADER) {
		FilePool& filePool = motherBoard.getReactor().getFilePool();
		auto time = getCurrentTime();
		casImage.updateAfterLoadState();
		if (!oldChecksum.empty() &&
		    !FileOperations::exists(casImage.getResolved())) {
			auto file = filePool.getFile(FileType::TAPE, oldChecksum);
			if (file.is_open()) {
				casImage.setResolved(file.getURL());
			}
		}
		try {
			insertTape(casImage, time);
		} catch (MSXException&) {
			if (oldChecksum.empty()) {
				// It's OK if we cannot reinsert an empty
				// image. One likely scenario for this case is
				// the following:
				//  - cassetteplayer new myfile.wav
				//  - don't actually start saving to tape yet
				//  - create a savestate and load that state
				// Because myfile.wav contains no data yet, it
				// is deleted from the filesystem. So on a
				// loadstate it won't be found.
			} else {
				throw;
			}
		}

		if (playImage && !oldChecksum.empty()) {
			Sha1Sum newChecksum = playImage->getSha1Sum();
			if (oldChecksum != newChecksum) {
				motherBoard.getMSXCliComm().printWarning(
					"The content of the tape ",
					casImage.getResolved(),
					" has changed since the time this "
					"savestate was created. This might "
					"result in emulation problems.");
			}
		}
	}

	// only for RECORD
	//double lastX;
	//double lastY;
	//double partialOut;
	//double partialInterval;
	//std::unique_ptr<WavWriter> recordImage;

	ar.serialize("tapePos",      tapePos,
	             "prevSyncTime", prevSyncTime,
	             "audioPos",     audioPos,
	             "state",        state,
	             "lastOutput",   lastOutput,
	             "motor",        motor,
	             "motorControl", motorControl);

	if constexpr (Archive::IS_LOADER) {
		auto time = getCurrentTime();
		if (playImage && (tapePos > playImage->getEndTime())) {
			tapePos = playImage->getEndTime();
			motherBoard.getMSXCliComm().printWarning("Tape position "
				"beyond tape end! Setting tape position to end. "
				"This can happen if you load a replay from an "
				"older openMSX version with a different CAS-to-WAV "
				"baud rate or when the tape image has been changed "
				"compared to when the replay was created.");
		}
		if (state == State::RECORD) {
			// TODO we don't support savestates in RECORD mode yet
			motherBoard.getMSXCliComm().printWarning(
				"Restoring a state where the MSX was saving to "
				"tape is not yet supported. Emulation will "
				"continue without actually saving.");
			setState(State::STOP, getImageName(), time);
		}
		if (!playImage && (state == State::PLAY)) {
			// This should only happen for manually edited
			// savestates, though we shouldn't crash on it.
			setState(State::STOP, getImageName(), time);
		}
		sync(time);
		updateLoadingState(time);
	}
}
INSTANTIATE_SERIALIZE_METHODS(CassettePlayer);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, CassettePlayer, "CassettePlayer");

} // namespace openmsx
