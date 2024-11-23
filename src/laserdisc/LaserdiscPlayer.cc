#include "LaserdiscPlayer.hh"
#include "CommandException.hh"
#include "CommandController.hh"
#include "EventDistributor.hh"
#include "FileContext.hh"
#include "DeviceConfig.hh"
#include "HardwareConfig.hh"
#include "XMLElement.hh"
#include "CassettePort.hh"
#include "MSXCliComm.hh"
#include "Display.hh"
#include "GlobalSettings.hh"
#include "Reactor.hh"
#include "ReverseManager.hh"
#include "MSXMotherBoard.hh"
#include "PioneerLDControl.hh"
#include "LDRenderer.hh"
#include "RendererFactory.hh"
#include "Math.hh"
#include "narrow.hh"
#include "one_of.hh"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>

using std::string;

namespace openmsx {

static std::string_view getLaserDiscPlayerName()
{
	return "laserdiscplayer";
}

// Command

LaserdiscPlayer::Command::Command(
		CommandController& commandController_,
		StateChangeDistributor& stateChangeDistributor_,
		Scheduler& scheduler_)
	: RecordedCommand(commandController_, stateChangeDistributor_,
			  scheduler_, getLaserDiscPlayerName())
{
}

void LaserdiscPlayer::Command::execute(
	std::span<const TclObject> tokens, TclObject& result, EmuTime::param time)
{
	auto& laserdiscPlayer = OUTER(LaserdiscPlayer, laserdiscCommand);
	if (tokens.size() == 1) {
		// Returning Tcl lists here, similar to the disk commands in
		// DiskChanger
		result.addListElement(tmpStrCat(getName(), ':'),
		                      laserdiscPlayer.getImageName().getResolved());
	} else if (tokens[1] == "eject") {
		checkNumArgs(tokens, 2, Prefix{2}, nullptr);
		result = "Ejecting laserdisc.";
		laserdiscPlayer.eject(time);
	} else if (tokens[1] == "insert") {
		checkNumArgs(tokens, 3, "filename");
		try {
			result = "Changing laserdisc.";
			laserdiscPlayer.setImageName(string(tokens[2].getString()), time);
		} catch (MSXException& e) {
			throw CommandException(std::move(e).getMessage());
		}
	} else {
		throw SyntaxError();
	}
}

string LaserdiscPlayer::Command::help(std::span<const TclObject> tokens) const
{
	if (tokens.size() >= 2) {
		if (tokens[1] == "insert") {
			return "Inserts the specified laserdisc image into "
			       "the laserdisc player.";
		} else if (tokens[1] == "eject") {
			return "Eject the laserdisc.";
		}
	}
	return "laserdiscplayer insert <filename> "
	       ": insert a (different) laserdisc image\n"
	       "laserdiscplayer eject             "
	       ": eject the laserdisc\n";
}

void LaserdiscPlayer::Command::tabCompletion(std::vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		using namespace std::literals;
		static constexpr std::array extra = {"eject"sv, "insert"sv};
		completeString(tokens, extra);
	} else if (tokens.size() == 3 && tokens[1] == "insert") {
		completeFileName(tokens, userFileContext());
	}
}

// LaserdiscPlayer

static constexpr unsigned DUMMY_INPUT_RATE = 44100; // actual rate depends on .ogg file

LaserdiscPlayer::LaserdiscPlayer(
		const HardwareConfig& hwConf, PioneerLDControl& ldControl_)
	: ResampledSoundDevice(hwConf.getMotherBoard(), getLaserDiscPlayerName(),
	                       "Laserdisc Player", 1, DUMMY_INPUT_RATE, true)
	, syncAck (hwConf.getMotherBoard().getScheduler())
	, syncOdd (hwConf.getMotherBoard().getScheduler())
	, syncEven(hwConf.getMotherBoard().getScheduler())
	, motherBoard(hwConf.getMotherBoard())
	, ldControl(ldControl_)
	, laserdiscCommand(motherBoard.getCommandController(),
		           motherBoard.getStateChangeDistributor(),
		           motherBoard.getScheduler())
	, autoRunSetting(
		motherBoard.getCommandController(), "autorunlaserdisc",
		"automatically try to run Laserdisc", true)
	, loadingIndicator(
		motherBoard.getReactor().getGlobalSettings().getThrottleManager())
{
	motherBoard.getCassettePort().setLaserdiscPlayer(this);

	Reactor& reactor = motherBoard.getReactor();
	reactor.getDisplay().attach(*this);

	createRenderer();
	reactor.getEventDistributor().registerEventListener(EventType::BOOT, *this);
	scheduleDisplayStart(getCurrentTime());

	static const XMLElement* xml = [] {
		auto& doc = XMLDocument::getStaticDocument();
		auto* result = doc.allocateElement(string(getLaserDiscPlayerName()).c_str());
		result->setFirstChild(doc.allocateElement("sound"))
		      ->setFirstChild(doc.allocateElement("volume", "30000"));
		return result;
	}();
	registerSound(DeviceConfig(hwConf, *xml));

	motherBoard.registerMediaInfo(getLaserDiscPlayerName(), *this);
	motherBoard.getMSXCliComm().update(CliComm::UpdateType::HARDWARE, getLaserDiscPlayerName(), "add");
}

