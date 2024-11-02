#include "AviRecorder.hh"

#include "AviWriter.hh"
#include "Mixer.hh"
#include "WavWriter.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "FileContext.hh"
#include "CommandException.hh"
#include "Display.hh"
#include "PostProcessor.hh"
#include "MSXMixer.hh"
#include "Filename.hh"
#include "CliComm.hh"
#include "FileOperations.hh"
#include "TclArgParser.hh"
#include "TclObject.hh"

#include "Math.hh"
#include "enumerate.hh"
#include "narrow.hh"
#include "outer.hh"
#include "small_buffer.hh"

#include <array>
#include <cassert>
#include <memory>

namespace openmsx {

AviRecorder::AviRecorder(Reactor& reactor_)
	: reactor(reactor_)
	, recordCommand(reactor.getCommandController())
{
}

AviRecorder::~AviRecorder()
{
	assert(!aviWriter);
	assert(!wavWriter);
}

void AviRecorder::start(bool recordAudio, bool recordVideo, bool recordMono,
                        bool recordStereo, const Filename& filename)
{
	stop();
	MSXMotherBoard* motherBoard = reactor.getMotherBoard();
	if (!motherBoard) {
		throw CommandException("No active MSX machine.");
	}
	if (recordAudio) {
		mixer = &motherBoard->getMSXMixer();
		warnedStereo = false;
		if (recordStereo) {
			stereo = true;
		} else if (recordMono) {
			stereo = false;
			warnedStereo = true; // no warning if data is actually stereo
		} else {
			stereo = mixer->needStereoRecording();
		}
		sampleRate = mixer->getSampleRate();
		warnedSampleRate = false;
	}
	if (recordVideo) {
		// Set V99x8, V9990, Laserdisc, ... in record mode (when
		// present). Only the active one will actually send frames to
		// the video. This also works for Video9000.
		postProcessors.clear();
		for (auto* l : reactor.getDisplay().getAllLayers()) {
			if (auto* pp = dynamic_cast<PostProcessor*>(l)) {
				postProcessors.push_back(pp);
			}
		}
		if (postProcessors.empty()) {
			throw CommandException(
				"Current renderer doesn't support video recording.");
		}
		// any source is fine because they all have the same bpp
		warnedFps = false;
		duration = EmuDuration::infinity();
		prevTime = EmuTime::infinity();

		try {
			aviWriter = std::make_unique<AviWriter>(
				filename, frameWidth, frameHeight,
				(recordAudio && stereo) ? 2 : 1, sampleRate);
		} catch (MSXException& e) {
			throw CommandException("Can't start recording: ",
			                       e.getMessage());
		}
	} else {
		assert(recordAudio);
		wavWriter = std::make_unique<Wav16Writer>(
			filename, stereo ? 2 : 1, sampleRate);
	}
	// only set recorders when all errors are checked for
	for (auto* pp : postProcessors) {
		pp->setRecorder(this);
	}
	if (mixer) mixer->setRecorder(this);
}

void AviRecorder::stop()
{
	for (auto* pp : postProcessors) {
		pp->setRecorder(nullptr);
	}
	postProcessors.clear();
	if (mixer) {
		mixer->setRecorder(nullptr);
		mixer = nullptr;
	}
	sampleRate = 0;
	aviWriter.reset();
	wavWriter.reset();
}

static int16_t float2int16(float f)
{
	return Math::clipToInt16(lrintf(32768.0f * f));
}

void AviRecorder::addWave(std::span<const StereoFloat> data)
{
	if (data.empty()) return;

	assert(mixer);
	if (!warnedSampleRate && (mixer->getSampleRate() != sampleRate)) {
		warnedSampleRate = true;
		reactor.getCliComm().printWarning(
			"Detected audio sample frequency change during "
			"avi recording. Audio/video might get out of sync "
			"because of this.");
	}
	auto num = data.size();
	if (stereo) {
		if (wavWriter) {
			wavWriter->write(data);
		} else {
			small_buffer<int16_t, 2 * 4096> buf(uninitialized_tag{}, 2 * size_t(num));
			for (auto [i, s] : enumerate(data)) {
				buf[2 * i + 0] = float2int16(s.left);
				buf[2 * i + 1] = float2int16(s.right);
			}
			assert(aviWriter);
			append(audioBuf, std::span{buf});
		}
	} else {
		small_buffer<int16_t, 4096> buf(uninitialized_tag{}, num);
		size_t i = 0;
		for (/**/; !warnedStereo && i < num; ++i) {
			if (data[i].left != data[i].right) {
				reactor.getCliComm().printWarning(
				    "Detected stereo sound during mono recording. "
				    "Channels will be mixed down to mono. To "
				    "avoid this warning you can explicitly pass the "
				    "-mono or -stereo flag to the record command.");
				warnedStereo = true;
				break;
			}
			buf[i] = float2int16(data[i].left);
		}
		for (/**/; i < num; ++i) {
			buf[i] = float2int16((data[i].left + data[i].right) * 0.5f);
		}

		if (wavWriter) {
			wavWriter->write(buf);
		} else {
			assert(aviWriter);
			append(audioBuf, std::span{buf});
		}
	}
}

void AviRecorder::addImage(const FrameSource* frame, EmuTime::param time)
{
	assert(!wavWriter);
	if (duration != EmuDuration::infinity()) {
		if (!warnedFps && ((time - prevTime) != duration)) {
			warnedFps = true;
			reactor.getCliComm().printWarning(
				"Detected frame rate change (PAL/NTSC or frameskip) "
				"during avi recording. Audio/video might get out of "
				"sync because of this.");
		}
	} else if (prevTime != EmuTime::infinity()) {
		duration = time - prevTime;
		aviWriter->setFps(narrow_cast<float>(1.0 / duration.toDouble()));
	}
	prevTime = time;

	if (mixer) {
		mixer->updateStream(time);
	}
	aviWriter->addFrame(frame, audioBuf);
	audioBuf.clear();
}

// TODO: Can this be dropped?
unsigned AviRecorder::getFrameHeight() const {
	assert (frameHeight != 0); // someone uses the getter too early?
	return frameHeight;
}

void AviRecorder::processStart(Interpreter& interp, std::span<const TclObject> tokens, TclObject& result)
{
	std::string_view prefix = "openmsx";
	bool audioOnly    = false;
	bool videoOnly    = false;
	bool recordMono   = false;
	bool recordStereo = false;
	bool doubleSize   = false;
	bool tripleSize   = false;
	std::array info = {
		valueArg("-prefix", prefix),
		flagArg("-audioonly", audioOnly),
		flagArg("-videoonly", videoOnly),
		flagArg("-mono",      recordMono),
		flagArg("-stereo",    recordStereo),
		flagArg("-doublesize", doubleSize),
		flagArg("-triplesize", tripleSize),
	};
	auto arguments = parseTclArgs(interp, tokens.subspan(2), info);

	if (audioOnly && videoOnly) {
		throw CommandException("Can't have both -videoonly and -audioonly.");
	}
	if (recordStereo && recordMono) {
		throw CommandException("Can't have both -mono and -stereo.");
	}
	if (doubleSize && tripleSize) {
		throw CommandException("Can't have both -doublesize and -triplesize.");
	}
	if (videoOnly && (recordStereo || recordMono)) {
		throw CommandException("Can't have both -videoonly and -stereo or -mono.");
	}
	std::string_view filenameArg;
	switch (arguments.size()) {
	case 0:
		// nothing
		break;
	case 1:
		filenameArg = arguments[0].getString();
		break;
	default:
		throw SyntaxError();
	}

	frameWidth = 320;
	frameHeight = 240;
	if (doubleSize) {
		frameWidth  *= 2;
		frameHeight *= 2;
	} else if (tripleSize) {
		frameWidth  *= 3;
		frameHeight *= 3;
	}
	bool recordAudio = !videoOnly;
	bool recordVideo = !audioOnly;
	std::string_view directory = recordVideo ? VIDEO_DIR : AUDIO_DIR;
	std::string_view extension = recordVideo ? VIDEO_EXTENSION : AUDIO_EXTENSION;
	auto filename = FileOperations::parseCommandFileArgument(
		filenameArg, directory, prefix, extension);

	if (aviWriter || wavWriter) {
		result = "Already recording.";
	} else {
		start(recordAudio, recordVideo, recordMono, recordStereo,
				Filename(filename));
		result = tmpStrCat("Recording to ", filename);
	}
}

void AviRecorder::processStop(std::span<const TclObject> /*tokens*/)
{
	stop();
}

void AviRecorder::processToggle(Interpreter& interp, std::span<const TclObject> tokens, TclObject& result)
{
	if (aviWriter || wavWriter) {
		// drop extra tokens
		processStop(tokens.first<2>());
	} else {
		processStart(interp, tokens, result);
	}
}

bool AviRecorder::isRecording() const
{
	return aviWriter || wavWriter;
}

void AviRecorder::status(std::span<const TclObject> /*tokens*/, TclObject& result) const
{
	result.addDictKeyValue("status", isRecording() ? "recording" : "idle");
}

// class AviRecorder::Cmd

AviRecorder::Cmd::Cmd(CommandController& commandController_)
	: Command(commandController_, "record")
{
}

void AviRecorder::Cmd::execute(std::span<const TclObject> tokens, TclObject& result)
{
	if (tokens.size() < 2) {
		throw CommandException("Missing argument");
	}
	auto& recorder = OUTER(AviRecorder, recordCommand);
	executeSubCommand(tokens[1].getString(),
		"start",  [&]{ recorder.processStart(getInterpreter(), tokens, result); },
		"stop",   [&]{
			checkNumArgs(tokens, 2, Prefix{2}, nullptr);
			recorder.processStop(tokens); },
		"toggle", [&]{ recorder.processToggle(getInterpreter(), tokens, result); },
		"status", [&]{
			checkNumArgs(tokens, 2, Prefix{2}, nullptr);
			recorder.status(tokens, result); });
}

std::string AviRecorder::Cmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "Controls video recording: Write openMSX audio/video to a .avi file.\n"
	       "record start              Record to file 'openmsxNNNN.avi'\n"
	       "record start <filename>   Record to given file\n"
	       "record start -prefix foo  Record to file 'fooNNNN.avi'\n"
	       "record stop               Stop recording\n"
	       "record toggle             Toggle recording (useful as keybinding)\n"
	       "record status             Query recording state\n"
	       "\n"
	       "The start subcommand also accepts an optional -audioonly, -videoonly, "
	       " -mono, -stereo, -doublesize, -triplesize flag.\n"
	       "Videos are recorded in a 320x240 size by default, at 640x480 when the "
	       "-doublesize flag is used and at 960x720 when the -triplesize flag is used.";
}

void AviRecorder::Cmd::tabCompletion(std::vector<std::string>& tokens) const
{
	using namespace std::literals;
	if (tokens.size() == 2) {
		static constexpr std::array cmds = {
			"start"sv, "stop"sv, "toggle"sv, "status"sv,
		};
		completeString(tokens, cmds);
	} else if ((tokens.size() >= 3) && (tokens[1] == "start")) {
		static constexpr std::array options = {
			"-prefix"sv, "-videoonly"sv, "-audioonly"sv,
			"-doublesize"sv, "-triplesize"sv,
			"-mono"sv, "-stereo"sv,
		};
		completeFileName(tokens, userFileContext(), options);
	}
}

} // namespace openmsx
