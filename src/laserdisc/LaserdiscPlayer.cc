// $Id$

#include "LaserdiscPlayer.hh"
#include "RecordedCommand.hh"
#include "CommandException.hh"
#include "FileContext.hh"
#include "XMLElement.hh"
#include "CassettePort.hh"
#include "Display.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "PioneerLDControl.hh"

using std::auto_ptr;
using std::string;
using std::vector;
using std::set;

namespace openmsx {

class LaserdiscCommand : public RecordedCommand
{
public:
	LaserdiscCommand(CommandController& commandController,
			MSXEventDistributor& msxEventDistributor,
			Scheduler& scheduler, 
			LaserdiscPlayer& laserdiscPlayer);
	virtual string execute(const vector<string>& tokens, 
			EmuTime::param time);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	LaserdiscPlayer& laserdiscPlayer;
};

LaserdiscCommand::LaserdiscCommand(CommandController& commandController_,
			MSXEventDistributor& msxEventDistributor,
			Scheduler& scheduler, LaserdiscPlayer& laserdiscPlayer_)
	: RecordedCommand(commandController_, msxEventDistributor,
			  scheduler, "laserdiscplayer")
	, laserdiscPlayer(laserdiscPlayer_ )
{
}

string LaserdiscCommand::execute(const vector<string>& tokens, EmuTime::param time)
{
	string result;

	if (tokens.size() == 3 && tokens[1] == "insert") {
		try {
			result += "Changing laserdisc";
			Filename filename(tokens[2], getCommandController());
			laserdiscPlayer.setImageName(filename, time);
		} catch (MSXException& e) {
			throw CommandException(e.getMessage());
		}
	} else if (tokens.size() == 3 && tokens[1] == "mute") {
		if (tokens[2] == "off") {
			laserdiscPlayer.setMuting(false, false, time);
			result += "Laserdisc muting off.";
		} else if (tokens[2] == "left")  {
			laserdiscPlayer.setMuting(true, false, time);
			result += "Laserdisc muting left on, right off.";
		} else if (tokens[2] == "right")  { 
			laserdiscPlayer.setMuting(false, true, time);
			result += "Laserdisc muting left off, and right on.";
		} else if (tokens[2] == "both")  { 
			laserdiscPlayer.setMuting(true, true, time);
			result += "Laserdisc muting both left and right.";
		} else {
			throw SyntaxError();
		}
	} else {
		throw SyntaxError();
	}

	return result;
}

string LaserdiscCommand::help(const vector<string>& tokens) const
{
	string helptext;

	if (tokens.size() >= 2) {
		if (tokens[1] == "insert") {
			helptext =
				"Inserts the specfied laserdisc image into "
				"the laserdisc player.";
		} else if (tokens[1] == "mute") {
			helptext =
				"Setting this to 'off' makes both left and "
				"right audio channels audible. 'left' mutes "
				"the left channels. 'right' mutes the left "
				"audio channel. 'both' mutes both channels.";
		}
	} else {
		helptext = 
			"laserdisc insert <filename> "
			": insert a (different) laserdisc image\n"
			"laserdisc mute              "
			": set muting of laserdisc audio\n";
	}

	return helptext;
}

void LaserdiscCommand::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		set<string> extra;
		extra.insert("insert");
		extra.insert("mute");
		completeString(tokens, extra);
	} else if (tokens.size() == 3 && tokens[1] == "insert") {
		UserFileContext context;
		completeFileName(getCommandController(), tokens, context);
	} else if (tokens.size() == 3 && tokens[1] == "mute") {
		set<string> extra;
		extra.insert("both");
		extra.insert("left");
		extra.insert("right");
		extra.insert("off");
		completeString(tokens, extra);
	}
}

LaserdiscPlayer::LaserdiscPlayer(MSXMotherBoard& motherBoard_, PioneerLDControl& ldcontrol_)
	: SoundDevice(motherBoard_.getMSXMixer(), getName(), getDescription(), 1, true)
	, Schedulable(motherBoard_.getScheduler())
	, Resample(motherBoard_.getReactor().getGlobalSettings(), 2)
	, laserdiscCommand(new LaserdiscCommand(
			motherBoard_.getCommandController(),
			motherBoard_.getMSXEventDistributor(),
			motherBoard_.getScheduler(),
			*this))
	, motherBoard(motherBoard_)
	, ldcontrol(ldcontrol_)
	, sampleClock(EmuTime::zero)
	, frameClock(EmuTime::zero)
	, remote_state(REMOTE_GAP)
	, remote_last_bit(false)
	, remote_last_edge(EmuTime::zero)
	, ack(false)
	, seeking(false)
	, player_state(PLAYER_STOPPED)
{
	motherBoard_.getCassettePort().setLaserdiscPlayer(this);
	static XMLElement laserdiscPlayerConfig("laserdiscplayer");
	static bool init = false;
	if (!init) {
		init = true;
		auto_ptr<XMLElement> sound(new XMLElement("sound"));
		sound->addChild(auto_ptr<XMLElement>(new XMLElement("volume", "20000")));
		laserdiscPlayerConfig.addChild(sound);
	}

	Display& display = motherBoard_.getReactor().getDisplay();

	display.attach(*this);
	
	createRenderer();

	setSyncPoint(frameClock + 1, FRAME);
	registerSound(laserdiscPlayerConfig);
}