LaserdiscPlayer::~LaserdiscPlayer()
{
	unregisterSound();
	Reactor& reactor = motherBoard.getReactor();
	reactor.getDisplay().detach(*this);
	reactor.getEventDistributor().unregisterEventListener(EventType::BOOT, *this);
	motherBoard.unregisterMediaInfo(*this);
	motherBoard.getMSXCliComm().update(CliComm::UpdateType::HARDWARE, getLaserDiscPlayerName(), "remove");
}

string LaserdiscPlayer::getStateString() const
{
	switch (playerState) {
		using enum PlayerState;
		case STOPPED:     return "stopped";
		case PLAYING:     return "playing";
		case MULTI_SPEED: return "multispeed";
		case PAUSED:      return "paused";
		case STILL:       return "still";
	}
	UNREACHABLE;
}

void LaserdiscPlayer::getMediaInfo(TclObject& result)
{
	result.addDictKeyValues("target", getImageName().getResolved(),
	                        "state", getStateString());
}

void LaserdiscPlayer::scheduleDisplayStart(EmuTime::param time)
{
	Clock<60000, 1001> frameClock(time);
	// The video is 29.97Hz, however we need to do vblank processing
	// at the full 59.94Hz
	syncOdd .setSyncPoint(frameClock + 1);
	syncEven.setSyncPoint(frameClock + 2);
}

// The protocol used to communicate over the cable for commands to the
// laserdisc player is the NEC infrared protocol with minor deviations:
// 1) The leader pulse and space is a little shorter.
// 2) The remote does not send NEC repeats; full NEC codes are repeated
//    after 20ms. The main unit does not understand NEC repeats.
// 3) No carrier modulation is done over the ext protocol.
//
// My Laserdisc player is an Pioneer LD-700 which has a remote called
// the CU-700. This is much like the CU-CLD106 which is described
// here: http://lirc.sourceforge.net/remotes/pioneer/CU-CLD106
// The codes and protocol are exactly the same.
void LaserdiscPlayer::extControl(bool bit, EmuTime::param time)
{
	if (remoteLastBit == bit) return;
	remoteLastBit = bit;

	// The tolerance here is based on actual measurements of an LD-700
	EmuDuration duration = time - remoteLastEdge;
	remoteLastEdge = time;
	unsigned usec = duration.getTicksAt(1000000); // microseconds

	switch (remoteState) {
	using enum RemoteState;
	case IDLE:
		if (bit) {
			remoteBits = remoteBitNr = 0;
			remoteState = HEADER_PULSE;
		}
		break;
	case HEADER_PULSE:
		if (5800 <= usec && usec < 11200) {
			remoteState = NEC_HEADER_SPACE;
		} else {
			remoteState = IDLE;
		}
		break;
	case NEC_HEADER_SPACE:
		if (3400 <= usec && usec < 6200) {
			remoteState = NEC_BITS_PULSE;
		} else {
			remoteState = IDLE;
		}
		break;
	case NEC_BITS_PULSE:
		if (usec >= 380 && usec < 1070) {
			remoteState = NEC_BITS_SPACE;
		} else {
			remoteState = IDLE;
		}
		break;
	case NEC_BITS_SPACE:
		if (1260 <= usec && usec < 4720) {
			// bit 1
			remoteBits |= 1 << remoteBitNr;
		} else if (usec < 300 || usec >= 1065) {
			// error
			remoteState = IDLE;
			break;
		}

		// since it does not matter how long the trailing pulse
		// is, we process the button here. Note that real hardware
		// needs the trailing pulse to be at least 200µs
		if (++remoteBitNr == 32) {
			auto custom      = narrow_cast<byte>(( remoteBits >>  0) & 0xff);
			auto customCompl = narrow_cast<byte>((~remoteBits >>  8) & 0xff);
			auto code        = narrow_cast<byte>(( remoteBits >> 16) & 0xff);
			auto codeCompl   = narrow_cast<byte>((~remoteBits >> 24) & 0xff);
			if (custom == customCompl &&
			    custom == 0xa8 &&
			    code == codeCompl) {
				submitRemote(RemoteProtocol::NEC, code);
			}
			remoteState = IDLE;
		} else {
			remoteState = NEC_BITS_PULSE;
		}

		break;
	}
}

