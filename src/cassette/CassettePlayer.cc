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
#include "CliComm.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "GlobalSettings.hh"
#include "CommandException.hh"
#include "EventDistributor.hh"
#include "FileOperations.hh"
#include "WavWriter.hh"
#include "TclObject.hh"
#include "DynamicClock.hh"
#include "EmuDuration.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <algorithm>
#include <cassert>
#include <memory>

using std::string;
using std::vector;

namespace openmsx {

// TODO: this description is not entirely accurate, but it is used
// as an identifier for this audio device in e.g. Catapult. We should
// use another way to identify audio devices A.S.A.P.!
constexpr static_string_view DESCRIPTION = "Cassetteplayer, use to read .cas or .wav files.";

constexpr unsigned DUMMY_INPUT_RATE = 44100; // actual rate depends on .cas/.wav file
constexpr unsigned RECORD_FREQ = 44100;
constexpr double OUTPUT_AMP = 60.0;

static XMLElement createXML()
{
	XMLElement xml("cassetteplayer");
	xml.addChild("sound").addChild("volume", "5000");
	return xml;
}

static std::string_view getCassettePlayerName()
{
	return "cassetteplayer";
}

CassettePlayer::CassettePlayer(const HardwareConfig& hwConf)
	: ResampledSoundDevice(hwConf.getMotherBoard(), getCassettePlayerName(), DESCRIPTION, 1, DUMMY_INPUT_RATE, false)
	, syncEndOfTape(hwConf.getMotherBoard().getScheduler())
	, syncAudioEmu (hwConf.getMotherBoard().getScheduler())
	, tapePos(EmuTime::zero())
	, prevSyncTime(EmuTime::zero())
	, audioPos(0)
	, motherBoard(hwConf.getMotherBoard())
	, tapeCommand(
		motherBoard.getCommandController(),
		motherBoard.getStateChangeDistributor(),
		motherBoard.getScheduler())
	, loadingIndicator(
		motherBoard.getReactor().getGlobalSettings().getThrottleManager())
	, autoRunSetting(
		motherBoard.getCommandController(),
		"autoruncassettes", "automatically try to run cassettes", true)
	, sampcnt(0)
	, state(STOP)
	, lastOutput(false)
	, motor(false), motorControl(true)
	, syncScheduled(false)
{
	static XMLElement xml = createXML();
	registerSound(DeviceConfig(hwConf, xml));

	motherBoard.getReactor().getEventDistributor().registerEventListener(
		OPENMSX_BOOT_EVENT, *this);
	motherBoard.getMSXCliComm().update(CliComm::HARDWARE, getCassettePlayerName(), "add");

	removeTape(EmuTime::zero());
}

CassettePlayer::~CassettePlayer()
{
	unregisterSound();
	if (auto* c = getConnector()) {
		c->unplug(getCurrentTime());
	}
	motherBoard.getReactor().getEventDistributor().unregisterEventListener(
		OPENMSX_BOOT_EVENT, *this);
	motherBoard.getMSXCliComm().update(CliComm::HARDWARE, getCassettePlayerName(), "remove");
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
	string instr1, instr2;
	switch (type) {
		case CassetteImage::ASCII:
			instr1 = R"(RUN\"CAS:\")";
			break;
		case CassetteImage::BINARY:
			instr1 = R"(BLOAD\"CAS:\",R)";
			break;
		case CassetteImage::BASIC:
			// Note that CLOAD:RUN won't work: BASIC ignores stuff
			// after the CLOAD command. That's why it's split in two.
			instr1 = "CLOAD";
			instr2 = "RUN";
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

		// Without the 0.1s delay here, the type command gets messed up
		// on MSX1 machines for some reason (starting to type too early?)
		"    after time 0.1 \"type [lindex $args 0]\\\\r\"\n"

		"    set next [lrange $args 1 end]\n"
		"    if {[llength $next] == 0} return\n"

		// H_READ isn't called after CLOAD, but H_MAIN is. However, it's
		// also called right after H_READ, so we wait a little before
		// setting up the breakpoint.
		"    set cmd1 \"openmsx::auto_run_cb $next\"\n"
		"    set cmd2 \"set openmsx::auto_run_bp \\[debug set_bp 0xFF0C 1 \\\"$cmd1\\\"\\]\"\n" // H_MAIN
		"    after time 0.2 $cmd2\n"
		"  }\n"

		"  if {[info exists auto_run_bp]} {debug remove_bp $auto_run_bp\n}\n"
		"  set auto_run_bp [debug set_bp 0xFF07 1 {\n" // H_READ
		"    openmsx::auto_run_cb ", instr1, ' ', instr2, "\n"
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
		case PLAY:   return "play";
		case RECORD: return "record";
		case STOP:   return "stop";
	}
	UNREACHABLE; return {};
}

bool CassettePlayer::isRolling() const
{
	// Is the tape 'rolling'?
	// is true when:
	//  not in stop mode (there is a tape inserted and not at end-of-tape)
	//  AND [ user forced playing (motorcontrol=off) OR motor enabled by
	//        software (motor=on) ]
	return (getState() != STOP) && (motor || !motorControl);
}

double CassettePlayer::getTapePos(EmuTime::param time)
{
	sync(time);
	return (tapePos - EmuTime::zero()).toDouble();
}

double CassettePlayer::getTapeLength(EmuTime::param time)
{
	if (playImage) {
		return (playImage->getEndTime() - EmuTime::zero()).toDouble();
	} else if (getState() == RECORD) {
		return getTapePos(time);
	} else {
		return 0.0;
	}
}

void CassettePlayer::checkInvariants() const
{
	switch (getState()) {
	case STOP:
		assert(!recordImage);
		if (playImage) {
			// we're at end-of tape
			assert(!getImageName().empty());
		} else {
			// no tape inserted, imageName may or may not be empty
		}
		break;
	case PLAY:
		assert(!getImageName().empty());
		assert(!recordImage);
		assert(playImage);
		break;
	case RECORD:
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
	assert(!((oldState == PLAY)   && (newState == RECORD)));
	assert(!((oldState == RECORD) && (newState == PLAY)));

	// stuff for leaving the old state
	//  'recordImage==nullptr' can happen in case of loadstate.
	if ((oldState == RECORD) && recordImage) {
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
	if (newState == RECORD) {
		partialOut = 0.0;
		partialInterval = 0.0;
		lastX = lastOutput ? OUTPUT_AMP : -OUTPUT_AMP;
		lastY = 0.0;
	}
	motherBoard.getMSXCliComm().update(
		CliComm::STATUS, "cassetteplayer", getStateString());

	updateLoadingState(time); // sets SP for tape-end detection

	checkInvariants();
}

void CassettePlayer::updateLoadingState(EmuTime::param time)
{
	assert(prevSyncTime == time); // sync() must be called
	// TODO also set loadingIndicator for RECORD?
	// note: we don't use isRolling()
	loadingIndicator.update(motor && (getState() == PLAY));

	syncEndOfTape.removeSyncPoint();
	if (isRolling() && (getState() == PLAY)) {
		syncEndOfTape.setSyncPoint(time + (playImage->getEndTime() - tapePos));
	}
}

void CassettePlayer::setImageName(const Filename& newImage)
{
	casImage = newImage;
	motherBoard.getMSXCliComm().update(
		CliComm::MEDIA, "cassetteplayer", casImage.getResolved());
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
	unsigned inputRate = playImage ? playImage->getFrequency() : 44100;
	if (inputRate != getInputRate()) {
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
	setState(STOP, getImageName(), time); // keep current image
	insertTape(filename, time);
	rewind(time); // sets PLAY mode
	autoRun();
}

void CassettePlayer::rewind(EmuTime::param time)
{
	sync(time); // before tapePos changes
	assert(getState() != RECORD);
	tapePos = EmuTime::zero();
	audioPos = 0;

	if (getImageName().empty()) {
		// no image inserted, do nothing
		assert(getState() == STOP);
	} else {
		// keep current image
		setState(PLAY, getImageName(), time);
	}
	updateLoadingState(time);
}

void CassettePlayer::recordTape(const Filename& filename, EmuTime::param time)
{
	removeTape(time); // flush (possible) previous recording
	recordImage = std::make_unique<Wav8Writer>(filename, 1, RECORD_FREQ);
	tapePos = EmuTime::zero();
	setState(RECORD, filename, time);
}

void CassettePlayer::removeTape(EmuTime::param time)
{
	// first stop with tape still inserted
	setState(STOP, getImageName(), time);
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
	if (getState() == PLAY) {
		// playing
		sync(time);
		return isRolling() ? playImage->getSampleAt(tapePos) : 0;
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
	if (!isRolling() || (getState() != PLAY)) return;

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
	double rest = 1.0 - partialInterval;
	if (rest <= samples) {
		// enough to fill next interval
		partialOut += out * rest;
		fillBuf(1, int(partialOut));
		samples -= rest;

		// fill complete intervals
		int count = int(samples);
		if (count > 0) {
			fillBuf(count, int(out));
		}
		samples -= count;

		// partial last interval
		partialOut = samples * out;
		partialInterval = 0.0;
	} else {
		partialOut += samples * out;
		partialInterval += samples;
	}
}

void CassettePlayer::fillBuf(size_t length, double x)
{
	assert(recordImage);
	constexpr double A = 252.0 / 256.0;

	double y = lastY + (x - lastX);

	while (length) {
		size_t len = std::min(length, BUF_SIZE - sampcnt);
		repeat(len, [&] {
			buf[sampcnt++] = int(y) + 128;
			y *= A;
		});
		length -= len;
		assert(sampcnt <= BUF_SIZE);
		if (BUF_SIZE == sampcnt) {
			flushOutput();
		}
	}
	lastY = y;
	lastX = x;
}

void CassettePlayer::flushOutput()
{
	try {
		recordImage->write(buf, 1, unsigned(sampcnt));
		sampcnt = 0;
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
	lastOutput = static_cast<CassettePort&>(conn).lastOut();
}

void CassettePlayer::unplugHelper(EmuTime::param time)
{
	// note: may not throw exceptions
	setState(STOP, getImageName(), time); // keep current image
}


void CassettePlayer::generateChannels(float** buffers, unsigned num)
{
	// Single channel device: replace content of buffers[0] (not add to it).
	if ((getState() != PLAY) || !isRolling()) {
		buffers[0] = nullptr;
		return;
	}
	playImage->fillBuffer(audioPos, buffers, num);
	audioPos += num;
}

float CassettePlayer::getAmplificationFactorImpl() const
{
	return playImage ? playImage->getAmplificationFactorImpl() : 1.0f;
}

int CassettePlayer::signalEvent(const std::shared_ptr<const Event>& event)
{
	if (event->getType() == OPENMSX_BOOT_EVENT) {
		if (!getImageName().empty()) {
			// Reinsert tape to make sure everything is reset.
			try {
				playTape(getImageName(), getCurrentTime());
			} catch (MSXException& e) {
				motherBoard.getMSXCliComm().printWarning(
					"Failed to insert tape: ", e.getMessage());
			}
		}
	}
	return 0;
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
	setState(STOP, getImageName(), time); // keep current image
}

void CassettePlayer::execSyncAudioEmu(EmuTime::param time)
{
	if (getState() == PLAY) {
		updateStream(time);
		sync(time);
		DynamicClock clk(EmuTime::zero());
		clk.setFreq(playImage->getFrequency());
		audioPos = clk.getTicksTill(tapePos);
	}
	syncScheduled = false;
}


// class TapeCommand

CassettePlayer::TapeCommand::TapeCommand(
		CommandController& commandController_,
		StateChangeDistributor& stateChangeDistributor_,
		Scheduler& scheduler_)
	: RecordedCommand(commandController_, stateChangeDistributor_,
	                  scheduler_, "cassetteplayer")
{
}

void CassettePlayer::TapeCommand::execute(
	span<const TclObject> tokens, TclObject& result, EmuTime::param time)
{
	auto& cassettePlayer = OUTER(CassettePlayer, tapeCommand);
	if (tokens.size() == 1) {
		// Returning Tcl lists here, similar to the disk commands in
		// DiskChanger
		TclObject options = makeTclList(cassettePlayer.getStateString());
		result.addListElement(tmpStrCat(getName(), ':'),
		                      cassettePlayer.getImageName().getResolved(),
		                      options);

	} else if (tokens[1] == "new") {
		std::string_view directory = "taperecordings";
		std::string_view prefix = "openmsx";
		std::string_view extension = ".wav";
		string filename = FileOperations::parseCommandFileArgument(
			(tokens.size() == 3) ? tokens[2].getString() : string{},
			directory, prefix, extension);
		cassettePlayer.recordTape(Filename(filename), time);
		result = tmpStrCat(
			"Created new cassette image file: ", filename,
			", inserted it and set recording mode.");

	} else if (tokens[1] == "insert" && tokens.size() == 3) {
		try {
			result = "Changing tape";
			Filename filename(tokens[2].getString(), userFileContext());
			cassettePlayer.playTape(filename, time);
		} catch (MSXException& e) {
			throw CommandException(std::move(e).getMessage());
		}

	} else if (tokens[1] == "motorcontrol" && tokens.size() == 3) {
		if (tokens[2] == "on") {
			cassettePlayer.setMotorControl(true, time);
			result = "Motor control enabled.";
		} else if (tokens[2] == "off") {
			cassettePlayer.setMotorControl(false, time);
			result = "Motor control disabled.";
		} else {
			throw SyntaxError();
		}

	} else if (tokens.size() != 2) {
		throw SyntaxError();

	} else if (tokens[1] == "motorcontrol") {
		result = tmpStrCat("Motor control is ",
		                (cassettePlayer.motorControl ? "on" : "off"));

	} else if (tokens[1] == "record") {
			result = "TODO: implement this... (sorry)";

	} else if (tokens[1] == "play") {
		if (cassettePlayer.getState() == CassettePlayer::RECORD) {
			try {
				result = "Play mode set, rewinding tape.";
				cassettePlayer.playTape(
					cassettePlayer.getImageName(), time);
			} catch (MSXException& e) {
				throw CommandException(std::move(e).getMessage());
			}
		} else if (cassettePlayer.getState() == CassettePlayer::STOP) {
			throw CommandException("No tape inserted or tape at end!");
		} else {
			// PLAY mode
			result = "Already in play mode.";
		}

	} else if (tokens[1] == "eject") {
		result = "Tape ejected";
		cassettePlayer.removeTape(time);

	} else if (tokens[1] == "rewind") {
		string r;
		if (cassettePlayer.getState() == CassettePlayer::RECORD) {
			try {
				r = "First stopping recording... ";
				cassettePlayer.playTape(
					cassettePlayer.getImageName(), time);
			} catch (MSXException& e) {
				throw CommandException(std::move(e).getMessage());
			}
		}
		cassettePlayer.rewind(time);
		r += "Tape rewound";
		result = r;

	} else if (tokens[1] == "getpos") {
		result = cassettePlayer.getTapePos(time);

	} else if (tokens[1] == "getlength") {
		result = cassettePlayer.getTapeLength(time);

	} else {
		try {
			result = "Changing tape";
			Filename filename(tokens[1].getString(), userFileContext());
			cassettePlayer.playTape(filename, time);
		} catch (MSXException& e) {
			throw CommandException(std::move(e).getMessage());
		}
	}
	//if (!cassettePlayer.getConnector()) {
	//	cassettePlayer.cliComm.printWarning("Cassetteplayer not plugged in.");
	//}
}

string CassettePlayer::TapeCommand::help(const vector<string>& tokens) const
{
	string helptext;
	if (tokens.size() >= 2) {
		if (tokens[1] == "eject") {
			helptext =
			    "Well, just eject the cassette from the cassette "
			    "player/recorder!";
		} else if (tokens[1] == "rewind") {
			helptext =
			    "Indeed, rewind the tape that is currently in the "
			    "cassette player/recorder...";
		} else if (tokens[1] == "motorcontrol") {
			helptext =
			    "Setting this to 'off' is equivalent to "
			    "disconnecting the black remote plug from the "
			    "cassette player: it makes the cassette player "
			    "run (if in play mode); the motor signal from the "
			    "MSX will be ignored. Normally this is set to "
			    "'on': the cassetteplayer obeys the motor control "
			    "signal from the MSX.";
		} else if (tokens[1] == "play") {
			helptext =
			    "Go to play mode. Only useful if you were in "
			    "record mode (which is currently the only other "
			    "mode available).";
		} else if (tokens[1] == "new") {
			helptext =
			    "Create a new cassette image. If the file name is "
			    "omitted, one will be generated in the default "
			    "directory for tape recordings. Implies going to "
			    "record mode (why else do you want a new cassette "
			    "image?).";
		} else if (tokens[1] == "insert") {
			helptext =
			    "Inserts the specified cassette image into the "
			    "cassette player, rewinds it and switches to play "
			    "mode.";
		} else if (tokens[1] == "record") {
			helptext =
			    "Go to record mode. NOT IMPLEMENTED YET. Will be "
			    "used to be able to resume recording to an "
			    "existing cassette image, previously inserted with "
			    "the insert command.";
		} else if (tokens[1] == "getpos") {
			helptext =
			    "Return the position of the tape, in seconds from "
			    "the beginning of the tape.";
		} else if (tokens[1] == "getlength") {
			helptext =
			    "Return the length of the tape in seconds.";
		}
	} else {
		helptext =
		    "cassetteplayer eject             "
		    ": remove tape from virtual player\n"
		    "cassetteplayer rewind            "
		    ": rewind tape in virtual player\n"
		    "cassetteplayer motorcontrol      "
		    ": enables or disables motor control (remote)\n"
		    "cassetteplayer play              "
		    ": change to play mode (default)\n"
		    "cassetteplayer record            "
		    ": change to record mode (NOT IMPLEMENTED YET)\n"
		    "cassetteplayer new [<filename>]  "
		    ": create and insert new tape image file and go to record mode\n"
		    "cassetteplayer insert <filename> "
		    ": insert (a different) tape file\n"
		    "cassetteplayer getpos            "
		    ": query the position of the tape\n"
		    "cassetteplayer getlength         "
		    ": query the total length of the tape\n"
		    "cassetteplayer <filename>        "
		    ": insert (a different) tape file\n";
	}
	return helptext;
}

void CassettePlayer::TapeCommand::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		static constexpr const char* const cmds[] = {
			"eject", "rewind", "motorcontrol", "insert", "new",
			"play", "getpos", "getlength",
			//"record",
		};
		completeFileName(tokens, userFileContext(), cmds);
	} else if ((tokens.size() == 3) && (tokens[1] == "insert")) {
		completeFileName(tokens, userFileContext());
	} else if ((tokens.size() == 3) && (tokens[1] == "motorcontrol")) {
		static constexpr const char* const extra[] = { "on", "off" };
		completeString(tokens, extra);
	}
}

bool CassettePlayer::TapeCommand::needRecord(span<const TclObject> tokens) const
{
	return tokens.size() > 1;
}


static constexpr std::initializer_list<enum_string<CassettePlayer::State>> stateInfo = {
	{ "PLAY",   CassettePlayer::PLAY   },
	{ "RECORD", CassettePlayer::RECORD },
	{ "STOP",   CassettePlayer::STOP   }
};
SERIALIZE_ENUM(CassettePlayer::State, stateInfo);

// version 1: initial version
// version 2: added checksum
template<typename Archive>
void CassettePlayer::serialize(Archive& ar, unsigned version)
{
	if (recordImage) {
		// buf, sampcnt
		flushOutput();
	}

	ar.serialize("casImage", casImage);

	Sha1Sum oldChecksum;
	if (!ar.isLoader() && playImage) {
		oldChecksum = playImage->getSha1Sum();
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

	if (ar.isLoader()) {
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

	if (ar.isLoader()) {
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
		if (state == RECORD) {
			// TODO we don't support savestates in RECORD mode yet
			motherBoard.getMSXCliComm().printWarning(
				"Restoring a state where the MSX was saving to "
				"tape is not yet supported. Emulation will "
				"continue without actually saving.");
			setState(STOP, getImageName(), time);
		}
		if (!playImage && (state == PLAY)) {
			// This should only happen for manually edited
			// savestates, though we shouldn't crash on it.
			setState(STOP, getImageName(), time);
		}
		sync(time);
		updateLoadingState(time);
	}
}
INSTANTIATE_SERIALIZE_METHODS(CassettePlayer);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, CassettePlayer, "CassettePlayer");

} // namespace openmsx