LaserdiscPlayer::~LaserdiscPlayer()
{
	unregisterSound();
	motherBoard.getReactor().getDisplay().detach(*this);
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
void LaserdiscPlayer::extcontrol(bool bit, EmuTime::param time)
{
	if (remote_last_bit == bit)
		return;

	// The tolerance we use here is not based on actual measurements;
	// in fact I have no idea if the algorithm is correct, maybe the
	// lirc source will reveal such information. Either way, this
	// works with existing software (hopefully).
	EmuDuration duration = time - remote_last_edge;
	unsigned usec = duration.getTicksAt(1000000); // microseconds

	switch (remote_state) {
	case REMOTE_GAP:
		// Is there a minimum length of a gap?
		if (bit) {
			remote_bits = remote_bitno = 0;
			remote_state = REMOTE_HEADER_PULSE;
		}
		break;
	case REMOTE_HEADER_PULSE:
		if (usec >= 8000 && usec < 8400)
			remote_state = REMOTE_HEADER_SPACE;
		else
			remote_state = REMOTE_GAP;
		break;
	case REMOTE_HEADER_SPACE:
		if (usec >= 3800 && usec < 4200)
			remote_state = REMOTE_BITS_PULSE;
		else if (usec >= 2000 && usec < 2400)
			remote_state = REMOTE_REPEAT_PULSE;
		else
			remote_state = REMOTE_GAP;
		break;
	case REMOTE_BITS_PULSE:
		// Is there a minimum or maximum length for the trailing pulse?
		if (usec < 400 ||  usec >= 700) {
			remote_state = REMOTE_GAP;
			break;
		}

		if (remote_bitno == 32) {
			byte custom = (remote_bits >> 24) & 0xff;
			byte custom_compl = (~remote_bits >> 16) & 0xff;
			byte code = (remote_bits >> 8) & 0xff;
			byte code_compl = ~remote_bits & 0xff;

			if (custom == custom_compl && code == code_compl)
				button(custom, code, time);

			remote_state = REMOTE_GAP;
			break;
		}

		remote_state = REMOTE_BITS_SPACE;
		break;
	case REMOTE_BITS_SPACE:
		remote_bits <<= 1;
		if (usec >= 1400 && usec < 1600) {
			// bit set
			remote_bits |= 1;
		} else if (usec < 400 || usec >= 700) {
			// not a bit unset, error
			remote_state = REMOTE_GAP;
			break;
		}

		remote_bitno++;
		remote_state = REMOTE_BITS_PULSE;
		break;
	case REMOTE_REPEAT_PULSE:
		// We should check that last repeat/button was 110ms ago
		// and succesful.
		if (usec < 400 ||  usec >= 700) {
			remote_state = REMOTE_GAP;
			break;
		}

		button_repeat(time);

		remote_state = REMOTE_GAP;
		break;
	}

	remote_last_edge = time;
	remote_last_bit = bit;
}

void LaserdiscPlayer::setack(EmuTime::param time, int wait)
{
	removeSyncPoint(ACK);
	Clock<1000> now(time);
	setSyncPoint(now + wait,  ACK);
	ack = true;
}

bool LaserdiscPlayer::extack(EmuTime::param /*time*/)
{
	return ack;
}

bool LaserdiscPlayer::extint(EmuTime::param /*time*/)
{
	/* FIXME: How exactly is this implemented? */
	return false;
}

void LaserdiscPlayer::button_repeat(EmuTime::param /*time*/)
{
	PRT_DEBUG("NEC protocol repeat received");
}

void LaserdiscPlayer::button(unsigned custom, unsigned code, EmuTime::param time)
{
	if (custom != 0x15) {
		return;
	}

#ifdef DEBUG
	string f = "";

	switch (code) {
	case 0xe2: f = "C+"; break;
	case 0x62: f = "C-"; break;
	case 0xc2: f = "D+"; break;	// Show Frame# & Chapter# OSD
	case 0xd2: f = "L+"; break;
	case 0x92: f = "L-"; break;
	case 0x52: f = "L@"; break;
	case 0x1a: f = "M+"; break;	// half speed forwards
	case 0xaa: f = "M-"; break;	// half speed backwards
	case 0xe8: f = "P+"; break;	// play
	case 0x68: f = "P@"; break;	// stop
	case 0x18: f = "P/"; break;	// pause
	case 0x2a: f = "S+"; break;
	case 0x0a: f = "S-"; break;
	case 0xa2: f = "X+"; break;
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

	case 0xca: // previous chapter
	case 0x4a: // next chapter
	default: break;
	}

	if (f != "") {
		std::cout << "PioneerLD7000::remote " << f << std::endl;
	} else {
		std::cout << "PioneerLD7000::remote unknown " << 
				std::hex << code << std::endl;
	}
#endif

	// deal with seeking.
	if (player_state != PLAYER_STOPPED) {
		bool ok = true;

		switch (code) {
		case 0x82:
			seek_state = SEEK_FRAME_BEGIN;
			seek_no = 0;
			break;
		case 0x02:
			seek_state = SEEK_CHAPTER_BEGIN;
			seek_no = 0;
			ok = false; // FIXME: no chapter information yet
			break;
		case 0x00: seek_no = seek_no * 10 + 0; break;
		case 0x80: seek_no = seek_no * 10 + 1; break;
		case 0x40: seek_no = seek_no * 10 + 2; break;
		case 0xc0: seek_no = seek_no * 10 + 3; break;
		case 0x20: seek_no = seek_no * 10 + 4; break;
		case 0xa0: seek_no = seek_no * 10 + 5; break;
		case 0x60: seek_no = seek_no * 10 + 6; break;
		case 0xe0: seek_no = seek_no * 10 + 7; break;
		case 0x10: seek_no = seek_no * 10 + 8; break;
		case 0x90: seek_no = seek_no * 10 + 9; break;
		case 0x42:
			ok = false;
			switch (seek_state) {
			case SEEK_FRAME_BEGIN:
				seek_state = SEEK_FRAME_END;
				break;
			case SEEK_FRAME_END:
				seek_state = SEEK_NONE;
				seekFrame(seek_no % 100000, time);
				break;
			case SEEK_CHAPTER_BEGIN:
				seek_state = SEEK_CHAPTER_END;
				break;
			case SEEK_CHAPTER_END:
				seek_state = SEEK_NONE;
				seekChapter(seek_no % 100, time);
				break;
			default:
				seek_state = SEEK_NONE;
			}
			break;
		case 0xff:
		default:
			ok = false;
			seek_state = SEEK_NONE;
			break;
		}

		if (ok) {
			// seeking will take much more than this!
			setack(time, 46);
		}
	}

	switch (code) {
	case 0x18: /* P/ */
		pause(time);
		break;
	case 0xe8: /* P+ */
		play(time);
		break;
	case 0x68: /* P@ (stop/eject) */
		stop(time);
		break;
	}
}

const string& LaserdiscPlayer::getName() const
{
	static const string name("laserdiscplayer");
	return name;
}

const string& LaserdiscPlayer::schedName() const
{
	return getName();
}

const string& LaserdiscPlayer::getDescription() const
{
	static const string name("Laserdisc Player");
	return name;
}

void LaserdiscPlayer::executeUntil(openmsx::EmuTime time, int userdata)
{
	updateStream(time);

	if (userdata == ACK) {
		if (seeking) {
			PRT_DEBUG("Laserdisc: seek complete");
		}
		ack = false;
		seeking = false;

		if (player_state == PLAYER_PLAYING)
			sampleClock.reset(time);
	} else if (userdata == FRAME) {
		renderer->frameStart(time);

		if (isVideoOutputAvailable(time)) {
			renderer->drawBitmap(video->getFrame());
		} else {
			renderer->drawBlank(0, 128, 196);
		}

		renderer->frameEnd(time);

		frameClock.reset(time);

		setSyncPoint(frameClock + 1, FRAME);
	}
}

void LaserdiscPlayer::setImageName(const Filename& newImage, EmuTime::param /*time*/)
{
	video.reset(new OggReader(newImage));
	filename = newImage;
	sampleClock.setFreq(video->getSampleRate());
	setOutputRate(outputRate);
}

const Filename& LaserdiscPlayer::getImageName() const
{
	return filename;
}

void LaserdiscPlayer::setOutputRate(unsigned newOutputRate)
{
	outputRate = newOutputRate;
	unsigned inputRate = video.get() ? video->getSampleRate() : outputRate;
	setInputRate(inputRate);
	setResampleRatio(inputRate, outputRate);
}

void LaserdiscPlayer::generateChannels(int **buffers, unsigned num)
{
	// If both channels are muted, we could still be loading from
	// tape. Therefore sound has to be generated and then discarded.
	// This also ensures that we maintain the right position when
	// both channels are muted.
	if (player_state != PLAYER_PLAYING || seeking) {
		buffers[0] = 0;
		return;
	}

	for (unsigned pos=0; pos<num; ) {
		float **pcm;
		unsigned rc, i;

		// We are using floats here since otherwise we have to
		// convert shorts to ints. libvorbisfile converts floats to
		// shorts when we do not request floats leaving another
		// conversion step up us.
		rc = video->fillFloatBuffer(&pcm, num - pos);
		if (rc == 0) {
			// we've fallen of the end of the file. We 
			// should raise an IRQ now.
			if (pos == 0)
				buffers[0] = 0;
			else for (; pos<num; pos++) {
				buffers[0][pos*2+0] = 0;
				buffers[0][pos*2+1] = 0;
			}
			player_state = PLAYER_STOPPED;
		} else {
			// maybe muting should be moved out of the loop?
			for (i=0; i<rc; i++, pos++) {
				buffers[0][pos*2+0] = mute_left ? 0 : 
							pcm[0][i] * 65536.f;
				buffers[0][pos*2+1] = mute_right ? 0 : 
							pcm[1][i] * 65536.f;
			}
			tapein = pcm[1][rc-1] * 32767.f;
		}
	}
}

bool LaserdiscPlayer::generateInput(int* buffer, unsigned num)
{
	return mixChannels(buffer, num);
}

bool LaserdiscPlayer::updateBuffer(unsigned length, int *buffer,
		EmuTime::param /*time*/, EmuDuration::param /*sampDur*/)
{
	return generateOutput(buffer, length);
}

void LaserdiscPlayer::setMuting(bool left, bool right, EmuTime::param time)
{
	updateStream(time);

	PRT_DEBUG("Laserdisc::setMuting L:" << 
				(left ? "on  R:" : "off R:") <<  
				(right ? "on" : "off"));
	mute_left = left;
	mute_right = right;
}

void LaserdiscPlayer::play(EmuTime::param time)
{
	PRT_DEBUG("Laserdisc::Play");

	if (video.get()) {
		updateStream(time);

		if (player_state == PLAYER_STOPPED) {
			// Disk needs to spin up, which takes 9.6s on
			// my Pioneer LD-92000. Also always seek to 
			// beginning (confirmed on real MSX and LD)
			video->seek(0);
			playingFromSample = 0;
			// Note that with "fullspeedwhenloading" this
			// should be reduced to.
			setack(time, 9600);
			seeking = true;
		} else {
			setack(time, 46);
		}
		player_state = PLAYER_PLAYING;
	}
}

unsigned LaserdiscPlayer::getCurrentSample(EmuTime::param time)
{
	return sampleClock.getTicksTill(time) + playingFromSample;
}

void LaserdiscPlayer::pause(EmuTime::param time)
{
	PRT_DEBUG("Laserdisc::Pause");
	if (video.get()) {
		updateStream(time);

		playingFromSample = getCurrentSample(time);
		player_state = PLAYER_PAUSED;
		setack(time, 46);
	}
}

void LaserdiscPlayer::stop(EmuTime::param time)
{
	PRT_DEBUG("Laserdisc::Stop");
	if (video.get()) {
		updateStream(time);

		player_state = PLAYER_STOPPED;
	}
}

void LaserdiscPlayer::seekFrame(int frame, EmuTime::param time)
{
	if (player_state != PLAYER_STOPPED) {
		PRT_DEBUG("Laserdisc::SeekFrame " << std::dec << frame);
		if (video.get()) {
			updateStream(time);

			// NTSC is 29.97Hz which is 30000 / 1001,
			// so frame 2997 is 100s and we want ms. The frame
			// number 5 digits, so we need 64 bits
			DivModByConst<30000> dm;
			playingFromSample = dm.div(1001000ull * frame);
			video->seek(playingFromSample);
			player_state = PLAYER_FROZEN;

			// seeking to the current frame takes 0.350s
			seeking = true;
			setack(time, 350);
		}
	}
}

void LaserdiscPlayer::seekChapter(int /*chapter*/, EmuTime::param /*time*/)
{
	if (player_state != PLAYER_STOPPED) {
		PRT_DEBUG("Laserdisc::SeekChapter " << std::dec << chapter);
		// we have no chapter information yet, so fail
	}
}

short LaserdiscPlayer::readSample(EmuTime::param time)
{
	/*
	 * Here we should return the value of the sample on the 
	 * right audio channel, ignoring muting.
	 */
	if (video.get() && player_state == PLAYER_PLAYING && !seeking) {
		updateStream(time);
		return tapein;
	}

	return 0;
}

bool LaserdiscPlayer::isVideoOutputAvailable(EmuTime::param time)
{
	bool videoOut;

	updateStream(time);

	switch (player_state) {
	case PLAYER_PLAYING:
	case PLAYER_FROZEN:
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

} // namespace openmsx