void LaserdiscPlayer::submitRemote(RemoteProtocol protocol, uint8_t code)
{
	// The END command for seeking/waiting acknowledges repeats,
	// Esh's Aurunmilla needs play as well.
	if (protocol != remoteProtocol || code != remoteCode ||
	    (protocol == RemoteProtocol::NEC && (code == one_of(0x42u, 0x17u)))) {
		remoteProtocol = protocol;
		remoteCode = code;
		remoteVblanksBack = 0;
		remoteExecuteDelayed = true;
	} else {
		// remote ignored
		remoteVblanksBack = 0;
		remoteExecuteDelayed = false;
	}
}

const RawFrame* LaserdiscPlayer::getRawFrame() const
{
	return renderer->getRawFrame();
}

void LaserdiscPlayer::setAck(EmuTime::param time, int wait)
{
	// activate ACK for 'wait' milliseconds
	syncAck.removeSyncPoint();
	syncAck.setSyncPoint(time + EmuDuration::msec(wait));
	ack = true;
}

void LaserdiscPlayer::remoteButtonNEC(uint8_t code, EmuTime::param time)
{
#ifdef DEBUG
	string f;
	switch (code) {
	case 0x47: f = "C+"; break;	// Increase playing speed
	case 0x46: f = "C-"; break;	// Decrease playing speed
	case 0x43: f = "D+"; break;	// Show Frame# & Chapter# OSD
	case 0x4b: f = "L+"; break;	// right
	case 0x49: f = "L-"; break;	// left
	case 0x4a: f = "L@"; break;	// stereo
	case 0x58: f = "M+"; break;	// multi speed forwards
	case 0x55: f = "M-"; break;	// multi speed backwards
	case 0x17: f = "P+"; break;	// play
	case 0x16: f = "P@"; break;	// stop
	case 0x18: f = "P/"; break;	// pause
	case 0x54: f = "S+"; break;	// frame step forward
	case 0x50: f = "S-"; break;	// frame step backwards
	case 0x45: f = "X+"; break;	// clear
	case 0x41: f = 'F'; break;	// seek frame
	case 0x40: f = 'C'; break;	// seek chapter
	case 0x42: f = "END"; break;	// done seek frame/chapter
	case 0x00: f = '0'; break;
	case 0x01: f = '1'; break;
	case 0x02: f = '2'; break;
	case 0x03: f = '3'; break;
	case 0x04: f = '4'; break;
	case 0x05: f = '5'; break;
	case 0x06: f = '6'; break;
	case 0x07: f = '7'; break;
	case 0x08: f = '8'; break;
	case 0x09: f = '9'; break;
	case 0x5f: f = "WAIT FRAME"; break;

	case 0x53: // previous chapter
	case 0x52: // next chapter
	default: break;
	}

	if (!f.empty()) {
		std::cerr << "LaserdiscPlayer::remote " << f << '\n';
	} else {
		std::cerr << "LaserdiscPlayer::remote unknown " << std::hex << code << '\n';
	}
#endif
	// When not playing the following buttons work
	// 0x17: start playing (ack sent)
	// 0x16: eject (no ack)
	// 0x49, 0x4a, 0x4b (ack sent)
	// if 0x49 is a repeat then no ACK is sent
	// if 0x49 is followed by 0x4a then ACK is sent
	if (code == one_of(0x49u, 0x4au, 0x4bu)) {
		updateStream(time);

		switch (code) {
		case 0x4b: // L+ (both channels play the left channel)
			stereoMode = StereoMode::LEFT;
			break;
		case 0x49: // L- (both channels play the right channel)
			stereoMode = StereoMode::RIGHT;
			break;
		case 0x4a: // L@ (normal stereo)
			stereoMode = StereoMode::STEREO;
			break;
		}

		setAck(time, 46);
	} else if (playerState == PlayerState::STOPPED) {
		switch (code) {
		case 0x16: // P@
			motherBoard.getMSXCliComm().printWarning(
				"ejecting laserdisc");
			eject(time);
			break;
		case 0x17: // P+
			play(time);
			break;
		}

		// During playing, playing will be acked if not repeated
		// within less than 115ms
	} else {
		// TODO: while seeking, only a small subset of buttons work
		bool nonSeekAck = true;

		switch (code) {
		using enum SeekState;
		case 0x5f:
			seekState = WAIT;
			seekNum = 0;
			stillOnWaitFrame = false;
			nonSeekAck = false;
			break;
		case 0x41:
			seekState = FRAME;
			seekNum = 0;
			break;
		case 0x40:
			seekState = CHAPTER;
			seekNum = 0;
			nonSeekAck = video->getChapter(0) != 0;
			break;
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
		case 0x08:
		case 0x09:
			seekNum = seekNum * 10 + code;
			break;
		case 0x42:
			switch (seekState) {
			case FRAME:
				seekState = NONE;
				seekFrame(seekNum % 100000, time);
				nonSeekAck = false;
				break;
			case CHAPTER:
				seekState = NONE;
				seekChapter(seekNum % 100, time);
				nonSeekAck = false;
				break;
			case WAIT:
				seekState = NONE;
				waitFrame = seekNum % 100000;
				if (waitFrame >= 101 && waitFrame < 200) {
					auto frame = video->getChapter(
						int(waitFrame - 100));
					if (frame) waitFrame = frame;
				}
				break;
			default:
				seekState = NONE;
				break;
			}
			break;
		case 0x45: // Clear "X+"
			if (seekState != NONE && seekNum != 0) {
				seekNum = 0;
			} else {
				seekState = NONE;
				seekNum = 0;
			}
			waitFrame = 0;
			break;
		case 0x18: // P/
			pause(time);
			nonSeekAck = false;
			break;
		case 0x17: // P+
			play(time);
			nonSeekAck = false;
			break;
		case 0x16: // P@ (stop/eject)
			stop(time);
			nonSeekAck = false;
			break;
		case 0xff:
			nonSeekAck = false;
			seekState = NONE;
			break;
		case 0x54: // S+ (frame step forward)
			if (seekState == WAIT) {
				stillOnWaitFrame = true;
			} else {
				stepFrame(true);
			}
			break;
		case 0x50: // S- (frame step backwards)
			stepFrame(false);
			break;
		case 0x55: // M- (multi-speed backwards)
			// Not supported
			motherBoard.getMSXCliComm().printWarning(
				"The Laserdisc player received a command to "
				"play backwards (M-). This is currently not "
				"supported.");
			nonSeekAck = false;
			break;
		case 0x58: // M+ (multi-speed forwards)
			playerState = PlayerState::MULTI_SPEED;
			setFrameStep();
			break;
		case 0x46: // C- (play slower)
			if (playingSpeed >= SPEED_STEP1) {
				playingSpeed--;
				frameStep = 1;	// TODO: is this correct?
			}
			break;
		case 0x47: // C+ (play faster)
			if (playingSpeed <= SPEED_X2) {
				playingSpeed++;
				frameStep = 1;	// TODO: is this correct?
			}
			break;
		default:
			motherBoard.getMSXCliComm().printWarning(
				"The Laserdisc player received an unknown "
				"command 0x", hex_string<2>(code));
			nonSeekAck = false;
			break;
		}

		if (nonSeekAck) {
			// All ACKs for operations which do not
			// require seeking
			setAck(time, 46);
		}
	}
}

