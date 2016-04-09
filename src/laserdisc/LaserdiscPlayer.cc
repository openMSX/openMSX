#include "LaserdiscPlayer.hh"
#include "CommandException.hh"
#include "CommandController.hh"
#include "EventDistributor.hh"
#include "FileContext.hh"
#include "DeviceConfig.hh"
#include "HardwareConfig.hh"
#include "XMLElement.hh"
#include "CassettePort.hh"
#include "CliComm.hh"
#include "Display.hh"
#include "GlobalSettings.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "PioneerLDControl.hh"
#include "OggReader.hh"
#include "LDRenderer.hh"
#include "RendererFactory.hh"
#include "Math.hh"
#include "likely.hh"
#include "memory.hh"
#include <cstdint>
#include <cstdlib>
#include <iostream>

using std::string;
using std::vector;

namespace openmsx {

// Command

LaserdiscPlayer::Command::Command(
		CommandController& commandController_,
		StateChangeDistributor& stateChangeDistributor_,
		Scheduler& scheduler_)
	: RecordedCommand(commandController_, stateChangeDistributor_,
			  scheduler_, "laserdiscplayer")
{
}

void LaserdiscPlayer::Command::execute(
	array_ref<TclObject> tokens, TclObject& result, EmuTime::param time)
{
	auto& laserdiscPlayer = OUTER(LaserdiscPlayer, laserdiscCommand);
	if (tokens.size() == 1) {
		// Returning Tcl lists here, similar to the disk commands in
		// DiskChanger
		result.addListElement(getName() + ':');
		result.addListElement(laserdiscPlayer.getImageName().getResolved());
	} else if (tokens.size() == 2 && tokens[1] == "eject") {
		result.setString("Ejecting laserdisc.");
		laserdiscPlayer.eject(time);
	} else if (tokens.size() == 3 && tokens[1] == "insert") {
		try {
			result.setString("Changing laserdisc.");
			laserdiscPlayer.setImageName(tokens[2].getString().str(), time);
		} catch (MSXException& e) {
			throw CommandException(e.getMessage());
		}
	} else {
		throw SyntaxError();
	}
}

string LaserdiscPlayer::Command::help(const vector<string>& tokens) const
{
	if (tokens.size() >= 2) {
		if (tokens[1] == "insert") {
			return "Inserts the specfied laserdisc image into "
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

void LaserdiscPlayer::Command::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		static const char* const extra[] = { "eject", "insert" };
		completeString(tokens, extra);
	} else if (tokens.size() == 3 && tokens[1] == "insert") {
		completeFileName(tokens, userFileContext());
	}
}

// LaserdiscPlayer

static XMLElement createXML()
{
	XMLElement xml("laserdiscplayer");
	xml.addChild("sound").addChild("volume", "30000");
	return xml;
}

LaserdiscPlayer::LaserdiscPlayer(
		const HardwareConfig& hwConf, PioneerLDControl& ldcontrol_)
	: ResampledSoundDevice(hwConf.getMotherBoard(), "laserdiscplayer",
	                       "Laserdisc Player", 1, true)
	, syncAck (hwConf.getMotherBoard().getScheduler())
	, syncOdd (hwConf.getMotherBoard().getScheduler())
	, syncEven(hwConf.getMotherBoard().getScheduler())
	, motherBoard(hwConf.getMotherBoard())
	, ldcontrol(ldcontrol_)
	, laserdiscCommand(motherBoard.getCommandController(),
		           motherBoard.getStateChangeDistributor(),
		           motherBoard.getScheduler())
	, sampleClock(EmuTime::zero)
	, start(EmuTime::zero)
	, muteLeft(false)
	, muteRight(false)
	, remoteState(REMOTE_IDLE)
	, remoteLastEdge(EmuTime::zero)
	, remoteLastBit(false)
	, remoteProtocol(IR_NONE)
	, ack(false)
	, seeking(false)
	, playerState(PLAYER_STOPPED)
	, autoRunSetting(
		motherBoard.getCommandController(), "autorunlaserdisc",
		"automatically try to run Laserdisc", true)
	, loadingIndicator(
		motherBoard.getReactor().getGlobalSettings().getThrottleManager())
	, sampleReads(0)
{
	motherBoard.getCassettePort().setLaserdiscPlayer(this);

	Reactor& reactor = motherBoard.getReactor();
	reactor.getDisplay().attach(*this);

	createRenderer();
	reactor.getEventDistributor().registerEventListener(OPENMSX_BOOT_EVENT, *this);
	scheduleDisplayStart(getCurrentTime());

	setInputRate(44100); // Initialize with dummy value

	static XMLElement xml = createXML();
	registerSound(DeviceConfig(hwConf, xml));
}

LaserdiscPlayer::~LaserdiscPlayer()
{
	unregisterSound();
	Reactor& reactor = motherBoard.getReactor();
	reactor.getDisplay().detach(*this);
	reactor.getEventDistributor().unregisterEventListener(OPENMSX_BOOT_EVENT, *this);
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
	case REMOTE_IDLE:
		if (bit) {
			remoteBits = remoteBitNr = 0;
			remoteState = REMOTE_HEADER_PULSE;
		}
		break;
	case REMOTE_HEADER_PULSE:
		if (5800 <= usec && usec < 11200) {
			remoteState = NEC_HEADER_SPACE;
		} else {
			remoteState = REMOTE_IDLE;
		}
		break;
	case NEC_HEADER_SPACE:
		if (3400 <= usec && usec < 6200) {
			remoteState = NEC_BITS_PULSE;
		} else {
			remoteState = REMOTE_IDLE;
		}
		break;
	case NEC_BITS_PULSE:
		if (usec >= 380 && usec < 1070) {
			remoteState = NEC_BITS_SPACE;
		} else {
			remoteState = REMOTE_IDLE;
		}
		break;
	case NEC_BITS_SPACE:
		if (1260 <= usec && usec < 4720) {
			// bit 1
			remoteBits |= 1 << remoteBitNr;
		} else if (usec < 300 || usec >= 1065) {
			// error
			remoteState = REMOTE_IDLE;
			break;
		}

		// since it does not matter how long the trailing pulse
		// is, we process the button here. Note that real hardware
		// needs the trailing pulse to be at least 200µs
		if (++remoteBitNr == 32) {
			byte custom	 = ( remoteBits >>  0) & 0xff;
			byte customCompl = (~remoteBits >>  8) & 0xff;
			byte code	 = ( remoteBits >> 16) & 0xff;
			byte codeCompl	 = (~remoteBits >> 24) & 0xff;
			if (custom == customCompl &&
			    custom == 0xa8 &&
			    code == codeCompl) {
				submitRemote(IR_NEC, code);
			}
			remoteState = REMOTE_IDLE;
		} else {
			remoteState = NEC_BITS_PULSE;
		}

		break;
	}
}

void LaserdiscPlayer::submitRemote(RemoteProtocol protocol, unsigned code)
{
	// The END command for seeking/waiting acknowledges repeats,
	// Esh's Aurunmilla needs play as well.
	if (protocol != remoteProtocol || code != remoteCode ||
	    (protocol == IR_NEC && (code == 0x42 || code == 0x17))) {
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

void LaserdiscPlayer::remoteButtonNEC(unsigned code, EmuTime::param time)
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
		std::cerr << "LaserdiscPlayer::remote " << f << std::endl;
	} else {
		std::cerr << "LaserdiscPlayer::remote unknown " << std::hex << code << std::endl;
	}
#endif
	// When not playing the following buttons work
	// 0x17: start playing (ack sent)
	// 0x16: eject (no ack)
	// 0x49, 0x4a, 0x4b (ack sent)
	// if 0x49 is a repeat then no ACK is sent
	// if 0x49 is followed by 0x4a then ACK is sent
	if (code == 0x49 || code == 0x4a || code == 0x4b) {
		updateStream(time);

		switch (code) {
		case 0x4b: // L+ (both channels play the left channel)
			stereoMode = LEFT;
			break;
		case 0x49: // L- (both channels play the right channel)
			stereoMode = RIGHT;
			break;
		case 0x4a: // L@ (normal stereo)
			stereoMode = STEREO;
			break;
		}

		setAck(time, 46);
	} else if (playerState == PLAYER_STOPPED) {
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
		// FIXME: while seeking, only a small subset of buttons work
		bool nonseekack = true;

		switch (code) {
		case 0x5f:
			seekState = SEEK_WAIT;
			seekNum = 0;
			stillOnWaitFrame = false;
			nonseekack = false;
			break;
		case 0x41:
			seekState = SEEK_FRAME;
			seekNum = 0;
			break;
		case 0x40:
			seekState = SEEK_CHAPTER;
			seekNum = 0;
			nonseekack = video->getChapter(0) != 0;
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
			case SEEK_FRAME:
				seekState = SEEK_NONE;
				seekFrame(seekNum % 100000, time);
				nonseekack = false;
				break;
			case SEEK_CHAPTER:
				seekState = SEEK_NONE;
				seekChapter(seekNum % 100, time);
				nonseekack = false;
				break;
			case SEEK_WAIT:
				seekState = SEEK_NONE;
				waitFrame = seekNum % 100000;
				if (waitFrame >= 101 && waitFrame < 200) {
					auto frame = video->getChapter(
						int(waitFrame - 100));
					if (frame) waitFrame = frame;
				}
				break;
			default:
				seekState = SEEK_NONE;
				break;
			}
			break;
		case 0x45: // Clear "X+"
			if (seekState != SEEK_NONE && seekNum != 0) {
				seekNum = 0;
			} else {
				seekState = SEEK_NONE;
				seekNum = 0;
			}
			waitFrame = 0;
			break;
		case 0x18: // P/
			pause(time);
			nonseekack = false;
			break;
		case 0x17: // P+
			play(time);
			nonseekack = false;
			break;
		case 0x16: // P@ (stop/eject)
			stop(time);
			nonseekack = false;
			break;
		case 0xff:
			nonseekack = false;
			seekState = SEEK_NONE;
			break;
		case 0x54: // S+ (frame step forward)
			if (seekState == SEEK_WAIT) {
				stillOnWaitFrame = true;
			} else {
				stepFrame(true);
			}
			break;
		case 0x50: // S- (frame step backwards)
			stepFrame(false);
			break;
		case 0x55: // M- (multispeed backwards)
			// Not supported
			motherBoard.getMSXCliComm().printWarning(
				"The Laserdisc player received a command to "
				"play backwards (M-). This is currently not "
				"supported.");
			nonseekack = false;
			break;
		case 0x58: // M+ (multispeed forwards)
			playerState = PLAYER_MULTISPEED;
			setFrameStep();
			break;
		case 0x46: // C- (play slower)
			if (playingSpeed >= SPEED_STEP1) {
				playingSpeed--;
				frameStep = 1;	// FIXME: is this correct?
			}
			break;
		case 0x47: // C+ (play faster)
			if (playingSpeed <= SPEED_X2) {
				playingSpeed++;
				frameStep = 1;	// FIXME: is this correct?
			}
			break;
		default:
			motherBoard.getMSXCliComm().printWarning(
				"The Laserdisc player received an unknown "
				"command 0x" + StringOp::toHexString(code, 2));
			nonseekack = false;
			break;
		}

		if (nonseekack) {
			// All ACKs for operations which do not
			// require seeking
			setAck(time, 46);
		}
	}
}

