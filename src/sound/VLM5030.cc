/*
    vlm5030.c

    VLM5030 emulator

    Written by Tatsuyuki Satoh
    Based on TMS5220 simulator (tms5220.c)

  note:
    memory read cycle(==sampling rate) = 122.9u(440clock)
    interpolator (LC8109 = 2.5ms)      = 20 * samples(125us)
    frame time (20ms)                  =  4 * interpolator
    9bit DAC is composed of 5bit Physical and 3bitPWM.

  todo:
    Noise Generator circuit without 'rand()' function.

----------- command format (Analytical result) ----------

1)end of speech (8bit)
:00000011:

2)silent some frame (8bit)
:????SS01:

SS : number of silent frames
   00 = 2 frame
   01 = 4 frame
   10 = 6 frame
   11 = 8 frame

3)-speech frame (48bit)
function:   6th  :  5th   :   4th  :   3rd  :   2nd  : 1st    :
end     :   ---  :  ---   :   ---  :   ---  :   ---  :00000011:
silent  :   ---  :  ---   :   ---  :   ---  :   ---  :0000SS01:
speech  :11111122:22233334:44455566:67778889:99AAAEEE:EEPPPPP0:

EEEEE  : energy : volume 0=off,0x1f=max
PPPPP  : pitch  : 0=noise , 1=fast,0x1f=slow
111111 : K1     : 48=off
22222  : K2     : 0=off,1=+min,0x0f=+max,0x10=off,0x11=+max,0x1f=-min
                : 16 == special function??
3333   : K3     : 0=off,1=+min,0x07=+max,0x08=-max,0x0f=-min
4444   : K4     :
555    : K5     : 0=off,1=+min,0x03=+max,0x04=-max,0x07=-min
666    : K6     :
777    : K7     :
888    : K8     :
999    : K9     :
AAA    : K10    :

 ---------- chirp table information ----------

DAC PWM cycle == 88system clock , (11clock x 8 pattern) = 40.6KHz
one chirp     == 5 x PWM cycle == 440system clock(8,136Hz)

chirp  0   : volume 10- 8 : with filter
chirp  1   : volume  8- 6 : with filter
chirp  2   : volume  6- 4 : with filter
chirp  3   : volume   4   : no filter ??
chirp  4- 5: volume  4- 2 : with filter
chirp  6-11: volume  2- 0 : with filter
chirp 12-..: volume   0   : silent

 ---------- digital output information ----------
 when ME pin = high , some status output to A0..15 pins

  A0..8   : DAC output value (abs)
  A9      : DAC sign flag , L=minus,H=Plus
  A10     : energy reload flag (pitch pulse)
  A11..15 : unknown

  [DAC output value(signed 6bit)] = A9 ? A0..8 : -(A0..8)

*/

#include "VLM5030.hh"

#include "DeviceConfig.hh"
#include "FileOperations.hh"
#include "XMLElement.hh"
#include "serialize.hh"

#include "cstd.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "random.hh"
#include "ranges.hh"
#include "xrange.hh"

#include <algorithm>
#include <array>
#include <cmath>