void LaserdiscPlayer::execSyncAck(EmuTime::param time)
{
	updateStream(time);

	if (seeking && playerState == PlayerState::PLAYING) {
		sampleClock.advance(time);
	}

	ack = false;
	seeking = false;
}

void LaserdiscPlayer::execSyncFrame(EmuTime::param time, bool odd)
{
	updateStream(time);

	if (!odd || (video && video->getFrameRate() == 60)) {
		if ((playerState != PlayerState::STOPPED) &&
		    (currentFrame > video->getFrames())) {
			playerState = PlayerState::STOPPED;
		}

		if (auto* rawFrame = renderer->getRawFrame()) {
			renderer->frameStart(time);

			if (isVideoOutputAvailable(time)) {
				assert(video);
				auto frame = currentFrame;
				if (video->getFrameRate() == 60) {
					frame *= 2;
					if (odd) frame--;
				}

				video->getFrameNo(*rawFrame, frame);

				if (!odd) {
					nextFrame(time);
				}
			} else {
				renderer->drawBlank(0, 128, 196);
			}
			renderer->frameEnd();
		}

		// Update throttling
		loadingIndicator.update(seeking || sampleReads > 500);
		sampleReads = 0;

		if (!odd) {
			scheduleDisplayStart(time);
		}
	}

	// Processing of the remote control happens at each frame
	// (even and odd, so at 59.94Hz)
	if (remoteProtocol == RemoteProtocol::NEC) {
		if (remoteExecuteDelayed) {
			remoteButtonNEC(remoteCode, time);
		}

		if (++remoteVblanksBack > 6) {
			remoteProtocol = RemoteProtocol::NONE;
		}
	}
	remoteExecuteDelayed = false;
}

void LaserdiscPlayer::setFrameStep()
{
	switch (playingSpeed) {
	case SPEED_X3:
	case SPEED_X2:
	case SPEED_X1:
		frameStep = 1;
		break;
	case SPEED_1IN2:
		frameStep = 2;
		break;
	case SPEED_1IN4:
		frameStep = 4;
		break;
	case SPEED_1IN8:
		frameStep = 8;
		break;
	case SPEED_1IN16:
		frameStep = 16;
		break;
	case SPEED_STEP1:
		frameStep = 30;
		break;
	case SPEED_STEP3:
		frameStep = 90;
		break;
	}
}