void LaserdiscPlayer::execSyncAck(EmuTime::param time)
{
	updateStream(time);

	if (seeking && playerState == PLAYER_PLAYING) {
		sampleClock.advance(time);
	}

	ack = false;
	seeking = false;
}

void LaserdiscPlayer::execSyncFrame(EmuTime::param time, bool odd)
{
	updateStream(time);

	if (!odd || (video && video->getFrameRate() == 60)) {
		if ((playerState != PLAYER_STOPPED) &&
		    (currentFrame > video->getFrames())) {
			playerState = PLAYER_STOPPED;
		}

		if (auto* rawFrame = renderer->getRawFrame()) {
			renderer->frameStart(time);

			if (isVideoOutputAvailable(time)) {
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
	if (remoteProtocol == IR_NEC) {
		if (remoteExecuteDelayed) {
			remoteButtonNEC(remoteCode, time);
		}

		if (++remoteVblanksBack > 6) {
			remoteProtocol = IR_NONE;
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
	if (waitFrame && waitFrame == currentFrame) {
		// Leave ACK raised until the next command
		ack = true;
		waitFrame = 0;

		if (stillOnWaitFrame) {
			playingFromSample = getCurrentSample(time);
			playerState = PLAYER_STILL;
			stillOnWaitFrame = false;
		}
	}

	if (playerState == PLAYER_MULTISPEED) {
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
	} else if (playerState == PLAYER_PLAYING) {
		currentFrame++;
	}

	// freeze if stop frame
	if ((playerState == PLAYER_PLAYING || playerState == PLAYER_MULTISPEED)
	     && video->stopFrame(currentFrame)) {
		// stop frame reached
		playingFromSample = getCurrentSample(time);
		playerState = PLAYER_STILL;
	}
}

void LaserdiscPlayer::setImageName(string newImage, EmuTime::param time)
{
	stop(time);
	oggImage = Filename(std::move(newImage), userFileContext());
	video = make_unique<OggReader>(oggImage, motherBoard.getMSXCliComm());

	unsigned inputRate = video->getSampleRate();
	sampleClock.setFreq(inputRate);
	if (inputRate != getInputRate()) {
		setInputRate(inputRate);
		createResampler();
	}
}

int LaserdiscPlayer::signalEvent(const std::shared_ptr<const Event>& event)
{
	if (event->getType() == OPENMSX_BOOT_EVENT && video) {
		autoRun();
	}
	return 0;
}

void LaserdiscPlayer::autoRun()
{
	if (!autoRunSetting.getBoolean()) return;

	string var = "::auto_run_ld_counter";
	string command =
		"if ![info exists " + var + "] { set " + var + " 0 }\n"
		"incr " + var + "\n"
		"after time 2 \"if $" + var + "==\\$" + var + " { "
		"type 1CALLLD\\\\r }\"";

	try {
		motherBoard.getCommandController().executeCommand(command);
	} catch (CommandException& e) {
		motherBoard.getMSXCliComm().printWarning(
			"Error executing loading instruction for AutoRun: " +
			e.getMessage() + "\n Please report a bug.");
	}
}

void LaserdiscPlayer::generateChannels(int** buffers, unsigned num)
{
	// Single channel device: replace content of buffers[0] (not add to it).
	if (playerState != PLAYER_PLAYING || seeking || (muteLeft && muteRight)) {
		buffers[0] = nullptr;
		return;
	}

	unsigned pos = 0;
	size_t currentSample;

	if (unlikely(!sampleClock.before(start))) {
		// Before playing of sounds begins
		EmuDuration duration = sampleClock.getTime() - start;
		unsigned len = duration.getTicksAt(video->getSampleRate());
		if (len >= num) {
			buffers[0] = nullptr;
			return;
		}

		for (/**/; pos < len; ++pos) {
			buffers[0][pos * 2 + 0] = 0;
			buffers[0][pos * 2 + 1] = 0;
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

	int left = stereoMode == RIGHT ? 1 : 0;
	int right = stereoMode == LEFT ? 0 : 1;

	while (pos < num) {
		const AudioFragment* audio = video->getAudio(lastPlayedSample);

		if (!audio) {
			if (pos == 0) {
				buffers[0] = nullptr;
				break;
			} else for (/**/; pos < num; ++pos) {
				buffers[0][pos * 2 + 0] = 0;
				buffers[0][pos * 2 + 1] = 0;
			}
		} else {
			auto offset = unsigned(lastPlayedSample - audio->position);
			unsigned len = std::min(audio->length - offset, num - pos);

			// maybe muting should be moved out of the loop?
			for (unsigned i = 0; i < len; ++i, ++pos) {
				buffers[0][pos * 2 + 0] = muteLeft ? 0 :
				   int(audio->pcm[left][offset + i] * 65536.f);
				buffers[0][pos * 2 + 1] = muteRight ? 0 :
				   int(audio->pcm[right][offset + i] * 65536.f);
			}

			lastPlayedSample += len;
		}
	}
}

bool LaserdiscPlayer::updateBuffer(unsigned length, int* buffer,
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
	if (video) {
		updateStream(time);

		if (seeking) {
			// Do not ACK, play while seeking
		} else if (playerState == PLAYER_STOPPED) {
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
			seekState = SEEK_NONE;
			seeking = true;
			waitFrame = 0;
			stereoMode = STEREO;
			playingSpeed = SPEED_1IN4;
		} else if (playerState == PLAYER_PLAYING) {
			// If Play command is issued while the player
			// is already playing, then if no ACK is sent then
			// Astron Belt will send LD1100 commands
			setAck(time, 46);
		} else if (playerState == PLAYER_MULTISPEED) {
			// Should be hearing stuff again
			playingFromSample = (currentFrame - 1ll) * 1001ll *
					video->getSampleRate() / 30000ll;
			sampleClock.advance(time);
			setAck(time, 46);
		} else {
			// STILL or PAUSED
			sampleClock.advance(time);
			setAck(time, 46);
		}
		playerState = PLAYER_PLAYING;
	}
}

size_t LaserdiscPlayer::getCurrentSample(EmuTime::param time)
{
	switch(playerState) {
	case PLAYER_PAUSED:
	case PLAYER_STILL:
		return playingFromSample;
	default:
		return playingFromSample + sampleClock.getTicksTill(time);
	}
}

void LaserdiscPlayer::pause(EmuTime::param time)
{
	if (playerState != PLAYER_STOPPED) {
		updateStream(time);

		if (playerState == PLAYER_PLAYING) {
			playingFromSample = getCurrentSample(time);
		} else if (playerState == PLAYER_MULTISPEED) {
			playingFromSample = (currentFrame - 1ll) * 1001ll *
					video->getSampleRate() / 30000ll;
			sampleClock.advance(time);
		}

		playerState = PLAYER_PAUSED;
		setAck(time, 46);
	}
}

void LaserdiscPlayer::stop(EmuTime::param time)
{
	if (playerState != PLAYER_STOPPED) {
		updateStream(time);
		playerState = PLAYER_STOPPED;
	}
}

void LaserdiscPlayer::eject(EmuTime::param time)
{
	stop(time);
	video.reset();
}

// Step one frame forwards or backwards. The frame will be visible and
// we won't be playing afterwards
void LaserdiscPlayer::stepFrame(bool forwards)
{
	bool needseek = false;

	// Note that on real hardware, the screen goes dark momentarily
	// if you try to step before the first frame or after the last one
	if (playerState == PLAYER_STILL) {
		if (forwards) {
			if (currentFrame < video->getFrames()) {
				currentFrame++;
			}
		} else {
			if (currentFrame > 1) {
				currentFrame--;
				needseek = true;
			}
		}
	}

	playerState = PLAYER_STILL;
	int64_t samplePos = (currentFrame - 1ll) * 1001ll *
	                    video->getSampleRate() / 30000ll;
	playingFromSample = samplePos;

	if (needseek) {
		if (video->getFrameRate() == 60)
			video->seek(currentFrame * 2, samplePos);
		else
			video->seek(currentFrame, samplePos);
	}
}

void LaserdiscPlayer::seekFrame(size_t toframe, EmuTime::param time)
{
	if ((playerState != PLAYER_STOPPED) && video) {
		updateStream(time);

		if (toframe <= 0) toframe = 1;
		if (toframe > video->getFrames()) toframe = video->getFrames();

		// Seek time needs to be emulated correctly since
		// e.g. Astron Belt does not wait for the seek
		// to complete, it simply assumes a certain
		// delay.
		//
		// This calculation is based on measurements on
		// a Pioneer LD-92000.
		auto dist = std::abs(int64_t(toframe) - int64_t(currentFrame));
		int seektime; // time in ms

		if (dist < 1000) {
			seektime = dist + 300;
		} else {
			seektime = 1800 + dist / 12;
		}

		int64_t samplePos = (toframe - 1ll) * 1001ll *
				    video->getSampleRate() / 30000ll;

		if (video->getFrameRate() == 60) {
			video->seek(toframe * 2, samplePos);
		} else {
			video->seek(toframe, samplePos);
		}
		playerState = PLAYER_STILL;
		playingFromSample = samplePos;
		currentFrame = toframe;

		// Seeking clears the frame to wait for
		waitFrame = 0;

		seeking = true;
		setAck(time, seektime);
	}
}

void LaserdiscPlayer::seekChapter(int chapter, EmuTime::param time)
{
	if ((playerState != PLAYER_STOPPED) && video) {
		auto frameno = video->getChapter(chapter);
		if (!frameno) return;
		seekFrame(frameno, time);
	}
}

short LaserdiscPlayer::readSample(EmuTime::param time)
{
	// Here we should return the value of the sample on the
	// right audio channel, ignoring muting (this is done in the MSX)
	// but honouring the stereo mode as this is done in the
	// Laserdisc player
	if (playerState == PLAYER_PLAYING && !seeking) {
		auto sample = getCurrentSample(time);
		if (const AudioFragment* audio = video->getAudio(sample)) {
			++sampleReads;
			int channel = stereoMode == LEFT ? 0 : 1;
			return int(audio->pcm[channel][sample - audio->position]
							 * 32767.f);
		}
	}
	return 0;
}

bool LaserdiscPlayer::isVideoOutputAvailable(EmuTime::param time)
{
	updateStream(time);

	bool videoOut;
	switch (playerState) {
	case PLAYER_PLAYING:
	case PLAYER_MULTISPEED:
	case PLAYER_STILL:
		videoOut = !seeking;
		break;
	default:
		videoOut = false;
		break;
	}
	ldcontrol.videoIn(videoOut);

	return videoOut;
}

void LaserdiscPlayer::preVideoSystemChange()
{
	renderer.reset();
}

void LaserdiscPlayer::postVideoSystemChange()
{
	createRenderer();
}

void LaserdiscPlayer::createRenderer()
{
	Display& display = getMotherBoard().getReactor().getDisplay();
	renderer = RendererFactory::createLDRenderer(*this, display);
}

static std::initializer_list<enum_string<LaserdiscPlayer::RemoteState>> RemoteStateInfo = {
	{ "IDLE",		LaserdiscPlayer::REMOTE_IDLE		},
	{ "HEADER_PULSE",	LaserdiscPlayer::REMOTE_HEADER_PULSE	},
	{ "NEC_HEADER_SPACE",	LaserdiscPlayer::NEC_HEADER_SPACE	},
	{ "NEC_BITS_PULSE",	LaserdiscPlayer::NEC_BITS_PULSE		},
	{ "NEC_BITS_SPACE",	LaserdiscPlayer::NEC_BITS_SPACE		},
};
SERIALIZE_ENUM(LaserdiscPlayer::RemoteState, RemoteStateInfo);

static std::initializer_list<enum_string<LaserdiscPlayer::PlayerState>> PlayerStateInfo = {
	{ "STOPPED",		LaserdiscPlayer::PLAYER_STOPPED		},
	{ "PLAYING",		LaserdiscPlayer::PLAYER_PLAYING		},
	{ "MULTISPEED",		LaserdiscPlayer::PLAYER_MULTISPEED	},
	{ "PAUSED",		LaserdiscPlayer::PLAYER_PAUSED		},
	{ "STILL",		LaserdiscPlayer::PLAYER_STILL		}
};
SERIALIZE_ENUM(LaserdiscPlayer::PlayerState, PlayerStateInfo);

static std::initializer_list<enum_string<LaserdiscPlayer::SeekState>> SeekStateInfo = {
	{ "NONE",		LaserdiscPlayer::SEEK_NONE		},
	{ "CHAPTER",		LaserdiscPlayer::SEEK_CHAPTER		},
	{ "FRAME",		LaserdiscPlayer::SEEK_FRAME		},
	{ "WAIT",		LaserdiscPlayer::SEEK_WAIT		}
};
SERIALIZE_ENUM(LaserdiscPlayer::SeekState, SeekStateInfo);

static std::initializer_list<enum_string<LaserdiscPlayer::StereoMode>> StereoModeInfo = {
	{ "LEFT",		LaserdiscPlayer::LEFT			},
	{ "RIGHT",		LaserdiscPlayer::RIGHT			},
	{ "STEREO",		LaserdiscPlayer::STEREO			}
};
SERIALIZE_ENUM(LaserdiscPlayer::StereoMode, StereoModeInfo);

static std::initializer_list<enum_string<LaserdiscPlayer::RemoteProtocol>> RemoteProtocolInfo = {
	{ "NONE",		LaserdiscPlayer::IR_NONE		},
	{ "NEC",		LaserdiscPlayer::IR_NEC			},
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
	if (remoteState != REMOTE_IDLE) {
		ar.serialize("RemoteBitNr", remoteBitNr);
		ar.serialize("RemoteBits", remoteBits);
		if (ar.versionBelow(version, 3)) {
			assert(ar.isLoader());
			remoteBits = Math::reverseNBits(remoteBits, remoteBitNr);
		}
	}
	ar.serialize("RemoteLastBit", remoteLastBit);
	ar.serialize("RemoteLastEdge", remoteLastEdge);
	ar.serialize("RemoteProtocol", remoteProtocol);
	if (remoteProtocol != IR_NONE) {
		ar.serialize("RemoteCode", remoteCode);
		if (ar.versionBelow(version, 3)) {
			assert(ar.isLoader());
			remoteCode = Math::reverseByte(remoteCode);
		}
		ar.serialize("RemoteExecuteDelayed", remoteExecuteDelayed);
		ar.serialize("RemoteVblanksBack", remoteVblanksBack);
	}

	// Serialize filename
	ar.serialize("OggImage", oggImage);
	if (ar.isLoader()) {
		sampleReads = 0;
		if (!oggImage.empty()) {
			setImageName(oggImage.getResolved(), getCurrentTime());
		} else {
			video.reset();
		}
	}
	ar.serialize("PlayerState", playerState);

	if (playerState != PLAYER_STOPPED) {
		// Serialize seek state
		ar.serialize("SeekState", seekState);
		if (seekState != SEEK_NONE) {
			ar.serialize("SeekNum", seekNum);
		}
		ar.serialize("seeking", seeking);

		// Playing state
		ar.serialize("WaitFrame", waitFrame);

		// This was not yet implemented in openmsx 0.8.1 and earlier
		if (ar.versionAtLeast(version, 2)) {
			ar.serialize("StillOnWaitFrame", stillOnWaitFrame);
		}

		ar.serialize("ACK", ack);
		ar.serialize("PlayingSpeed", playingSpeed);

		// Frame position
		ar.serialize("CurrentFrame", currentFrame);
		if (playerState == PLAYER_MULTISPEED) {
			ar.serialize("FrameStep", frameStep);
		}

		// Audio position
		ar.serialize("StereoMode", stereoMode);
		ar.serialize("FromSample", playingFromSample);
		ar.serialize("SampleClock", sampleClock);

		if (ar.isLoader()) {
			// If the samplerate differs, adjust accordingly
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
		ar.serialize("syncEven", syncEven);
		ar.serialize("syncOdd",  syncOdd);
		ar.serialize("syncAck",  syncAck);
	} else {
		Schedulable::restoreOld(ar, {&syncEven, &syncOdd, &syncAck});
	}

	if (ar.isLoader()) {
		isVideoOutputAvailable(getCurrentTime());
	}
}

INSTANTIATE_SERIALIZE_METHODS(LaserdiscPlayer);

} // namespace openmsx