namespace openmsx {


// interpolator per frame
static constexpr int FR_SIZE = 4;
// samples per interpolator
static constexpr uint8_t IP_SIZE_SLOWER = 240 / FR_SIZE;
static constexpr uint8_t IP_SIZE_SLOW   = 200 / FR_SIZE;
static constexpr uint8_t IP_SIZE_NORMAL = 160 / FR_SIZE;
static constexpr uint8_t IP_SIZE_FAST   = 120 / FR_SIZE;
static constexpr uint8_t IP_SIZE_FASTER =  80 / FR_SIZE;

// speed parameter
// SPC SPB SPA
//  1   0   1  more slow (05h)     : 42ms   (150%) : 60sample
//  1   1   x  slow      (06h,07h) : 34ms   (125%) : 50sample
//  x   0   0  normal    (00h,04h) : 25.6ms (100%) : 40sample
//  0   0   1  fast      (01h)     : 20.2ms  (75%) : 30sample
//  0   1   x  more fast (02h,03h) : 12.2ms  (50%) : 20sample
static constexpr std::array<uint8_t, 8> VLM5030_speed_table =
{
	IP_SIZE_NORMAL,
	IP_SIZE_FAST,
	IP_SIZE_FASTER,
	IP_SIZE_FASTER,
	IP_SIZE_NORMAL,
	IP_SIZE_SLOWER,
	IP_SIZE_SLOW,
	IP_SIZE_SLOW
};

// ROM Tables

// This is the energy lookup table

// sampled from real chip
static constexpr std::array<uint16_t, 0x20> energyTable =
{
	  0,  2,  4,  6, 10, 12, 14, 18, //  0-7
	 22, 26, 30, 34, 38, 44, 48, 54, //  8-15
	 62, 68, 76, 84, 94,102,114,124, // 16-23
	136,150,164,178,196,214,232,254  // 24-31
};

// This is the pitch lookup table
static constexpr std::array<uint8_t, 0x20> pitchTable =
{
	1,                               // 0     : random mode
	22,                              // 1     : start=22
	23, 24, 25, 26, 27, 28, 29, 30,  //  2- 9 : 1step
	32, 34, 36, 38, 40, 42, 44, 46,  // 10-17 : 2step
	50, 54, 58, 62, 66, 70, 74, 78,  // 18-25 : 4step
	86, 94, 102,110,118,126          // 26-31 : 8step
};

static constexpr std::array<int16_t, 64> K1_table = {
	-24898,  -25672,  -26446,  -27091,  -27736,  -28252,  -28768,  -29155,
	-29542,  -29929,  -30316,  -30574,  -30832,  -30961,  -31219,  -31348,
	-31606,  -31735,  -31864,  -31864,  -31993,  -32122,  -32122,  -32251,
	-32251,  -32380,  -32380,  -32380,  -32509,  -32509,  -32509,  -32509,
	 24898,   23995,   22963,   21931,   20770,   19480,   18061,   16642,
	 15093,   13416,   11610,    9804,    7998,    6063,    3999,    1935,
	     0,   -1935,   -3999,   -6063,   -7998,   -9804,  -11610,  -13416,
	-15093,  -16642,  -18061,  -19480,  -20770,  -21931,  -22963,  -23995
};
static constexpr std::array<int16_t, 32> K2_table = {
	     0,   -3096,   -6321,   -9417,  -12513,  -15351,  -18061,  -20770,
	-23092,  -25285,  -27220,  -28897,  -30187,  -31348,  -32122,  -32638,
	     0,   32638,   32122,   31348,   30187,   28897,   27220,   25285,
	 23092,   20770,   18061,   15351,   12513,    9417,    6321,    3096
};
static constexpr std::array<int16_t, 16> K3_table = {
	    0,   -3999,   -8127,  -12255,  -16384,  -20383,  -24511,  -28639,
	32638,   28639,   24511,   20383,   16254,   12255,    8127,    3999
};
static constexpr std::array<int16_t, 8> K5_table = {
	0,   -8127,  -16384,  -24511,   32638,   24511,   16254,    8127
};

unsigned VLM5030::getBits(unsigned sBit, unsigned bits) const
{
	unsigned offset = address + (sBit / 8);
	unsigned data = rom[(offset + 0) & address_mask] +
	                rom[(offset + 1) & address_mask] * 256;
	data >>= (sBit & 7);
	data &= (0xFF >> (8 - bits));
	return data;
}

// get next frame
int VLM5030::parseFrame()
{
	// remember previous frame
	old_energy = new_energy;
	old_pitch = new_pitch;
	old_k = new_k;
	// command byte check
	if (uint8_t cmd = rom[address & address_mask];
	    cmd & 0x01) {
		// extend frame
		new_energy = new_pitch = 0;
		ranges::fill(new_k, 0);
		++address;
		if (cmd & 0x02) {
			// end of speech
			return 0;
		} else {
			// silent frame
			int nums = ((cmd >> 2) + 1) * 2;
			return nums * FR_SIZE;
		}
	}
	// pitch
	new_pitch  = narrow_cast<uint8_t>((pitchTable[getBits(1, 5)] + pitch_offset) & 0xff);
	// energy
	new_energy = energyTable[getBits(6, 5)];

	// 10 K's
	new_k[9] = K5_table[getBits(11, 3)];
	new_k[8] = K5_table[getBits(14, 3)];
	new_k[7] = K5_table[getBits(17, 3)];
	new_k[6] = K5_table[getBits(20, 3)];
	new_k[5] = K5_table[getBits(23, 3)];
	new_k[4] = K5_table[getBits(26, 3)];
	new_k[3] = K3_table[getBits(29, 4)];
	new_k[2] = K3_table[getBits(33, 4)];
	new_k[1] = K2_table[getBits(37, 5)];
	new_k[0] = K1_table[getBits(42, 6)];

	address += 6;
	return FR_SIZE;
}

// decode and buffering data
void VLM5030::generateChannels(std::span<float*> bufs, unsigned num)
{
	// Single channel device: replace content of bufs[0] (not add to it).
	using enum Phase;
	if (phase == IDLE) {
		bufs[0] = nullptr;
		return;
	}

	int buf_count = 0;

	// running
	if (phase == one_of(RUN, STOP)) {
		// playing speech
		while (num > 0) {
			// check new interpolator or new frame
			if (sample_count == 0) {
				if (phase == STOP) {
					phase = END;
					sample_count = 1;
					goto phase_stop; // continue to end phase
				}
				sample_count = frame_size;
				// interpolator changes
				if (interp_count == 0) {
					// change to new frame
					interp_count = narrow_cast<uint8_t>(parseFrame()); // with change phase
					if (interp_count == 0) {
						// end mark found
						interp_count = FR_SIZE;
						sample_count = frame_size; // end -> stop time
						phase = STOP;
					}
					// Set old target as new start of frame
					current_energy = old_energy;
					current_pitch = old_pitch;
					ranges::copy(old_k, current_k); // no assignment because arrays have different type (intentional?)
					// is this a zero energy frame?
					if (current_energy == 0) {
						target_energy = 0;
						target_pitch = narrow_cast<uint8_t>(current_pitch);
						ranges::transform(current_k, target_k.data(), [](int k) { return int16_t(k); }); // type conversion intentional?
					} else {
						// normal frame
						target_energy = new_energy;
						target_pitch = new_pitch;
						target_k = new_k;
					}
				}
				// next interpolator
				// Update values based on step values 25%, 50%, 75%, 100%
				interp_count -= interp_step;
				// 3,2,1,0 -> 1,2,3,4
				int interp_effect = FR_SIZE - (interp_count % FR_SIZE);
				current_energy = old_energy + (target_energy - old_energy) * interp_effect / FR_SIZE;
				if (old_pitch > 1) {
					current_pitch = old_pitch + (target_pitch - old_pitch) * interp_effect / FR_SIZE;
				}
				for (auto i : xrange(10))
					current_k[i] = old_k[i] + (target_k[i] - old_k[i]) * interp_effect / FR_SIZE;
			}
			// calculate digital filter
			int current_val = [&] {
				if (old_energy == 0) {
					// generate silent samples here
					return 0;
				} else if (old_pitch <= 1) {
					// generate unvoiced samples here
					return random_bool() ?  int(current_energy)
					                     : -int(current_energy);
				} else {
					// generate voiced samples here
					return (pitch_count == 0) ? int(current_energy) : 0;
				}
			}();

			// Lattice filter here
			std::array<int, 11> u;
			u[10] = current_val;
			for (int i = 9; i >= 0; --i) {
				u[i] = u[i + 1] - ((current_k[i] * x[i]) / 32768);
			}
			for (int i = 9; i >= 1; --i) {
				x[i] = x[i - 1] + ((current_k[i - 1] * u[i - 1]) / 32768);
			}
			x[0] = u[0];

			// clipping, buffering
			bufs[0][buf_count] = narrow<float>(std::clamp(u[0], -511, 511));
			++buf_count;
			--sample_count;
			++pitch_count;
			if (pitch_count >= current_pitch) {
				pitch_count = 0;
			}
			--num;
		}
	// return;
	}
phase_stop:
	switch (phase) {
	using enum Phase;
	case SETUP:
		if (sample_count <= num) {
			sample_count = 0;
			// pin_BSY = true;
			phase = WAIT;
		} else {
			sample_count -= narrow<uint8_t>(num);
		}
		break;
	case END:
		if (sample_count <= num) {
			sample_count = 0;
			pin_BSY = false;
			phase = IDLE;
		} else {
			sample_count -= narrow<uint8_t>(num);
		}
		break;
	default:
		break; // nothing
	}
	// silent buffering
	while (num > 0) {
		bufs[0][buf_count++] = 0;
		--num;
	}
}

float VLM5030::getAmplificationFactorImpl() const
{
	return 1.0f / (1 << 9);
}

// setup parameter option when RST=H
void VLM5030::setupParameter(uint8_t param)
{
	// latch parameter value
	parameter = param;

	// bit 0,1 : 4800bps / 9600bps , interpolator step
	if (param & 2) {          // bit 1 = 1 , 9600bps
		interp_step = 4;  // 9600bps : no interpolator
	} else if (param & 1) {   // bit1 = 0 & bit0 = 1 , 4800bps
		interp_step = 2;  // 4800bps : 2 interpolator
	} else {                  // bit1 = bit0 = 0 : 2400bps
		interp_step = 1;  // 2400bps : 4 interpolator
	}

	// bit 3,4,5 : speed (frame size)
	frame_size = VLM5030_speed_table[(param >> 3) & 7];

	// bit 6,7 : low / high pitch
	if (param & 0x80) {        // bit7=1 , high pitch
		pitch_offset = -8;
	} else if (param & 0x40) { // bit6=1 , low pitch
		pitch_offset = 8;
	} else {
		pitch_offset = 0;
	}
}

void VLM5030::reset()
{
	phase = Phase::RESET;
	address = 0;
	vcu_addr_h = 0;
	pin_BSY = false;

	old_energy = old_pitch = 0;
	new_energy = new_pitch = 0;
	current_energy = current_pitch = 0;
	target_energy = target_pitch = 0;
	ranges::fill(old_k, 0);
	ranges::fill(new_k, 0);
	ranges::fill(current_k, 0);
	ranges::fill(target_k, 0);
	interp_count = sample_count = pitch_count = 0;
	ranges::fill(x, 0);
	// reset parameters
	setupParameter(0x00);
}

// get BSY pin level
bool VLM5030::getBSY(EmuTime::param time) const
{
	const_cast<VLM5030*>(this)->updateStream(time);
	return pin_BSY;
}

// latch control data
void VLM5030::writeData(uint8_t data)
{
	latch_data = data;
}

void VLM5030::writeControl(uint8_t data, EmuTime::param time)
{
	updateStream(time);
	setRST((data & 0x01) != 0);
	setVCU((data & 0x04) != 0);
	setST ((data & 0x02) != 0);
}

// set RST pin level : reset / set table address A8-A15
void VLM5030::setRST(bool pin)
{
	if (pin_RST) {
		if (!pin) { // H -> L : latch parameters
			pin_RST = false;
			setupParameter(latch_data);
		}
	} else {
		if (pin) { // L -> H : reset chip
			pin_RST = true;
			if (pin_BSY) {
				reset();
			}
		}
	}
}

// set VCU pin level : ?? unknown
void VLM5030::setVCU(bool pin)
{
	// direct mode / indirect mode
	pin_VCU = pin;
}

// set ST pin level  : set table address A0-A7 / start speech
void VLM5030::setST(bool pin)
{
	if (pin_ST == pin) {
		// pin level unchanged
		return;
	}
	if (!pin) {
		// H -> L
		pin_ST = false;
		if (pin_VCU) {
			// direct access mode & address High
			vcu_addr_h = narrow<uint16_t>((latch_data << 8) + 0x01);
		} else {
			// check access mode
			if (vcu_addr_h) {
				// direct access mode
				address = (vcu_addr_h & 0xff00) + latch_data;
				vcu_addr_h = 0;
			} else {
				// indirect access mode
				int table = (latch_data & 0xfe) + ((int(latch_data) & 1) << 8);
				address = uint16_t((rom[(table + 0) & address_mask] << 8) |
				                   (rom[(table + 1) & address_mask] << 0));
			}
			// reset process status
			sample_count = frame_size;
			interp_count = FR_SIZE;
			// clear filter
			// start after 3 sampling cycle
			phase = Phase::RUN;
		}
	} else {
		// L -> H
		pin_ST = true;
		// setup speech, BSY on after 30ms?
		phase = Phase::SETUP;
		sample_count = 1; // wait time for busy on
		pin_BSY = true;
	}
}


static XMLElement* getRomConfig(
	DeviceConfig& config, const std::string& name, std::string_view romFilename)
{
	auto& doc = config.getXMLDocument();
	auto* voiceROMconfig = doc.allocateElement(doc.allocateString(name));
	voiceROMconfig->setFirstAttribute(doc.allocateAttribute("id", "name"));
	auto* romElement = voiceROMconfig->setFirstChild(doc.allocateElement("rom"));
	romElement->setFirstChild(doc.allocateElement( // load by sha1sum
			"sha1", "4f36d139ee4baa7d5980f765de9895570ee05f40"))
	         ->setNextSibling(doc.allocateElement( // load by predefined filename in software rom's dir
			"filename",
			doc.allocateString(tmpStrCat(FileOperations::stripExtension(romFilename), "_voice.rom"))))
		 ->setNextSibling(doc.allocateElement( // or hardcoded filename in ditto dir
			"filename", "keyboardmaster/voice.rom"));
	return voiceROMconfig;
}

static constexpr auto INPUT_RATE = unsigned(cstd::round(3579545 / 440.0));

VLM5030::VLM5030(const std::string& name_, static_string_view desc,
                 std::string_view romFilename, const DeviceConfig& config)
	: ResampledSoundDevice(config.getMotherBoard(), name_, desc, 1, INPUT_RATE, false)
	, rom(name_ + " ROM", "rom", DeviceConfig(config, *getRomConfig(const_cast<DeviceConfig&>(config), name_, romFilename)))
{
	reset();
	phase = Phase::IDLE;

	assert(rom.size() != 0);
	address_mask = narrow<unsigned>(rom.size() - 1);

	registerSound(config);
}

VLM5030::~VLM5030()
{
	unregisterSound();
}

static constexpr std::initializer_list<enum_string<VLM5030::Phase>> phaseInfo = {
	{ "RESET", VLM5030::Phase::RESET },
	{ "IDLE",  VLM5030::Phase::IDLE  },
	{ "SETUP", VLM5030::Phase::SETUP },
	{ "WAIT",  VLM5030::Phase::WAIT  },
	{ "RUN",   VLM5030::Phase::RUN   },
	{ "STOP",  VLM5030::Phase::STOP  },
	{ "END",   VLM5030::Phase::END   },
};
SERIALIZE_ENUM(VLM5030::Phase, phaseInfo);

// version 1: initial version
// version 2: serialize 'phase' as an enum instead of uint8_t
template<typename Archive>
void VLM5030::serialize(Archive& ar, unsigned version)
{
	ar.serialize("address_mask",   address_mask,
	             "frame_size",     frame_size,
	             "pitch_offset",   pitch_offset,
	             "current_energy", current_energy,
	             "current_pitch",  current_pitch,
	             "current_k",      current_k,
	             "x",              x,
	             "address",        address,
	             "vcu_addr_h",     vcu_addr_h,
	             "old_k",          old_k,
	             "new_k",          new_k,
	             "target_k",       target_k,
	             "old_energy",     old_energy,
	             "new_energy",     new_energy,
	             "target_energy",  target_energy,
	             "old_pitch",      old_pitch,
	             "new_pitch",      new_pitch,
	             "target_pitch",   target_pitch,
	             "interp_step",    interp_step,
	             "interp_count",   interp_count,
	             "sample_count",   sample_count,
	             "pitch_count",    pitch_count,
	             "latch_data",     latch_data,
	             "parameter",      parameter,
	             "pin_BSY",        pin_BSY,
	             "pin_ST",         pin_ST,
	             "pin_VCU",        pin_VCU,
	             "pin_RST",        pin_RST);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("phase", phase);
	} else {
		assert(Archive::IS_LOADER);
		int tmp = 0; // dummy init to avoid warning
		ar.serialize("phase", tmp);
		switch (tmp) {
			using enum Phase;
			case 0:  phase = RESET; break;
			case 1:  phase = IDLE;  break;
			case 2:  phase = SETUP; break;
			case 3:  phase = WAIT;  break;
			case 4:  phase = RUN;   break;
			case 5:  phase = STOP;  break;
			default: phase = END;   break;
		}
	}
}

INSTANTIATE_SERIALIZE_METHODS(VLM5030);

} // namespace openmsx