void LaserdiscPlayer::nextFrame(EmuTime::param time)
{
	using enum PlayerState;
	if (waitFrame && waitFrame == currentFrame) {
		// Leave ACK raised until the next command
		ack = true;
		waitFrame = 0;

		if (stillOnWaitFrame) {
			playingFromSample = getCurrentSample(time);
			playerState = STILL;
			stillOnWaitFrame = false;
		}
	}

	if (playerState == MULTI_SPEED) {
		if (--frameStep)  {
			return;
		}

		switch (playingSpeed) {
		case SPEED_X3:
			currentFrame += 3;
			break;
		case SPEED_X2:
			currentFrame += 2;
			break;
		default:
			currentFrame += 1;
			break;
		}
		setFrameStep();
	} else if (playerState == PLAYING) {
		currentFrame++;
	}

	// freeze if stop frame
	if (playerState == one_of(PLAYING, MULTI_SPEED)) {
		assert(video);
		if (video->stopFrame(currentFrame)) {
			// stop frame reached
			playingFromSample = getCurrentSample(time);
			playerState = STILL;
		}
	}
}

void LaserdiscPlayer::setImageName(string newImage, EmuTime::param time)
{
	stop(time);
	oggImage = Filename(std::move(newImage), userFileContext());
	video.emplace(oggImage, motherBoard.getMSXCliComm());

	unsigned inputRate = video->getSampleRate();
	sampleClock.setFreq(inputRate);
	if (inputRate != getInputRate()) {
		setInputRate(inputRate);
		createResampler();
	}
}

bool LaserdiscPlayer::signalEvent(const Event& event)
{
	if (getType(event) == EventType::BOOT && video) {
		autoRun();
	}
	return false;
}

void LaserdiscPlayer::autoRun()
{
	if (!autoRunSetting.getBoolean()) return;
	if (motherBoard.getReverseManager().isReplaying()) {
		// See comments in CassettePlayer::autoRun()
		return;
	}

	string var = "::auto_run_ld_counter";
	string command = strCat(
		"if ![info exists ", var, "] { set ", var, " 0 }\n"
		"incr ", var, "\n"
		"after time 2 \"if $", var, "==\\$", var, " { "
		"type_via_keyboard 1CALLLD\\\\r }\"");
	try {
		motherBoard.getCommandController().executeCommand(command);
	} catch (CommandException& e) {
		motherBoard.getMSXCliComm().printWarning(
			"Error executing loading instruction for AutoRun: ",
			e.getMessage(), "\n Please report a bug.");
	}
}

void LaserdiscPlayer::generateChannels(std::span<float*> buffers, unsigned num)
{
	// Single channel device: replace content of buffers[0] (not add to it).
	assert(buffers.size() == 1);
	if (playerState != PlayerState::PLAYING || seeking || (muteLeft && muteRight)) {
		buffers[0] = nullptr;
		return;
	}
	assert(video);

	unsigned pos = 0;
	size_t currentSample;

	if (!sampleClock.before(start)) [[unlikely]] {
		// Before playing of sounds begins
		EmuDuration duration = sampleClock.getTime() - start;
		unsigned len = duration.getTicksAt(video->getSampleRate());
		if (len >= num) {
			buffers[0] = nullptr;
			return;
		}

		for (/**/; pos < len; ++pos) {
			buffers[0][pos * 2 + 0] = 0.0f;
			buffers[0][pos * 2 + 1] = 0.0f;
		}

		currentSample = playingFromSample;
	} else {
		currentSample = getCurrentSample(start);
	}

	unsigned drift = video->getSampleRate() / 30;

	if (currentSample > (lastPlayedSample + drift) ||
	    (currentSample + drift) < lastPlayedSample) {
		// audio drift
		lastPlayedSample = currentSample;
	}

	int left = stereoMode == StereoMode::RIGHT ? 1 : 0;
	int right = stereoMode == StereoMode::LEFT ? 0 : 1;

	while (pos < num) {
		const AudioFragment* audio = video->getAudio(lastPlayedSample);

		if (!audio) {
			if (pos == 0) {
				buffers[0] = nullptr;
				break;
			} else {
				for (/**/; pos < num; ++pos) {
					buffers[0][pos * 2 + 0] = 0.0f;
					buffers[0][pos * 2 + 1] = 0.0f;
				}
			}
		} else {
			auto offset = unsigned(lastPlayedSample - audio->position);
			unsigned len = std::min(audio->length - offset, num - pos);

			// maybe muting should be moved out of the loop?
			for (unsigned i = 0; i < len; ++i, ++pos) {
				buffers[0][pos * 2 + 0] = muteLeft ? 0.0f :
				   audio->pcm[left][offset + i];
				buffers[0][pos * 2 + 1] = muteRight ? 0.0f :
				   audio->pcm[right][offset + i];
			}

			lastPlayedSample += len;
		}
	}
}

