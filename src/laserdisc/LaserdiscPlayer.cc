// $Id$

#include "LaserdiscPlayer.hh"
#include "RecordedCommand.hh"
#include "CommandException.hh"
#include "FileContext.hh"
#include "XMLElement.hh"
#include "CassettePort.hh"
#include "CliComm.hh"
#include "Display.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "GlobalSettings.hh"
#include "PioneerLDControl.hh"
#include "OggReader.hh"
#include "LDRenderer.hh"
#include "ThrottleManager.hh"
#include "likely.hh"

using std::auto_ptr;
using std::string;
using std::vector;
using std::set;

namespace openmsx {

// LaserdiscCommand

class LaserdiscCommand : public RecordedCommand
{
public:
	LaserdiscCommand(CommandController& commandController,
			 StateChangeDistributor& stateChangeDistributor,
			 Scheduler& scheduler,
			 LaserdiscPlayer& laserdiscPlayer);
	virtual string execute(const vector<string>& tokens,
			       EmuTime::param time);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	LaserdiscPlayer& laserdiscPlayer;
};

LaserdiscCommand::LaserdiscCommand(
		CommandController& commandController_,
		StateChangeDistributor& stateChangeDistributor,
		Scheduler& scheduler, LaserdiscPlayer& laserdiscPlayer_)
	: RecordedCommand(commandController_, stateChangeDistributor,
			  scheduler, "laserdiscplayer")
	, laserdiscPlayer(laserdiscPlayer_)
{
}

string LaserdiscCommand::execute(const vector<string>& tokens, EmuTime::param time)
{
	string result;
	if (tokens.size() == 2 && tokens[1] == "eject") {
		result += "Ejecting laserdisc.";
		laserdiscPlayer.eject(time);
	} else if (tokens.size() == 3 && tokens[1] == "insert") {
		try {
			result += "Changing laserdisc.";
			laserdiscPlayer.setImageName(tokens[2], time);
		} catch (MSXException& e) {
			throw CommandException(e.getMessage());
		}
	} else {
		throw SyntaxError();
	}
	return result;
}

string LaserdiscCommand::help(const vector<string>& tokens) const
{
	if (tokens.size() >= 2) {
		if (tokens[1] == "insert") {
			return "Inserts the specfied laserdisc image into "
			       "the laserdisc player.";
		} else if (tokens[1] == "eject") {
			return "Eject the laserdisc.";
		}
	}
	return "laserdisc insert <filename> "
	       ": insert a (different) laserdisc image\n"
	       "laserdisc eject             "
	       ": eject the laserdisc\n";
}

void LaserdiscCommand::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		set<string> extra;
		extra.insert("eject");
		extra.insert("insert");
		completeString(tokens, extra);
	} else if (tokens.size() == 3 && tokens[1] == "insert") {
		UserFileContext context;
		completeFileName(getCommandController(), tokens, context);
	}
}

// LaserdiscPlayer

LaserdiscPlayer::LaserdiscPlayer(
		MSXMotherBoard& motherBoard_, PioneerLDControl& ldcontrol_,
		ThrottleManager& throttleManager)
	: SoundDevice(motherBoard_.getMSXMixer(), "laserdiscplayer",
		      "Laserdisc Player", 1, true)
	, Schedulable(motherBoard_.getScheduler())
	, Resample(motherBoard_.getReactor().getGlobalSettings().getResampleSetting())
	, motherBoard(motherBoard_)
	, ldcontrol(ldcontrol_)
	, laserdiscCommand(new LaserdiscCommand(
			   motherBoard_.getCommandController(),
			   motherBoard_.getStateChangeDistributor(),
			   motherBoard_.getScheduler(),
			   *this))
	, sampleClock(EmuTime::zero)
	, start(EmuTime::zero)
	, muteLeft(false)
	, muteRight(false)
	, remoteState(REMOTE_IDLE)
	, remoteLastEdge(EmuTime::zero)
	, remoteLastBit(false)
	, remoteProtocol(IR_NONE)
	, ack(false)
	, foundFrame(false)
	, seeking(false)
	, playerState(PLAYER_STOPPED)
	, loadingIndicator(new LoadingIndicator(throttleManager))
	, sampleReads(0)
{
	static XMLElement laserdiscPlayerConfig("laserdiscplayer");
	static bool init = false;
	if (!init) {
		init = true;
		auto_ptr<XMLElement> sound(new XMLElement("sound"));
		sound->addChild(auto_ptr<XMLElement>(new XMLElement("volume", "30000")));
		laserdiscPlayerConfig.addChild(sound);
	}

	motherBoard.getCassettePort().setLaserdiscPlayer(this);

	Display& display = motherBoard_.getReactor().getDisplay();
	display.attach(*this);

	createRenderer();
	scheduleDisplayStart(Schedulable::getCurrentTime());

	registerSound(laserdiscPlayerConfig);
}

