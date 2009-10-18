// $Id$

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
//   default.  When specifiying record mode at insert (somehow), it should be
//   at the back.
//   Alternatively, we could remember the index in tape images by storing the
//   index in some persistent data file with its SHA1 sum as it was as we last
//   saw it. When there are write actions to the tape, the hash has to be
//   recalculated and replaced in the data file. An optimization would be to
//   first simply check on the length of the file and fall back to SHA1 if that
//   results in multiple matches.

#include "CassettePlayer.hh"
#include "BooleanSetting.hh"
#include "Connector.hh"
#include "CassettePort.hh"
#include "CommandController.hh"
#include "RecordedCommand.hh"
#include "XMLElement.hh"
#include "FileContext.hh"
#include "WavImage.hh"
#include "CasImage.hh"
#include "CliComm.hh"
#include "CommandException.hh"
#include "EventDistributor.hh"
#include "FileOperations.hh"
#include "WavWriter.hh"
#include "ThrottleManager.hh"
#include "TclObject.hh"
#include "DynamicClock.hh"
#include "Clock.hh"
#include "StringOp.hh"
#include "unreachable.hh"
#include <algorithm>
#include <cassert>

using std::auto_ptr;
using std::string;
using std::vector;
using std::set;

namespace openmsx {

static const unsigned RECORD_FREQ = 44100;
static const double OUTPUT_AMP = 60.0;

enum SyncType {
	END_OF_TAPE,
	SYNC_AUDIO_EMU
};

class TapeCommand : public RecordedCommand
{
public:
	TapeCommand(CommandController& commandController,
	            MSXEventDistributor& msxEventDistributor,
	            Scheduler& scheduler,
	            CassettePlayer& cassettePlayer);
	virtual string execute(const vector<string>& tokens, EmuTime::param time);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	CassettePlayer& cassettePlayer;
};


CassettePlayer::CassettePlayer(
		CommandController& commandController_,
		MSXMixer& mixer, Scheduler& scheduler,
		MSXEventDistributor& msxEventDistributor,
		EventDistributor& eventDistributor_,
		CliComm& cliComm_,
		EnumSetting<ResampleType>& resampleSetting,
		ThrottleManager& throttleManager)
	: SoundDevice(mixer, getName(), getDescription(), 1)
	, Resample(resampleSetting)
	, Schedulable(scheduler)
	, tapePos(EmuTime::zero)
	, prevSyncTime(EmuTime::zero)
	, audioPos(0)
	, commandController(commandController_)
	, cliComm(cliComm_)
	, eventDistributor(eventDistributor_)
	, tapeCommand(new TapeCommand(commandController, msxEventDistributor,
	                              scheduler, *this))
	, loadingIndicator(new LoadingIndicator(throttleManager))
	, autoRunSetting(new BooleanSetting(commandController,
		"autoruncassettes", "automatically try to run cassettes", false))
	, sampcnt(0)
	, state(STOP)
	, lastOutput(false)
	, motor(false), motorControl(true)
	, syncScheduled(false)
{
	removeTape(EmuTime::zero);

	static XMLElement cassettePlayerConfig("cassetteplayer");
	static bool init = false;
	if (!init) {
		init = true;
		auto_ptr<XMLElement> sound(new XMLElement("sound"));
		sound->addChild(auto_ptr<XMLElement>(new XMLElement("volume", "5000")));
		cassettePlayerConfig.addChild(sound);
	}
	registerSound(cassettePlayerConfig);
	eventDistributor.registerEventListener(OPENMSX_BOOT_EVENT, *this);
	cliComm.update(CliComm::HARDWARE, getName(), "add");
}

CassettePlayer::~CassettePlayer()
{
	unregisterSound();
	if (Connector* connector = getConnector()) {
		connector->unplug(getCurrentTime());
	}
	eventDistributor.unregisterEventListener(OPENMSX_BOOT_EVENT, *this);
	cliComm.update(CliComm::HARDWARE, getName(), "remove");
}

void CassettePlayer::autoRun()
{
	if (!playImage.get()) return;

	// try to automatically run the tape, if that's set
	CassetteImage::FileType type = playImage->getFirstFileType();
	if (!autoRunSetting->getValue() || type == CassetteImage::UNKNOWN) {
		return;
	}
	string loadingInstruction;
	switch (type) {
		case CassetteImage::ASCII:
			loadingInstruction = "RUN\\\"CAS:\\\"";
			break;
		case CassetteImage::BINARY:
			loadingInstruction = "BLOAD\\\"CAS:\\\",R";
			break;
		case CassetteImage::BASIC:
			loadingInstruction = "CLOAD\\\\rRUN";
			break;
		default:
			UNREACHABLE; // Shouldn't be possible
	}
	string var = "::auto_run_cas_counter";
	string command =
		"if ![info exists " + var + "] { set " + var + " 0 }\n"
		"incr " + var + "\n"
		"after time 2 \"if $" + var + "==\\$" + var + " { "
		"type " + loadingInstruction + "\\\\r }\"";
	try {
		commandController.executeCommand(command);
	} catch (CommandException& e) {
		cliComm.printWarning(
			"Error executing loading instruction for AutoRun: " +
			e.getMessage() + "\n Please report a bug.");
	}
}

CassettePlayer::State CassettePlayer::getState() const
{
	return state;
}

string CassettePlayer::getStateString() const
{
	switch (getState()) {
		case PLAY:   return "play";
		case RECORD: return "record";
		case STOP:   return "stop";
	}
	UNREACHABLE; return "";
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
	return (tapePos - EmuTime::zero).toDouble();
}

double CassettePlayer::getTapeLength(EmuTime::param time)
{
	if (playImage.get()) {
		return (playImage->getEndTime() - EmuTime::zero).toDouble();
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
		assert(!recordImage.get());
		if (playImage.get()) {
			// we're at end-of tape
			assert(!getImageName().empty());
		} else {
			// no tape inserted, imageName may or may not be empty
		}
		break;
	case PLAY:
		assert(!getImageName().empty());
		assert(!recordImage.get());
		assert(playImage.get());
		break;
	case RECORD:
		assert(!getImageName().empty());
		assert(recordImage.get());
		assert(!playImage.get());
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
	if (oldState == RECORD) {
		flushOutput();
		bool empty = recordImage.get()->isEmpty();
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
	cliComm.update(CliComm::STATUS, "cassetteplayer", getStateString());

	updateLoadingState(time); // sets SP for tape-end detection

	checkInvariants();
}

void CassettePlayer::updateLoadingState(EmuTime::param time)
{
	// TODO also set loadingIndicator for RECORD?
	// note: we don't use isRolling()
	loadingIndicator->update(motor && (getState() == PLAY));

	removeSyncPoint(END_OF_TAPE);
	if (isRolling() && (getState() == PLAY)) {
		setSyncPoint(time + (playImage->getEndTime() - tapePos), END_OF_TAPE);
	}
}

void CassettePlayer::setImageName(const Filename& newImage)
{
	casImage = newImage;
	cliComm.update(CliComm::MEDIA, "cassetteplayer", casImage.getResolved());
}

const Filename& CassettePlayer::getImageName() const
{
	return casImage;
}

void CassettePlayer::insertTape(const Filename& filename)
{
	if (!filename.empty()) {
		try {
			// first try WAV
			playImage.reset(new WavImage(filename));
		} catch (MSXException& e) {
			try {
				// if that fails use CAS
				playImage.reset(new CasImage(filename, cliComm));
			} catch (MSXException& e2) {
				throw MSXException(
					"Failed to insert WAV image: \"" +
					e.getMessage() +
					"\" and also failed to insert CAS image: \"" +
					e2.getMessage() + "\"");
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
	setImageName(filename);
}

void CassettePlayer::playTape(const Filename& filename, EmuTime::param time)
{
	if (getState() == RECORD) {
		// First close the recorded image. Otherwise it goes wrong
		// if you switch from RECORD->PLAY on the same image.
		setState(STOP, getImageName(), time); // keep current image
	}
	insertTape(filename);
	rewind(time); // sets PLAY mode
	autoRun();
	setOutputRate(outputRate); // recalculate resample stuff
}

void CassettePlayer::rewind(EmuTime::param time)
{
	assert(getState() != RECORD);
	tapePos = EmuTime::zero;
	audioPos = 0;

	if (getImageName().empty()) {
		// no image inserted, do nothing
		assert(getState() == STOP);
	} else {
		// keep current image
		setState(PLAY, getImageName(), time);
	}
}

void CassettePlayer::recordTape(const Filename& filename, EmuTime::param time)
{
	removeTape(time); // flush (possible) previous recording
	recordImage.reset(new Wav8Writer(filename, 1, RECORD_FREQ));
	tapePos = EmuTime::zero;
	setState(RECORD, filename, time);
}

void CassettePlayer::removeTape(EmuTime::param time)
{
	playImage.reset();
	tapePos = EmuTime::zero;
	setState(STOP, Filename(), time);
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

short CassettePlayer::readSample(EmuTime::param time)
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
	if (!isRolling()) return;

	tapePos += duration;

	// synchronize audio with actual tape position
	if ((getState() == PLAY) && !syncScheduled) {
		// don't sync too often, this improves sound quality
		syncScheduled = true;
		Clock<1> next(time);
		next += 1;
		setSyncPoint(next.getTime(), SYNC_AUDIO_EMU);
	}
}

void CassettePlayer::generateRecordOutput(EmuDuration::param duration)
{
	if (!recordImage.get() || !isRolling()) return;

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
	assert(recordImage.get());
	static const double A = 252.0 / 256.0;

	double y = lastY + (x - lastX);

	while (length) {
		size_t len = std::min(length, BUF_SIZE - sampcnt);
		for (size_t j = 0; j < len; ++j) {
			buf[sampcnt++] = int(y) + 128;
			y *= A;
		}
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
		cliComm.printWarning(
			"Failed to write to tape: " + e.getMessage());
	}
}


const string& CassettePlayer::getName() const
{
	static const string name("cassetteplayer");
	return name;
}

const string& CassettePlayer::getDescription() const
{
	// TODO: this description is not entirely accurate, but it is used
	// as an identifier for this device in e.g. Catapult. We should use
	// another way to identify devices A.S.A.P.!
	static const string desc(
		"Cassetteplayer, use to read .cas or .wav files.");
	return desc;
}

void CassettePlayer::plugHelper(Connector& connector, EmuTime::param time)
{
	sync(time);
	lastOutput = static_cast<CassettePort&>(connector).lastOut();
}

void CassettePlayer::unplugHelper(EmuTime::param time)
{
	// note: may not throw exceptions
	setState(STOP, getImageName(), time); // keep current image
}


void CassettePlayer::setOutputRate(unsigned newOutputRate)
{
	outputRate = newOutputRate;
	unsigned inputRate = playImage.get() ? playImage->getFrequency()
	                                     : outputRate;
	setInputRate(inputRate);
	setResampleRatio(inputRate, outputRate, isStereo());
}

void CassettePlayer::generateChannels(int** buffers, unsigned num)
{
	if ((getState() != PLAY) || !isRolling()) {
		buffers[0] = 0;
		return;
	}
	// Note: fillBuffer() replaces the values in the buffer. It should add
	//       to the existing values in the buffer. But because there is only
	//       one channel this doesn't matter (buffer contains all zeros).
	playImage->fillBuffer(audioPos, buffers, num);
	audioPos += num;
}

bool CassettePlayer::generateInput(int* buffer, unsigned num)
{
	return mixChannels(buffer, num);
}

bool CassettePlayer::updateBuffer(unsigned length, int* buffer,
     EmuTime::param /*time*/, EmuDuration::param /*sampDur*/)
{
	return generateOutput(buffer, length);
}


bool CassettePlayer::signalEvent(shared_ptr<const Event> event)
{
	if (event->getType() == OPENMSX_BOOT_EVENT) {
		if (!getImageName().empty()) {
			// Reinsert tape to make sure everything is reset.
			try {
				playTape(getImageName(), getCurrentTime());
			} catch (MSXException& e) {
				cliComm.printWarning(
					"Failed to insert tape: " + e.getMessage());
			}
		}
	}
	return true;
}

const std::string& CassettePlayer::schedName() const
{
	static const string schedName = "CassettePlayer";
	return schedName;
}

void CassettePlayer::executeUntil(EmuTime::param time, int userData)
{
	switch (userData) {
	case END_OF_TAPE:
		// tape ended
		cliComm.printWarning(
			"Tape end reached... stopping. "
			"You may need to insert another tape image "
			"that contains side B. (Or you used the wrong "
			"loading command.)");
		setState(STOP, getImageName(), time); // keep current image
		break;
	case SYNC_AUDIO_EMU:
		if (getState() == PLAY) {
			updateStream(time);
			sync(time);
			DynamicClock clk(EmuTime::zero);
			clk.setFreq(playImage->getFrequency());
			audioPos = clk.getTicksTill(tapePos);
		}
		syncScheduled = false;
		break;
	}
}


// class TapeCommand

TapeCommand::TapeCommand(CommandController& commandController,
                         MSXEventDistributor& msxEventDistributor,
                         Scheduler& scheduler,
                         CassettePlayer& cassettePlayer_)
	: RecordedCommand(commandController, msxEventDistributor,
	                  scheduler, "cassetteplayer")
	, cassettePlayer(cassettePlayer_)
{
}

string TapeCommand::execute(const vector<string>& tokens, EmuTime::param time)
{
	StringOp::Builder result;
	if (tokens.size() == 1) {
		// Returning Tcl lists here, similar to the disk commands in
		// DiskChanger
		TclObject tmp(getCommandController().getInterpreter());
		tmp.addListElement(getName() + ':');
		tmp.addListElement(cassettePlayer.getImageName().getResolved());

		TclObject options(getCommandController().getInterpreter());
		options.addListElement(cassettePlayer.getStateString());
		tmp.addListElement(options);
		result << tmp.getString();

	} else if (tokens[1] == "new") {
		string filename = (tokens.size() == 3)
		                ? tokens[2]
		                : FileOperations::getNextNumberedFileName(
		                         "taperecordings", "openmsx", ".wav");
		cassettePlayer.recordTape(Filename(filename), time);
		result << "Created new cassette image file: " << filename
		       << ", inserted it and set recording mode.";

	} else if (tokens[1] == "insert" && tokens.size() == 3) {
		try {
			result << "Changing tape";
			Filename filename(tokens[2], getCommandController());
			cassettePlayer.playTape(filename, time);
		} catch (MSXException& e) {
			throw CommandException(e.getMessage());
		}

	} else if (tokens[1] == "motorcontrol" && tokens.size() == 3) {
		if (tokens[2] == "on") {
			cassettePlayer.setMotorControl(true, time);
			result << "Motor control enabled.";
		} else if (tokens[2] == "off") {
			cassettePlayer.setMotorControl(false, time);
			result << "Motor control disabled.";
		} else {
			throw SyntaxError();
		}

	} else if (tokens.size() != 2) {
		throw SyntaxError();

	} else if (tokens[1] == "motorcontrol") {
		result << "Motor control is "
		       << (cassettePlayer.motorControl ? "on" : "off");

	} else if (tokens[1] == "record") {
			result << "TODO: implement this... (sorry)";

	} else if (tokens[1] == "play") {
		if (cassettePlayer.getState() == CassettePlayer::RECORD) {
			try {
				result << "Play mode set, rewinding tape.";
				cassettePlayer.playTape(
					cassettePlayer.getImageName(), time);
			} catch (MSXException& e) {
				throw CommandException(e.getMessage());
			}
		} else if (cassettePlayer.getState() == CassettePlayer::STOP) {
			throw CommandException("No tape inserted or tape at end!");
		} else {
			// PLAY mode
			result << "Already in play mode.";
		}

	} else if (tokens[1] == "eject") {
		result << "Tape ejected";
		cassettePlayer.removeTape(time);

	} else if (tokens[1] == "rewind") {
		if (cassettePlayer.getState() == CassettePlayer::RECORD) {
			try {
				result << "First stopping recording... ";
				cassettePlayer.playTape(
					cassettePlayer.getImageName(), time);
			} catch (MSXException& e) {
				throw CommandException(e.getMessage());
			}
		}
		cassettePlayer.rewind(time);
		result << "Tape rewound";

	} else if (tokens[1] == "getpos") {
		result << cassettePlayer.getTapePos(time);

	} else if (tokens[1] == "getlength") {
		result << cassettePlayer.getTapeLength(time);

	} else {
		try {
			result << "Changing tape";
			Filename filename(tokens[1], getCommandController());
			cassettePlayer.playTape(filename, time);
		} catch (MSXException& e) {
			throw CommandException(e.getMessage());
		}
	}
	//if (!cassettePlayer.getConnector()) {
	//	cassettePlayer.cliComm.printWarning("Cassetteplayer not plugged in.");
	//}
	return result;
}

string TapeCommand::help(const vector<string>& tokens) const
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

void TapeCommand::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		set<string> extra;
		extra.insert("eject");
		extra.insert("rewind");
		extra.insert("motorcontrol");
		extra.insert("insert");
		extra.insert("new");
		extra.insert("play");
	//	extra.insert("record");
		extra.insert("getpos");
		extra.insert("getlength");
		UserFileContext context;
		completeFileName(getCommandController(), tokens, context, extra);
	} else if ((tokens.size() == 3) && (tokens[1] == "insert")) {
		UserFileContext context;
		completeFileName(getCommandController(), tokens, context);
	} else if ((tokens.size() == 3) && (tokens[1] == "motorcontrol")) {
		set<string> extra;
		extra.insert("on");
		extra.insert("off");
		completeString(tokens, extra);
	}
}


static enum_string<CassettePlayer::State> stateInfo[] = {
	{ "PLAY",   CassettePlayer::PLAY   },
	{ "RECORD", CassettePlayer::RECORD },
	{ "STOP",   CassettePlayer::STOP   }
};
SERIALIZE_ENUM(CassettePlayer::State, stateInfo);

template<typename Archive>
void CassettePlayer::serialize(Archive& ar, unsigned /*version*/)
{
	if (recordImage.get()) {
		// buf, sampcnt
		flushOutput();
	}
	if (state == RECORD) {
		// TODO we don't support savestates in RECORD mode yet
		setState(STOP, getImageName(), getCurrentTime());
	}

	ar.serialize("casImage", casImage);
	if (ar.isLoader()) {
		removeTape(getCurrentTime());
		casImage.updateAfterLoadState(commandController);
		insertTape(casImage);
	}

	// only for RECORD
	//double lastX;
	//double lastY;
	//double partialOut;
	//double partialInterval;
	//std::auto_ptr<WavWriter> recordImage;

	ar.serialize("tapePos", tapePos);
	ar.serialize("prevSyncTime", prevSyncTime);
	ar.serialize("audioPos", audioPos);
	ar.serialize("state", state);
	ar.serialize("lastOutput", lastOutput);
	ar.serialize("motor", motor);
	ar.serialize("motorControl", motorControl);

	if (ar.isLoader()) {
		updateLoadingState(getCurrentTime());
	}
}
INSTANTIATE_SERIALIZE_METHODS(CassettePlayer);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, CassettePlayer, "CassettePlayer");

} // namespace openmsx