float LaserdiscPlayer::getAmplificationFactorImpl() const
{
	return 2.0f;
}

bool LaserdiscPlayer::updateBuffer(size_t length, float* buffer,
                                   EmuTime::param time)
{
	bool result = ResampledSoundDevice::updateBuffer(length, buffer, time);
	start = time; // current end-time is next start-time
	return result;
}

void LaserdiscPlayer::setMuting(bool left, bool right, EmuTime::param time)
{
	updateStream(time);
	muteLeft = left;
	muteRight = right;
}

void LaserdiscPlayer::play(EmuTime::param time)
{
	if (!video) return;

	updateStream(time);

	using enum PlayerState;
	if (seeking) {
		// Do not ACK, play while seeking
	} else if (playerState == STOPPED) {
		// Disk needs to spin up, which takes 9.6s on
		// my Pioneer LD-92000. Also always seek to
		// beginning (confirmed on real MSX and LD)
		video->seek(1, 0);
		lastPlayedSample = 0;
		playingFromSample = 0;
		currentFrame = 1;
		// Note that with "fullspeedwhenloading" this
		// should be reduced to.
		setAck(time, 9600);
		seekState = SeekState::NONE;
		seeking = true;
		waitFrame = 0;
		stereoMode = StereoMode::STEREO;
		playingSpeed = SPEED_1IN4;
	} else if (playerState == PLAYING) {
		// If Play command is issued while the player
		// is already playing, then if no ACK is sent then
		// Astron Belt will send LD1100 commands
		setAck(time, 46);
	} else if (playerState == MULTI_SPEED) {
		// Should be hearing stuff again
		playingFromSample = (currentFrame - 1LL) * 1001LL *
				video->getSampleRate() / 30000LL;
		sampleClock.advance(time);
		setAck(time, 46);
	} else {
		// STILL or PAUSED
		sampleClock.advance(time);
		setAck(time, 46);
	}
	playerState = PLAYING;
}

size_t LaserdiscPlayer::getCurrentSample(EmuTime::param time)
{
	switch(playerState) {
	case PlayerState::PAUSED:
	case PlayerState::STILL:
		return playingFromSample;
	default:
		return playingFromSample + sampleClock.getTicksTill(time);
	}
}

void LaserdiscPlayer::pause(EmuTime::param time)
{
	using enum PlayerState;
	if (playerState == STOPPED) return;

	updateStream(time);

	if (playerState == PLAYING) {
		playingFromSample = getCurrentSample(time);
	} else if (playerState == MULTI_SPEED) {
		playingFromSample = (currentFrame - 1LL) * 1001LL *
				video->getSampleRate() / 30000LL;
		sampleClock.advance(time);
	}

	playerState = PAUSED;
	setAck(time, 46);
}

void LaserdiscPlayer::stop(EmuTime::param time)
{
	if (playerState == PlayerState::STOPPED) return;

	updateStream(time);
	playerState = PlayerState::STOPPED;
}

void LaserdiscPlayer::eject(EmuTime::param time)
{
	stop(time);
	oggImage = {};
	video.reset();
}

// Step one frame forwards or backwards. The frame will be visible and
// we won't be playing afterwards
void LaserdiscPlayer::stepFrame(bool forwards)
{
	// TODO can video be nullopt?
	bool needSeek = false;

	// Note that on real hardware, the screen goes dark momentarily
	// if you try to step before the first frame or after the last one
	if (playerState == PlayerState::STILL) {
		assert(video);
		if (forwards) {
			if (currentFrame < video->getFrames()) {
				currentFrame++;
			}
		} else {
			if (currentFrame > 1) {
				currentFrame--;
				needSeek = true;
			}
		}
	}

	playerState = PlayerState::STILL;
	auto samplePos = (currentFrame - 1LL) * 1001LL *
	                    video->getSampleRate() / 30000LL;
	playingFromSample = samplePos;

	if (needSeek) {
		if (video->getFrameRate() == 60)
			video->seek(currentFrame * 2, samplePos);
		else
			video->seek(currentFrame, samplePos);
	}
}

