// $Id$

#include "CassettePlayer.hh"
#include "Connector.hh"
#include "CassettePort.hh"
#include "CommandController.hh"
#include "GlobalSettings.hh"
#include "XMLElement.hh"
#include "FileContext.hh"
#include "WavImage.hh"
#include "CasImage.hh"
#include "DummyCassetteImage.hh"
#include "CliComm.hh"
#include "CommandException.hh"
#include "MSXMotherBoard.hh"
#include "Scheduler.hh"
#include "FileOperations.hh"
#include "WavWriter.hh"
#include <algorithm>
#include <cstdlib>

using std::auto_ptr;
using std::list;
using std::string;
using std::vector;
using std::set;

namespace openmsx {

static const unsigned RECORD_FREQ = 44100;
static const double OUTPUT_AMP = 60.0;


MSXCassettePlayerCLI::MSXCassettePlayerCLI(CommandLineParser& commandLineParser_)
	: commandLineParser(commandLineParser_)
{
	commandLineParser.registerOption("-cassetteplayer", this);
	commandLineParser.registerFileClass("cassetteimage", this);
}

bool MSXCassettePlayerCLI::parseOption(const string& option,
                                       list<string>& cmdLine)
{
	parseFileType(getArgument(option, cmdLine), cmdLine);
	return true;
}
const string& MSXCassettePlayerCLI::optionHelp() const
{
	static const string text(
	  "Put cassette image specified in argument in virtual cassetteplayer");
	return text;
}

void MSXCassettePlayerCLI::parseFileType(const string& filename,
                                         list<string>& /*cmdLine*/)
{
	XMLElement& config = commandLineParser.getMotherBoard().
	            getCommandController().getGlobalSettings().getMediaConfig();
	XMLElement& playerElem = config.getCreateChild("cassetteplayer");
	playerElem.setData(filename);
	playerElem.setFileContext(auto_ptr<FileContext>(
		new UserFileContext(commandLineParser.getMotherBoard().
		                                     getCommandController())));
}
const string& MSXCassettePlayerCLI::fileTypeHelp() const
{
	static const string text(
		"Cassette image, raw recording or fMSX CAS image");
	return text;
}


CassettePlayer::CassettePlayer(
		CommandController& commandController, Mixer& mixer)
	: SoundDevice(mixer, getName(), getDescription())
	, SimpleCommand(commandController, "cassetteplayer")
	, motor(false), forcePlay(false)
	, lastOutput(false)
	, sampcnt(0)
	, cliComm(commandController.getCliComm())
{
	XMLElement& config = commandController.getGlobalSettings().getMediaConfig();
	playerElem = &config.getCreateChild("cassetteplayer");
	const string filename = playerElem->getData();
	if (!filename.empty()) {
		try {
			insertTape(playerElem->getFileContext().resolve(filename),
			           EmuTime::zero);
		} catch (MSXException& e) {
			throw FatalError("Couldn't load tape image: " + filename);
		}
	} else {
		removeTape(EmuTime::zero);
	}

	static XMLElement cassettePlayerConfig("cassetteplayer");
	static bool init = false;
	if (!init) {
		init = true;
		auto_ptr<XMLElement> sound(new XMLElement("sound"));
		sound->addChild(auto_ptr<XMLElement>(new XMLElement("volume", "5000")));
		cassettePlayerConfig.addChild(sound);
	}
	registerSound(cassettePlayerConfig);
}

CassettePlayer::~CassettePlayer()
{
	unregisterSound();
	if (Connector* connector = getConnector()) {
		connector->unplug(
			getCommandController().getScheduler().getCurrentTime());
	}
}

void CassettePlayer::insertTape(const string& filename, const EmuTime& time)
{
	try {
		// first try WAV
		cassette.reset(new WavImage(filename));
	} catch (MSXException &e) {
		// if that fails use CAS
		cassette.reset(new CasImage(filename));
	}
	rewind(time);
	setMute(!isPlaying());
	playerElem->setData(filename);
}


void CassettePlayer::startRecording(const std::string& filename,
                                    const EmuTime& time)
{
	wavWriter.reset(new WavWriter(filename, 1, 8, RECORD_FREQ));
	reinitRecording(time);
}

void CassettePlayer::reinitRecording(const EmuTime& time)
{
	setMute(true);
	recTime = time;
	partialOut = 0.0;
	partialInterval = 0.0;
	lastX = lastOutput ? OUTPUT_AMP : -OUTPUT_AMP;
	lastY = 0.0;
}

void CassettePlayer::stopRecording(const EmuTime& time)
{
	if (wavWriter.get()) {
		setSignal(lastOutput, time);
		if (sampcnt) {
			flushOutput();
		}
		wavWriter.reset();
	}
}

void CassettePlayer::removeTape(const EmuTime& time)
{
	stopRecording(time);
	setMute(true);
	cassette.reset(new DummyCassetteImage());
	playerElem->setData("");
}

void CassettePlayer::rewind(const EmuTime& time)
{
	stopRecording(time);
	tapeTime = EmuTime::zero;
	playTapeTime = EmuTime::zero;
}

bool CassettePlayer::isPlaying() const
{
	return motor || forcePlay;
}

void CassettePlayer::updatePosition(const EmuTime& time)
{
	assert(!wavWriter.get());
	if (isPlaying()) {
		tapeTime += (time - prevTime);
		playTapeTime = tapeTime;
	}
	prevTime = time;
}

void CassettePlayer::fillBuf(size_t length, double x)
{
	assert(wavWriter.get());
	static const double A = 252.0 / 256.0;

	double y = lastY + (x - lastX);

	while (length) {
		int len = std::min(length, BUF_SIZE - sampcnt);
		for (int j = 0; j < len; ++j) {
			buf[sampcnt++] = (int)y + 128;
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
	wavWriter->write8mono(buf, sampcnt);
	sampcnt = 0;
	wavWriter->flush(); // update wav header
}

void CassettePlayer::updateAll(const EmuTime& time)
{
	if (wavWriter.get()) {
		// recording
		if (isPlaying()) {
			// was already recording, update output
			setSignal(lastOutput, time);
		} else {
			// (possibly) restart recording, reset parameters
			reinitRecording(time);
		}
		flushOutput();
	} else {
		// playing
		updatePosition(time);
	}
}

void CassettePlayer::setMotor(bool status, const EmuTime& time)
{
	updateAll(time);
	motor = status;
	setMute(wavWriter.get() || !isPlaying());
}

void CassettePlayer::setForce(bool status, const EmuTime& time)
{
	updateAll(time);
	forcePlay = status;
	setMute(wavWriter.get() || !isPlaying());
}

short CassettePlayer::getSample(const EmuTime& time)
{
	assert(!wavWriter.get());
	return isPlaying() ? cassette->getSampleAt(time) : 0;
}

short CassettePlayer::readSample(const EmuTime& time)
{
	if (!wavWriter.get()) {
		// playing
		updatePosition(time);
		return getSample(tapeTime);
	} else {
		// recording
		return 0;
	}
}

void CassettePlayer::setSignal(bool output, const EmuTime& time)
{
	if (wavWriter.get() && isPlaying()) {
		double out = output ? OUTPUT_AMP : -OUTPUT_AMP;
		double samples = (time - recTime).toDouble() * RECORD_FREQ;
		double rest = 1.0 - partialInterval;
		if (rest <= samples) {
			// enough to fill next interval
			partialOut += out * rest;
			fillBuf(1, (int)partialOut);
			samples -= rest;

			// fill complete intervals
			int count = (int)samples;
			if (count > 0) {
				fillBuf(count, (int)out);
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
	recTime = time;
	lastOutput = output;
}

const string& CassettePlayer::getName() const
{
	static const string name("cassetteplayer");
	return name;
}

const string& CassettePlayer::getDescription() const
{
	static const string desc(
		"Cassetteplayer, use to read .cas or .wav files or "
		"write .wav files.");
	return desc;
}

void CassettePlayer::plugHelper(Connector* connector, const EmuTime& time)
{
	lastOutput = ((CassettePortInterface*)connector)->lastOut();
	updateAll(time);
}

void CassettePlayer::unplugHelper(const EmuTime& time)
{
	stopRecording(time);
}


string CassettePlayer::execute(const vector<string>& tokens)
{
	string result;
	EmuTime now = getCommandController().getScheduler().getCurrentTime();
	if (tokens.size() < 2) {
		throw SyntaxError();
	}
	if (tokens[1] == "-mode") {
		switch (tokens.size()) {
		case 2:
			result += "Mode is: ";
			result += wavWriter.get() ? "record" : "play";
			break;
		case 3:
			if (tokens[2] == "record") {
				// TODO: add filename parameter and maybe also
				//       -prefix option
				if (!wavWriter.get()) {
					string filename = FileOperations::
					  getNextNumberedFileName(
					    "taperecordings", "openmsx", ".wav");
					startRecording(filename, now);
					result += "Record mode set, writing to "
					          "file: " + filename;
				} else {
					result += "Already in recording mode.";
				}
			} else if (tokens[2] == "play") {
				if (wavWriter.get()) {
					stopRecording(now);
					result += "Play mode set";
				} else {
					result += "Already in play mode.";
				}
			} else {
				throw SyntaxError();
			}
			break;
		default:
			throw SyntaxError();
		}

	} else if (tokens.size() != 2) {
		throw SyntaxError();

	} else if (tokens[1] == "-eject") {
		result += "Tape ejected";
		removeTape(now);
		cliComm.update(CliComm::MEDIA, "cassetteplayer", "");
	} else if (tokens[1] == "eject") {
		result += "Tape ejected\n"
		          "Warning: use of 'eject' is deprecated, "
		          "instead use '-eject'";
		removeTape(now);
		cliComm.update(CliComm::MEDIA, "cassetteplayer", "");

	} else if (tokens[1] == "-rewind") {
		result += "Tape rewinded";
		rewind(now);
	} else if (tokens[1] == "rewind") {
		result += "Tape rewinded\n"
		          "Warning: use of 'rewind' is deprecated, "
		          "instead use '-rewind'";
		rewind(now);

	} else if (tokens[1] == "-force_play") {
		setForce(true, now);
	} else if (tokens[1] == "force_play") {
		setForce(true, now);
		result += "Warning: use of 'force_play' is deprecated, "
		          "instead use '-force_play'\n";
	} else if (tokens[1] == "-no_force_play") {
		setForce(false, now);
	} else if (tokens[1] == "no_force_play") {
		setForce(false, now);
		result += "Warning: use of 'no_force_play' is deprecated, "
		          "instead use '-no_force_play'";

	} else {
		try {
			stopRecording(now);
			result += "Changing tape";
			UserFileContext context(getCommandController());
			insertTape(context.resolve(tokens[1]), now);
			cliComm.update(CliComm::MEDIA,
			               "cassetteplayer", tokens[1]);
		} catch (MSXException &e) {
			throw CommandException(e.getMessage());
		}
	}
	return result;
}

string CassettePlayer::help(const vector<string>& /*tokens*/) const
{
	return "cassetteplayer -eject         : remove tape from virtual player\n"
	       "cassetteplayer -rewind        : rewind tape in virtual player\n"
	       "cassetteplayer -force_play    : force playing of tape (no remote)\n"
	       "cassetteplayer -no_force_play : don't force playing of tape\n"
	       "cassetteplayer -mode          : change mode (record or play)\n"
	       "cassetteplayer <filename>     : change the tape file\n";
}

void CassettePlayer::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		set<string> extra;
		extra.insert("-eject");
		extra.insert("-rewind");
		extra.insert("-force_play");
		extra.insert("-no_force_play");
		extra.insert("-mode");
		UserFileContext context(getCommandController());
		completeFileName(tokens, context, extra);
	} else if ((tokens.size() == 3) && (tokens[1] == "-mode")) {
		set<string> options;
		options.insert("record");
		options.insert("play");
		completeString(tokens, options);
	}
}

void CassettePlayer::setVolume(int newVolume)
{
	volume = newVolume;
}

void CassettePlayer::setSampleRate(int sampleRate)
{
	delta = EmuDuration(1.0 / sampleRate);
}

void CassettePlayer::updateBuffer(unsigned length, int* buffer,
     const EmuTime& /*time*/, const EmuDuration& /*sampDur*/)
{
	if (!wavWriter.get()) {
		while (length--) {
			*(buffer++) = (((int)getSample(playTapeTime)) * volume) >> 15;
			playTapeTime += delta;
		}
	} else { // not reachable, mute is set
		assert(false);
		//while (length--) {
		//	*(buffer++) = 0; // TODO
		//playTapeTime += delta;
	}
}

} // namespace openmsx