LaserdiscPlayer::~LaserdiscPlayer()
{
	unregisterSound();
	motherBoard.getReactor().getDisplay().detach(*this);
}

void LaserdiscPlayer::scheduleDisplayStart(EmuTime::param time)
{
	Clock<60000, 1001> frameClock(time);
	// The video is 29.97Hz, however we need to do vblank processing
	// at the full 59.94Hz
	setSyncPoint(frameClock + 1, VBLANK);
	setSyncPoint(frameClock + 2, FRAME); // FRAME will execute VBLANK
}

// The protocol used to communicate over the cable for commands to the
// laserdisc player is the NEC infrared protocol. Note that the repeat
// functionality in this protocol is not implemented yet.
//
// My Laserdisc player is an Pioneer LD-92000 which has a remote called
// the CU-CLD037. This is much like the CU-CLD106 which is described
// here: http://lirc.sourceforge.net/remotes/pioneer/CU-CLD106
// The codes and protocol are exactly the same.
//
// For example, the header pulse is 8263 microseconds according to
// lirc. The software in the PBASIC generates a header pulse of
// 64 periods of 7812.5Hz, which is 0.008190s, which is 8190
// microseconds.
void LaserdiscPlayer::extControl(bool bit, EmuTime::param time)
{
	if (remoteLastBit == bit) return;
	remoteLastBit = bit;

	// The tolerance here is not based on actual measurements
	EmuDuration duration = time - remoteLastEdge;
	remoteLastEdge = time;
	unsigned usec = duration.getTicksAt(1000000); // microseconds

//	PRT_DEBUG("LaserdiscPlayer::extControl bit:" << std::dec <<
//		bit << " state:" << remoteState << " usec:" << usec);

	switch (remoteState) {
	case REMOTE_IDLE:
		if (bit) {
			remoteBits = remoteBitNr = 0;
			remoteState = REMOTE_HEADER_PULSE;
		}
		break;
	case REMOTE_HEADER_PULSE:
		if (8000 <= usec && usec < 8400) {
			remoteState = NEC_HEADER_SPACE;
		} else if (140 <= usec && usec < 280) {
			remoteState = LD1100_BITS_SPACE;
		} else {
			remoteState = REMOTE_IDLE;
		}
		break;
	// LD-1100
	case LD1100_BITS_SPACE:
		if (450 <= usec && usec < 550) {
			remoteBits = (remoteBits << 1) | 0;
			remoteState = LD1100_BITS_PULSE;
			++remoteBitNr;
		} else if (1350 <= usec && usec < 1650) {
			remoteBits = (remoteBits << 1) | 1;
			remoteState = LD1100_BITS_PULSE;
			++remoteBitNr;
		} else {
			remoteState = REMOTE_IDLE;
		}
		break;
	case LD1100_SEEN_GAP:
	case LD1100_BITS_PULSE:
		if (225 <= usec && usec < 275) {
			if (remoteBitNr == 30) {
				submitRemote(IR_LD1100, remoteBits);
				remoteState = REMOTE_IDLE;
			} else if ((remoteBitNr == 10 || remoteBitNr == 20) &&
					remoteState != LD1100_SEEN_GAP) {
				remoteState = LD1100_GAP;
			} else {
				remoteState = LD1100_BITS_SPACE;
			}
		} else {
			remoteState = REMOTE_IDLE;
		}
		break;
	case LD1100_GAP:
		if (9000 <= usec && usec < 11000) {
			remoteState = LD1100_SEEN_GAP;
		} else {
			remoteState = REMOTE_IDLE;
		}
		break;
	// NEC protocol
	case NEC_HEADER_SPACE:
		if (3800 <= usec && usec < 4200) {
			remoteState = NEC_BITS_PULSE;
		} else if (2000 <= usec && usec < 2400) {
			remoteState = NEC_REPEAT_PULSE;
		} else {
			remoteState = REMOTE_IDLE;
		}
		break;
	case NEC_BITS_PULSE:
		// Is there a minimum or maximum length for the trailing pulse?
		if (400 <= usec && usec < 700) {
			if (remoteBitNr == 32) {
				byte custom	 = ( remoteBits >> 24) & 0xff;
				byte customCompl = (~remoteBits >> 16) & 0xff;
				byte code	 = ( remoteBits >>  8) & 0xff;
				byte codeCompl	 = (~remoteBits >>  0) & 0xff;
				if (custom == customCompl && 
				    custom == 0x15 &&
				    code == codeCompl) {
					submitRemote(IR_NEC, code);
				}
				remoteState = REMOTE_IDLE;
			} else {
				remoteState = NEC_BITS_SPACE;
			}
		} else {
			remoteState = REMOTE_IDLE;
			break;
		}
		break;
	case NEC_BITS_SPACE:
		if (1400 <= usec && usec < 1600) {
			// bit 1
			remoteBits = (remoteBits << 1) | 1;
			++remoteBitNr;
			remoteState = NEC_BITS_PULSE;
		} else if (400 <= usec && usec < 700) {
			// bit 0
			remoteBits = (remoteBits << 1) | 0;
			++remoteBitNr;
			remoteState = NEC_BITS_PULSE;
		} else {
			// error
			remoteState = REMOTE_IDLE;
		}
		break;
	case NEC_REPEAT_PULSE:
		// We should check that last repeat/button was 110ms ago
		// and succesful.
		PRT_DEBUG("Laserdisc::extControl repeat: " << std::dec << usec);
		if (400 <= usec && usec < 700) {
			buttonRepeat(time);
		}
		remoteState = REMOTE_IDLE;
		break;
	}
}