void LaserdiscPlayer::seekFrame(size_t toFrame, EmuTime::param time)
{
	if ((playerState == PlayerState::STOPPED) || !video) return;

	updateStream(time);

	if (toFrame <= 0) toFrame = 1;
	if (toFrame > video->getFrames()) toFrame = video->getFrames();

	// Seek time needs to be emulated correctly since
	// e.g. Astron Belt does not wait for the seek
	// to complete, it simply assumes a certain
	// delay.
	//
	// This calculation is based on measurements on
	// a Pioneer LD-92000.
	auto dist = std::abs(int64_t(toFrame) - int64_t(currentFrame));
	int seekTime = (dist < 1000) // time in ms
			? narrow<int>(dist + 300)
			: narrow<int>(1800 + dist / 12);

	auto samplePos = (toFrame - 1LL) * 1001LL *
				video->getSampleRate() / 30000LL;

	if (video->getFrameRate() == 60) {
		video->seek(toFrame * 2, samplePos);
	} else {
		video->seek(toFrame, samplePos);
	}
	playerState = PlayerState::STILL;
	playingFromSample = samplePos;
	currentFrame = toFrame;

	// Seeking clears the frame to wait for
	waitFrame = 0;

	seeking = true;
	setAck(time, seekTime);
}

void LaserdiscPlayer::seekChapter(int chapter, EmuTime::param time)
{
	if ((playerState == PlayerState::STOPPED) || !video) return;

	auto frameNo = video->getChapter(chapter);
	if (!frameNo) return;
	seekFrame(frameNo, time);
}

int16_t LaserdiscPlayer::readSample(EmuTime::param time)
{
	// Here we should return the value of the sample on the
	// right audio channel, ignoring muting (this is done in the MSX)
	// but honouring the stereo mode as this is done in the
	// Laserdisc player
	if (playerState == PlayerState::PLAYING && !seeking) {
		assert(video);
		auto sample = getCurrentSample(time);
		if (const AudioFragment* audio = video->getAudio(sample)) {
			++sampleReads;
			int channel = stereoMode == StereoMode::LEFT ? 0 : 1;
			return narrow_cast<int16_t>
				(audio->pcm[channel][sample - audio->position]
							 * 32767.f);
		}
	}
	return 0;
}

bool LaserdiscPlayer::isVideoOutputAvailable(EmuTime::param time)
{
	updateStream(time);

	bool videoOut = [&] {
		switch (playerState) {
		case PlayerState::PLAYING:
		case PlayerState::MULTI_SPEED:
		case PlayerState::STILL:
			return !seeking;
		default:
			return false;
		}
	}();
	ldControl.videoIn(videoOut);

	return videoOut;
}

void LaserdiscPlayer::preVideoSystemChange() noexcept
{
	renderer.reset();
}

void LaserdiscPlayer::postVideoSystemChange() noexcept
{
	createRenderer();
}

void LaserdiscPlayer::createRenderer()
{
	Display& display = getMotherBoard().getReactor().getDisplay();
	renderer = RendererFactory::createLDRenderer(*this, display);
}

static constexpr std::initializer_list<enum_string<LaserdiscPlayer::RemoteState>> RemoteStateInfo = {
	{ "IDLE",             LaserdiscPlayer::RemoteState::IDLE             },
	{ "HEADER_PULSE",     LaserdiscPlayer::RemoteState::HEADER_PULSE     },
	{ "NEC_HEADER_SPACE", LaserdiscPlayer::RemoteState::NEC_HEADER_SPACE },
	{ "NEC_BITS_PULSE",   LaserdiscPlayer::RemoteState::NEC_BITS_PULSE   },
	{ "NEC_BITS_SPACE",   LaserdiscPlayer::RemoteState::NEC_BITS_SPACE   },
};
SERIALIZE_ENUM(LaserdiscPlayer::RemoteState, RemoteStateInfo);

static constexpr std::initializer_list<enum_string<LaserdiscPlayer::PlayerState>> PlayerStateInfo = {
	{ "STOPPED",    LaserdiscPlayer::PlayerState::STOPPED     },
	{ "PLAYING",    LaserdiscPlayer::PlayerState::PLAYING     },
	{ "MULTISPEED", LaserdiscPlayer::PlayerState::MULTI_SPEED },
	{ "PAUSED",     LaserdiscPlayer::PlayerState::PAUSED      },
	{ "STILL",      LaserdiscPlayer::PlayerState::STILL       }
};
SERIALIZE_ENUM(LaserdiscPlayer::PlayerState, PlayerStateInfo);

static constexpr std::initializer_list<enum_string<LaserdiscPlayer::SeekState>> SeekStateInfo = {
	{ "NONE",    LaserdiscPlayer::SeekState::NONE    },
	{ "CHAPTER", LaserdiscPlayer::SeekState::CHAPTER },
	{ "FRAME",   LaserdiscPlayer::SeekState::FRAME   },
	{ "WAIT",    LaserdiscPlayer::SeekState::WAIT    }
};
SERIALIZE_ENUM(LaserdiscPlayer::SeekState, SeekStateInfo);