void LaserdiscPlayer::submitRemote(RemoteProtocol protocol, unsigned code)
{
	if (protocol != remoteProtocol || code != remoteCode) {
		remoteProtocol = protocol;
		remoteCode = code;
		remoteVblanksBack = 0;
		remoteExecuteDelayed = true;
	} else {
		PRT_DEBUG("Laserdisc::remote ignored after " << std::dec 
			  << remoteVblanksBack << " vblanks");
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
	PRT_DEBUG("Laserdisc::Lowering ACK for " << std::dec << wait << "ms");
	removeSyncPoint(ACK);
	Clock<1000> now(time);
	setSyncPoint(now + wait,  ACK);
	ack = true;
	foundFrame = false;
}

bool LaserdiscPlayer::extAck(EmuTime::param /*time*/) const
{
	return ack || foundFrame;
}

bool LaserdiscPlayer::extInt(EmuTime::param /*time*/)
{
	/* FIXME: How exactly is this implemented? */
	return false;
}

void LaserdiscPlayer::buttonRepeat(EmuTime::param /*time*/)
{
	PRT_DEBUG("NEC protocol repeat received");
}

// See:
// http://www.laserdiscarchive.co.uk/laserdisc_archive/pioneer/pioneer_ld-1100/pioneer_ld-1100.htm
//
// Note there are more commands. The LD1100 is compatible with the PR8210. See
// pr8210_command() in the Daphne source code. This seems to be the subset
// needed to support Astron Belt. Also note that Daphne is more relaxed on
// what sort of input it accepts.
//
// To test in P-BASIC:
// CALL SEARCH(1,C,2) -> SEEK CHAPTER 0 0 0 0 2 SEEK
// CALL SEARCH(1,F,12345) -> SEEK FRAME 1 2 3 4 5 SEEK
//
// Astron Belt only searches for frames and omits the FRAME code
void LaserdiscPlayer::remoteButtonLD1100(unsigned code, EmuTime::param time)
{
	if ((code & 0x383) != 0x80 ||
			(code & 0x3ff) != ((code >> 10) & 0x3ff) ||
			(code & 0x3ff) != ((code >> 20) & 0x3ff)) {
		PRT_DEBUG("LD1100 remote: malformed 0x" << std::hex << code);
		return;
	}

	unsigned command = (code >> 2) & 0x1f;

	switch (command) {
	case 0x14: // Play
		PRT_DEBUG("LD1100 remote: play");
		seekState = SEEK_NONE;
		play(time);
		break;
	case 0x0a: // Pause
		PRT_DEBUG("LD1100 remote: pause");
		seekState = SEEK_NONE;
		pause(time);
		break;
	case 0x1e: // Stop
		PRT_DEBUG("LD1100 remote: stop");
		seekState = SEEK_NONE;
		stop(time);
		break;
	case 0x06: // Chapter
		PRT_DEBUG("LD1100 remote: chapter");
		if (seekState == SEEK_FRAME) {
			seekState = SEEK_CHAPTER;
		} else {
			PRT_DEBUG("LD1100: remote: chapter unexpected");
			seekState = SEEK_NONE;
		}
		break;
	case 0x0b: // Frame
		PRT_DEBUG("LD1100 remote: frame");
		if (seekState != SEEK_FRAME) {
			PRT_DEBUG("LD1100 remote: frame unexpected");
			seekState = SEEK_NONE;
		}
		break;
	case 0x1a: // Seek
		PRT_DEBUG("LD1100 remote: seek");
		switch (seekState) {
		case SEEK_NONE:
			seekState = SEEK_FRAME;
			seekNum = 0;
			break;
		case SEEK_FRAME:
			seekState = SEEK_NONE;
			seekFrame(seekNum % 100000, time);
			break;
		case SEEK_CHAPTER:
			seekState = SEEK_NONE;
			seekChapter(seekNum % 100, time);
			break;
		default:
			break;
		}
		break;
	case 0x01:
		PRT_DEBUG("LD1100 remote: 0");
		seekNum = seekNum * 10 + 0;
		break;
	case 0x11:
		PRT_DEBUG("LD1100 remote: 1");
		seekNum = seekNum * 10 + 1;
		break;
	case 0x09:
		PRT_DEBUG("LD1100 remote: 2");
		seekNum = seekNum * 10 + 2;
		break;
	case 0x19:
		PRT_DEBUG("LD1100 remote: 3");
		seekNum = seekNum * 10 + 3;
		break;
	case 0x05:
		PRT_DEBUG("LD1100 remote: 4");
		seekNum = seekNum * 10 + 4;
		break;
	case 0x15:
		PRT_DEBUG("LD1100 remote: 5");
		seekNum = seekNum * 10 + 5;
		break;
	case 0x0d:
		PRT_DEBUG("LD1100 remote: 6");
		seekNum = seekNum * 10 + 6;
		break;
	case 0x1d:
		PRT_DEBUG("LD1100 remote: 7");
		seekNum = seekNum * 10 + 7;
		break;
	case 0x03:
		PRT_DEBUG("LD1100 remote: 8");
		seekNum = seekNum * 10 + 8;
		break;
	case 0x13:
		PRT_DEBUG("LD1100 remote: 9");
		seekNum = seekNum * 10 + 9;
		break;
	default:
		PRT_DEBUG("LD1100 remote: unknown 0x" << std::hex << command);
		break;
	}
}

void LaserdiscPlayer::remoteButtonNEC(unsigned code, EmuTime::param time)
{
#ifdef DEBUG
	string f;
	switch (code) {
	case 0xe2: f = "C+"; break;	// Increase playing speed
	case 0x62: f = "C-"; break;	// Decrease playing speed
	case 0xc2: f = "D+"; break;	// Show Frame# & Chapter# OSD
	case 0xd2: f = "L+"; break;	// right
	case 0x92: f = "L-"; break;	// left
	case 0x52: f = "L@"; break;	// stereo
	case 0x1a: f = "M+"; break;	// multi speed forwards
	case 0xaa: f = "M-"; break;	// multi speed backwards
	case 0xe8: f = "P+"; break;	// play
	case 0x68: f = "P@"; break;	// stop
	case 0x18: f = "P/"; break;	// pause
	case 0x2a: f = "S+"; break;	// frame step forward
	case 0x0a: f = "S-"; break;	// frame step backwards
	case 0xa2: f = "X+"; break;	// clear
	case 0x82: f = "F"; break;	// seek frame
	case 0x02: f = "C"; break;	// seek chapter
	case 0x42: f = "END"; break;	// done seek frame/chapter
	case 0x00: f = "0"; break;
	case 0x80: f = "1"; break;
	case 0x40: f = "2"; break;
	case 0xc0: f = "3"; break;
	case 0x20: f = "4"; break;
	case 0xa0: f = "5"; break;
	case 0x60: f = "6"; break;
	case 0xe0: f = "7"; break;
	case 0x10: f = "8"; break;
	case 0x90: f = "9"; break;
	case 0xfa: f = "WAIT FRAME"; break;

	case 0xca: // previous chapter
	case 0x4a: // next chapter
	default: break;
	}

	if (!f.empty()) {
		PRT_DEBUG("PioneerLD7000::remote " << f);
	} else {
		PRT_DEBUG("PioneerLD7000::remote unknown " << std::hex << code);
	}
#endif
	// When not playing, only the play button works
	if (playerState == PLAYER_STOPPED) {
		if (code == 0xe8) {
			// P+
			play(time);
		}
	} else {
		// FIXME: while seeking, only a small subset of buttons work
		bool nonseekack = true;

		switch (code) {
		case 0xd2: // L+ (both channels play the right channel)
			updateStream(time);
			stereoMode = RIGHT;
			break;
		case 0x92: // L- (both channels play the left channel)
			updateStream(time);
			stereoMode = LEFT;
			break;
		case 0x52: // L@ (normal stereo)
			updateStream(time);
			stereoMode = STEREO;
			break;
		case 0xfa:
			seekState = SEEK_WAIT;
			seekNum = 0;
			nonseekack = false;
			break;
		case 0x82:
			seekState = SEEK_FRAME;
			seekNum = 0;
			break;
		case 0x02:
			seekState = SEEK_CHAPTER;
			seekNum = 0;
			nonseekack = video->chapter(0) != 0;
			break;
		case 0x00: seekNum = seekNum * 10 + 0; break;
		case 0x80: seekNum = seekNum * 10 + 1; break;
		case 0x40: seekNum = seekNum * 10 + 2; break;
		case 0xc0: seekNum = seekNum * 10 + 3; break;
		case 0x20: seekNum = seekNum * 10 + 4; break;
		case 0xa0: seekNum = seekNum * 10 + 5; break;
		case 0x60: seekNum = seekNum * 10 + 6; break;
		case 0xe0: seekNum = seekNum * 10 + 7; break;
		case 0x10: seekNum = seekNum * 10 + 8; break;
		case 0x90: seekNum = seekNum * 10 + 9; break;
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
				PRT_DEBUG("Wait frame set to " << std::dec <<
								waitFrame);
				break;
			default:
				seekState = SEEK_NONE;
				nonseekack = false;
				break;
			}
			break;
		case 0xa2: // Clear "X+"
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
		case 0xe8: // P+
			play(time);
			nonseekack = false;
			break;
		case 0x68: // P@ (stop/eject)
			stop(time);
			nonseekack = false;
			break;
		case 0xff:
			nonseekack = false;
			seekState = SEEK_NONE;
			break;
		case 0x2a: // S+ (frame step forward)
			stepFrame(true);
			break;
		case 0x0a: // S- (frame step backwards)
			stepFrame(false);
			break;
		case 0xaa: // M- (multispeed backwards)
			// Not supported
			motherBoard.getMSXCliComm().printWarning(
				"The Laserdisc player received a command to "
				"play backwards (M-). This is currently not "
				"supported.");
			nonseekack = false;
			break;
		case 0x1a: // M+ (multispeed forwards)
			playerState = PLAYER_PLAYING_MULTISPEED;
			setFrameStep();
			break;
		case 0x62: // C- (play slower)
			if (playingSpeed >= SPEED_STEP1) {
				playingSpeed--;
				frameStep = 1;	// FIXME: is this correct?
			}
			break;
		case 0xe2: // C+ (play faster)
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

const string& LaserdiscPlayer::schedName() const
{
	static const string name("laserdiscplayer");
	return name;
}

void LaserdiscPlayer::executeUntil(EmuTime::param time, int userdata)
{
	updateStream(time);

	switch (userdata) {
	case ACK: 
		if (seeking && playerState == PLAYER_PLAYING) {
			sampleClock.advance(time);
		}

		if (seeking) {
			PRT_DEBUG("Laserdisc: seek complete");
		}

		ack = false;
		seeking = false;
		PRT_DEBUG("Laserdisc: ACK cleared");
		break;
	case FRAME:
		if (RawFrame* rawFrame = renderer->getRawFrame()) {
			renderer->frameStart(time);

			if (isVideoOutputAvailable(time)) {
				video->getFrame(*rawFrame, currentFrame);

				nextFrame(time);
			} else {
				renderer->drawBlank(0, 128, 196);
			}
			renderer->frameEnd();
		}

		// Update throttling
		loadingIndicator->update(seeking || sampleReads > 500);
		sampleReads = 0;

		scheduleDisplayStart(time);
		// fall-through
	case VBLANK:
		// Processing of the remote control happens at each frame
		// (even and odd, so at 59.94Hz)
		if (remoteProtocol == IR_NEC) {
			if (remoteExecuteDelayed) {
				remoteButtonNEC(remoteCode, time);
			}

			if (++remoteVblanksBack > 6) {
				remoteProtocol = IR_NONE;
			}
		} else if (remoteProtocol == IR_LD1100) {
			if (remoteExecuteDelayed) {
				remoteButtonLD1100(remoteCode, time);
			}
			remoteProtocol = IR_NONE;
		}
		remoteExecuteDelayed = false;
	}
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
		PRT_DEBUG("LaserdiscPlayer: wait frame " << std::dec <<
						waitFrame << " reached");

		waitFrame = 0;
		foundFrame = true;
	}

	if (playerState == PLAYER_PLAYING_MULTISPEED) {
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
	if ((playerState == PLAYER_PLAYING || 
	     playerState == PLAYER_PLAYING_MULTISPEED) && 
	     video->stopFrame(currentFrame)) {
		PRT_DEBUG("LaserdiscPlayer: stopFrame " << std::dec <<
						currentFrame << " reached");

		playingFromSample = getCurrentSample(time);
		playerState = PLAYER_STILL;
	}
}

void LaserdiscPlayer::setImageName(const string& newImage, EmuTime::param time)
{
	stop(time);
	oggImage = Filename(newImage, motherBoard.getCommandController());
	video.reset(new OggReader(oggImage, motherBoard.getMSXCliComm()));
	sampleClock.setFreq(video->getSampleRate());
	setOutputRate(outputRate);
}

void LaserdiscPlayer::setOutputRate(unsigned newOutputRate)
{
	outputRate = newOutputRate;
	unsigned inputRate = video.get() ? video->getSampleRate() : outputRate;
	setInputRate(inputRate);
	setResampleRatio(inputRate, outputRate, isStereo());
}

void LaserdiscPlayer::generateChannels(int** buffers, unsigned num)
{
	if (playerState != PLAYER_PLAYING || seeking ||
						(muteLeft && muteRight)) {
		buffers[0] = 0;
		return;
	}

	unsigned pos = 0, len, currentSample;

	if (unlikely(!sampleClock.before(start))) {
		// Before playing of sounds begins
		EmuDuration duration = sampleClock.getTime() - start;
		len = duration.getTicksAt(video->getSampleRate());
		if (len >= num) {
			buffers[0] = 0;
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
		PRT_DEBUG("Laserdisc audio drift: " << std::dec <<
				lastPlayedSample << " " << currentSample);
		lastPlayedSample = currentSample;
	}

	int left = stereoMode == RIGHT ? 1 : 0;
	int right = stereoMode == LEFT ? 0 : 1;

	while (pos < num) {
		const AudioFragment* audio = video->getAudio(lastPlayedSample);

		if (!audio) {
			if (pos == 0) {
				buffers[0] = 0;
				break;
			} else for (/**/; pos < num; ++pos) {
				buffers[0][pos * 2 + 0] = 0;
				buffers[0][pos * 2 + 1] = 0;
			}
		} else {
			unsigned offset = lastPlayedSample - audio->position;
			len = std::min(audio->length - offset, num - pos);

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

bool LaserdiscPlayer::generateInput(int* buffer, unsigned num)
{
	return mixChannels(buffer, num);
}

bool LaserdiscPlayer::updateBuffer(unsigned length, int *buffer,
		EmuTime::param start_, EmuDuration::param /*sampDur*/)
{
	start = start_;
	return generateOutput(buffer, length);
}

void LaserdiscPlayer::setMuting(bool left, bool right, EmuTime::param time)
{
	updateStream(time);
	PRT_DEBUG("Laserdisc::setMuting L:" << (left  ? "on" : "off")
				   << " R:" << (right ? "on" : "off"));
	muteLeft = left;
	muteRight = right;
}

void LaserdiscPlayer::play(EmuTime::param time)
{
	PRT_DEBUG("Laserdisc::Play");

	if (video.get()) {
		updateStream(time);

		if (seeking) {
			// Do not ACK
			PRT_DEBUG("play while seeking");
		} else if (playerState == PLAYER_STOPPED) {
			// Disk needs to spin up, which takes 9.6s on
			// my Pioneer LD-92000. Also always seek to
			// beginning (confirmed on real MSX and LD)
			video->seek(1, 0);
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
		} else if (playerState == PLAYER_PLAYING_MULTISPEED) {
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

unsigned LaserdiscPlayer::getCurrentSample(EmuTime::param time)
{
	return sampleClock.getTicksTill(time) + playingFromSample;
}

void LaserdiscPlayer::pause(EmuTime::param time)
{
	if (playerState != PLAYER_STOPPED) {
		PRT_DEBUG("Laserdisc::Pause");

		updateStream(time);

		if (playerState == PLAYER_PLAYING) {
			playingFromSample = getCurrentSample(time);
		} else if (playerState == PLAYER_PLAYING_MULTISPEED) {
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
		PRT_DEBUG("Laserdisc::Stop");

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

	if (playerState == PLAYER_STILL) {
		if (forwards) {
			currentFrame++;
		}
		else if (currentFrame > 1) {
			currentFrame--;
			needseek = true;
		}
	}

	playerState = PLAYER_STILL;
	long long samplePos = (currentFrame - 1ll) * 1001ll *
			video->getSampleRate() / 30000ll;
	playingFromSample = samplePos;

	if (needseek) {
		video->seek(currentFrame, samplePos);
	}
}

void LaserdiscPlayer::seekFrame(int toframe, EmuTime::param time)
{
	if (playerState != PLAYER_STOPPED) {
		PRT_DEBUG("Laserdisc::SeekFrame " << std::dec << toframe);

		if (seeking) {
			PRT_DEBUG("FIXME: seek command while still seeking");
		}

		if (video.get()) {
			updateStream(time);

			if (toframe <= 0)  {
				toframe = 1;
			}
			// FIXME: check if seeking beyond end

			// Seek time needs to be emulated correctly since
			// e.g. Astron Belt does not wait for the seek
			// to complete, it simply assumes a certain
			// delay.
			//
			// This calculation is based on measurements on
			// a Pioneer LD-92000.
			int dist = abs(toframe - currentFrame);
			int seektime; // time in ms

			if (dist < 1000) {
				seektime = dist + 300;
			} else {
				seektime = 1800 + dist / 12;
			}

			long long samplePos = (toframe - 1ll) * 1001ll *
					video->getSampleRate() / 30000ll;

			video->seek(toframe, samplePos);

			playerState = PLAYER_STILL;
			playingFromSample = samplePos;
			currentFrame = toframe;

			// Seeking clears the frame to wait for
			waitFrame = 0;

			seeking = true;
			setAck(time, seektime);
		}
	}
}

void LaserdiscPlayer::seekChapter(int chapter, EmuTime::param time)
{
	if (playerState != PLAYER_STOPPED) {
		if (video.get()) {
			int frameno = video->chapter(chapter);
			if (!frameno) {
				return;
			}
			seekFrame(frameno, time);
		}
	}
}

short LaserdiscPlayer::readSample(EmuTime::param time)
{
	// Here we should return the value of the sample on the
	// right audio channel, ignoring muting (this is done in the MSX) 
	// but honouring the stereo mode as this is done in the 
	// Laserdisc player
	if (playerState == PLAYER_PLAYING && !seeking) {
		unsigned sample = getCurrentSample(time);
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
	case PLAYER_PLAYING_MULTISPEED:
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
	renderer.reset(RendererFactory::createLDRenderer(*this, display));
}

static enum_string<LaserdiscPlayer::RemoteState> RemoteStateInfo[] = {
	{ "IDLE",		LaserdiscPlayer::REMOTE_IDLE		},
	{ "HEADER_PULSE",	LaserdiscPlayer::REMOTE_HEADER_PULSE	},
	{ "NEC_HEADER_SPACE",	LaserdiscPlayer::NEC_HEADER_SPACE	},
	{ "NEC_BITS_PULSE",	LaserdiscPlayer::NEC_BITS_PULSE		},
	{ "NEC_BITS_SPACE",	LaserdiscPlayer::NEC_BITS_SPACE		},
	{ "NEC_REPEAT_PULSE",	LaserdiscPlayer::NEC_REPEAT_PULSE	},
	{ "LD1100_GAP",		LaserdiscPlayer::LD1100_GAP		},
	{ "LD1100_SEEN_GAP",	LaserdiscPlayer::LD1100_SEEN_GAP	},
	{ "LD1100_BITS_SPACE",	LaserdiscPlayer::LD1100_BITS_SPACE	},
	{ "LD1100_BITS_PULSE",	LaserdiscPlayer::LD1100_BITS_PULSE	}
};
SERIALIZE_ENUM(LaserdiscPlayer::RemoteState, RemoteStateInfo);

static enum_string<LaserdiscPlayer::PlayerState> PlayerStateInfo[] = {
	{ "STOPPED",		LaserdiscPlayer::PLAYER_STOPPED		},		{ "PLAYING",		LaserdiscPlayer::PLAYER_PLAYING		},
	{ "PLAYING_MULTISPEED",	LaserdiscPlayer::PLAYER_PLAYING_MULTISPEED },
	{ "PAUSED",		LaserdiscPlayer::PLAYER_PAUSED		},
	{ "STILL",		LaserdiscPlayer::PLAYER_STILL		}
};
SERIALIZE_ENUM(LaserdiscPlayer::PlayerState, PlayerStateInfo);

static enum_string<LaserdiscPlayer::SeekState> SeekStateInfo[] = {
	{ "NONE",		LaserdiscPlayer::SEEK_NONE		},
	{ "CHAPTER",		LaserdiscPlayer::SEEK_CHAPTER		},
	{ "FRAME",		LaserdiscPlayer::SEEK_FRAME		},
	{ "WAIT",		LaserdiscPlayer::SEEK_WAIT		}
};
SERIALIZE_ENUM(LaserdiscPlayer::SeekState, SeekStateInfo);

static enum_string<LaserdiscPlayer::StereoMode> StereoModeInfo[] = {
	{ "LEFT",		LaserdiscPlayer::LEFT			},
	{ "RIGHT",		LaserdiscPlayer::RIGHT			},
	{ "STEREO",		LaserdiscPlayer::STEREO			}
};
SERIALIZE_ENUM(LaserdiscPlayer::StereoMode, StereoModeInfo);

static enum_string<LaserdiscPlayer::RemoteProtocol> RemoteProtocolInfo[] = {
	{ "NONE",		LaserdiscPlayer::IR_NONE		},
	{ "NEC",		LaserdiscPlayer::IR_NEC			},
	{ "LD1100",		LaserdiscPlayer::IR_LD1100		}
};
SERIALIZE_ENUM(LaserdiscPlayer::RemoteProtocol, RemoteProtocolInfo);

template<typename Archive>
void LaserdiscPlayer::serialize(Archive& ar, unsigned /*version*/)
{
	// Serialize remote control
	ar.serialize("RemoteState", remoteState);
	if (remoteState != REMOTE_IDLE) { 
		ar.serialize("RemoteBitNr", remoteBitNr);
		ar.serialize("RemoteBits", remoteBits);
	}
	ar.serialize("RemoteLastBit", remoteLastBit);
	ar.serialize("RemoteLastEdge", remoteLastEdge);
	ar.serialize("RemoteProtocol", remoteProtocol);
	if (remoteProtocol != IR_NONE) {
		ar.serialize("RemoteCode", remoteCode);
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
		ar.serialize("ACK", ack);
		ar.serialize("FoundFrame", foundFrame);
		ar.serialize("PlayingSpeed", playingSpeed);

		// Frame position
		ar.serialize("CurrentFrame", currentFrame);
		if (playerState == PLAYER_PLAYING_MULTISPEED) {
			ar.serialize("FrameStep", frameStep);
		}

		// Audio position
		ar.serialize("StereoMode", stereoMode);
		ar.serialize("FromSample", playingFromSample);
		ar.serialize("SampleClock", sampleClock);

		if (ar.isLoader()) {
			// If the samplerate differs, adjust accordingly
			if (video->getSampleRate() != sampleClock.getFreq()) {
				uint64 pos = playingFromSample;

				pos *= video->getSampleRate();
				pos /= sampleClock.getFreq();

				playingFromSample = pos;
				sampleClock.setFreq(video->getSampleRate());
			}

			unsigned sample = getCurrentSample(getCurrentTime());
			video->seek(currentFrame, sample);
			lastPlayedSample = sample;
		}
	}

	ar.template serializeBase<Schedulable>(*this);

	if (ar.isLoader()) {
		isVideoOutputAvailable(getCurrentTime());
	}
}

INSTANTIATE_SERIALIZE_METHODS(LaserdiscPlayer);

} // namespace openmsx