static constexpr std::initializer_list<enum_string<LaserdiscPlayer::StereoMode>> StereoModeInfo = {
	{ "LEFT",   LaserdiscPlayer::StereoMode::LEFT   },
	{ "RIGHT",  LaserdiscPlayer::StereoMode::RIGHT  },
	{ "STEREO", LaserdiscPlayer::StereoMode::STEREO }
};
SERIALIZE_ENUM(LaserdiscPlayer::StereoMode, StereoModeInfo);

static constexpr std::initializer_list<enum_string<LaserdiscPlayer::RemoteProtocol>> RemoteProtocolInfo = {
	{ "NONE", LaserdiscPlayer::RemoteProtocol::NONE },
	{ "NEC",  LaserdiscPlayer::RemoteProtocol::NEC  },
};
SERIALIZE_ENUM(LaserdiscPlayer::RemoteProtocol, RemoteProtocolInfo);

// version 1: initial version
// version 2: added 'stillOnWaitFrame'
// version 3: reversed bit order of 'remoteBits' and 'remoteCode'
// version 4: removed 'userData' from Schedulable
template<typename Archive>
void LaserdiscPlayer::serialize(Archive& ar, unsigned version)
{
	// Serialize remote control
	ar.serialize("RemoteState", remoteState);
	if (remoteState != RemoteState::IDLE) {
		ar.serialize("RemoteBitNr", remoteBitNr,
		             "RemoteBits",  remoteBits);
		if (ar.versionBelow(version, 3)) {
			assert(Archive::IS_LOADER);
			remoteBits = Math::reverseNBits(remoteBits, remoteBitNr);
		}
	}
	ar.serialize("RemoteLastBit",  remoteLastBit,
	             "RemoteLastEdge", remoteLastEdge,
	             "RemoteProtocol", remoteProtocol);
	if (remoteProtocol != RemoteProtocol::NONE) {
		ar.serialize("RemoteCode", remoteCode);
		if (ar.versionBelow(version, 3)) {
			assert(Archive::IS_LOADER);
			remoteCode = Math::reverseByte(remoteCode);
		}
		ar.serialize("RemoteExecuteDelayed", remoteExecuteDelayed,
		             "RemoteVblanksBack",    remoteVblanksBack);
	}

	// Serialize filename
	ar.serialize("OggImage", oggImage);
	if constexpr (Archive::IS_LOADER) {
		sampleReads = 0;
		if (!oggImage.empty()) {
			setImageName(oggImage.getResolved(), getCurrentTime());
		} else {
			video.reset();
		}
	}
	ar.serialize("PlayerState", playerState);

	if (playerState != PlayerState::STOPPED) {
		// Serialize seek state
		ar.serialize("SeekState", seekState);
		if (seekState != SeekState::NONE) {
			ar.serialize("SeekNum", seekNum);
		}
		ar.serialize("seeking", seeking);

		// Playing state
		ar.serialize("WaitFrame", waitFrame);

		// This was not yet implemented in openmsx 0.8.1 and earlier
		if (ar.versionAtLeast(version, 2)) {
			ar.serialize("StillOnWaitFrame", stillOnWaitFrame);
		}

		ar.serialize("ACK",          ack,
		             "PlayingSpeed", playingSpeed);

		// Frame position
		ar.serialize("CurrentFrame", currentFrame);
		if (playerState == PlayerState::MULTI_SPEED) {
			ar.serialize("FrameStep", frameStep);
		}

		// Audio position
		ar.serialize("StereoMode",  stereoMode,
		             "FromSample",  playingFromSample,
		             "SampleClock", sampleClock);

		if constexpr (Archive::IS_LOADER) {
			// If the sample rate differs, adjust accordingly
			if (video->getSampleRate() != sampleClock.getFreq()) {
				uint64_t pos = playingFromSample;

				pos *= video->getSampleRate();
				pos /= sampleClock.getFreq();

				playingFromSample = pos;
				sampleClock.setFreq(video->getSampleRate());
			}

			auto sample = getCurrentSample(getCurrentTime());
			if (video->getFrameRate() == 60)
				video->seek(currentFrame * 2, sample);
			else
				video->seek(currentFrame, sample);
			lastPlayedSample = sample;
		}
	}

	if (ar.versionAtLeast(version, 4)) {
		ar.serialize("syncEven", syncEven,
		             "syncOdd",  syncOdd,
		             "syncAck",  syncAck);
	} else {
		Schedulable::restoreOld(ar, {&syncEven, &syncOdd, &syncAck});
	}

	if constexpr (Archive::IS_LOADER) {
		(void)isVideoOutputAvailable(getCurrentTime());
	}
}

INSTANTIATE_SERIALIZE_METHODS(LaserdiscPlayer);

} // namespace openmsx
