#include "SonyHBIV1.hh"

#include "MSXMotherBoard.hh"
#include "RawFrame.hh"
#include "VDP.hh"
#include "serialize.hh"

#include <algorithm>

namespace openmsx {

static constexpr unsigned REG_BASE   = 0x7FFC;
static constexpr unsigned WINDOW     = 0x7E00;
static constexpr unsigned WIDTH      = 256;
static constexpr unsigned HEIGHT     = 212;

// Block layout of the digitizer VRAM: a block size code selects the scale
// between the source picture and the block (dest/src), the picture is split
// in 'dest' blocks per axis.
static constexpr std::array<unsigned, 8> SCALE_DEST = {1, 2, 3, 4, 4, 2, 3, 4};
static constexpr std::array<unsigned, 8> SCALE_SRC  = {1, 1, 1, 1, 3, 2, 2, 2};
static constexpr unsigned START_X[4][5] = {
	{0, 256, 256, 256, 256}, {0, 128, 256, 256, 256},
	{0,  88, 172, 256, 256}, {0,  64, 128, 192, 256},
};
static constexpr unsigned START_Y[4][5] = {
	{0, 212, 212, 212, 212}, {0, 106, 212, 212, 212},
	{0,  71, 142, 212, 212}, {0,  53, 106, 159, 212},
};

SonyHBIV1::SonyHBIV1(DeviceConfig& config)
	: MSXDevice(config)
	, rom(getName() + " ROM", "rom", config)
{
	reset(EmuTime::dummy());
}

void SonyHBIV1::reset(EmuTime /*time*/)
{
	command = startBlockX = startBlockY = blockSizeX = blockSizeY = 0;
	mode = delay = vramOffset = vramLine = status = 0;
	pending = false;
	pendingTime = EmuTime::infinity();
	busyUntil = EmuTime::zero();
	vram.fill(0);
}

VDP* SonyHBIV1::getVDP() const
{
	return dynamic_cast<VDP*>(getMotherBoard().findDevice("VDP"));
}

EmuDuration SonyHBIV1::frameDuration() const
{
	if (auto* vdp = getVDP()) return vdp->getFrameDuration();
	return EmuDuration::hz(60);
}

void SonyHBIV1::update(EmuTime time)
{
	if (pending && time >= pendingTime) {
		pending = false;
		digitize();
		busyUntil = pendingTime + frameDuration();
		delay = 0;
	}
}

// Convert a block of the (scaled) picture to the selected format and store it
// in the digitizer VRAM.
void SonyHBIV1::digitizeBlock(
	const std::array<uint8_t, WIDTH * HEIGHT * 3>& img,
	unsigned vramX, unsigned vramY, unsigned width, unsigned height,
	unsigned destX, unsigned destY, unsigned srcX, unsigned srcY)
{
	auto rgb = [&](unsigned x, unsigned y) {
		return std::span<const uint8_t, 3>(&img[(std::min(y, HEIGHT - 1) * WIDTH + std::min(x, WIDTH - 1)) * 3], 3);
	};
	auto lumaY = [](unsigned r, unsigned g, unsigned b) { // 5 bit
		return std::min(31u, b / 2 + r / 4 + g / 8);
	};

	for (unsigned y = 0; y < height; ++y) {
		const unsigned sy = y * destY / srcY;
		int j = 0, k = 0;
		for (unsigned x = 0; x < width; ++x) {
			const unsigned sx = x * destX / srcX;
			auto p = rgb(sx, sy);
			uint8_t value = 0;
			switch (mode) {
			case 0:
			case 1: {
				const unsigned mask = (mode == 0) ? 0xFF : 0xFE;
				if ((x & 3) == 0) {
					unsigned r = 0, g = 0, b = 0;
					for (unsigned i = 0; i < 4; ++i) {
						auto q = rgb(sx + i, sy);
						r += q[0] >> 3; g += q[1] >> 3; b += q[2] >> 3;
					}
					r /= 4; g /= 4; b /= 4;
					const int yy = int(lumaY(r, g, b));
					j = std::clamp(int(r) - yy, -32, 31);
					k = std::clamp(int(g) - yy, -32, 31);
					if (j < 0) j += 64;
					if (k < 0) k += 64;
				}
				const unsigned y5 = lumaY(p[0] >> 3, p[1] >> 3, p[2] >> 3) & mask;
				switch (x & 3) {
				case 0: value = uint8_t((k & 7)  | (y5 << 3)); break;
				case 1: value = uint8_t((k >> 3) | (y5 << 3)); break;
				case 2: value = uint8_t((j & 7)  | (y5 << 3)); break;
				case 3: value = uint8_t((j >> 3) | (y5 << 3)); break;
				}
				break;
			}
			case 2: // GRB 3-3-2
				value = uint8_t(((p[1] >> 5) << 5) | ((p[0] >> 5) << 2) | (p[2] >> 6));
				break;
			default:
				value = 0;
				break;
			}
			vram[(vramY + y) * WIDTH + vramX + x] = value;
		}
	}
}

void SonyHBIV1::digitize()
{
	auto* vdp = getVDP();
	const RawFrame* frame = vdp ? vdp->getExternalVideoFrame() : nullptr;
	if (!frame || frame->getHeight() == 0) return; // no video signal

	// Reduce the external picture to 256x212 (box filter), RGB 8 bit.
	static std::array<uint8_t, WIDTH * HEIGHT * 3> img; // not re-entrant, fine
	const unsigned srcH = frame->getHeight();
	for (unsigned y = 0; y < HEIGHT; ++y) {
		const unsigned sy0 = y * srcH / HEIGHT;
		const unsigned sy1 = std::max(sy0 + 1, (y + 1) * srcH / HEIGHT);
		for (unsigned x = 0; x < WIDTH; ++x) {
			unsigned r = 0, g = 0, b = 0, n = 0;
			for (unsigned sy = sy0; sy < std::min(sy1, srcH); ++sy) {
				const unsigned srcW = frame->getLineWidthDirect(sy);
				if (srcW == 0) continue;
				const auto line = frame->getLineDirect(sy);
				const unsigned sx0 = x * srcW / WIDTH;
				const unsigned sx1 = std::max(sx0 + 1, (x + 1) * srcW / WIDTH);
				for (unsigned sx = sx0; sx < std::min(sx1, srcW); ++sx) {
					auto p = line[sx]; // same layout as the VDP uses
					r += p & 0xFF; g += (p >> 8) & 0xFF; b += (p >> 16) & 0xFF;
					++n;
				}
			}
			auto* o = &img[(y * WIDTH + x) * 3];
			if (n) { o[0] = uint8_t(r / n); o[1] = uint8_t(g / n); o[2] = uint8_t(b / n); }
			else   { o[0] = o[1] = o[2] = 0; }
		}
	}

	const unsigned destX = SCALE_DEST[blockSizeX], destY = SCALE_DEST[blockSizeY];
	const unsigned srcX  = SCALE_SRC [blockSizeX], srcY  = SCALE_SRC [blockSizeY];
	unsigned bx = std::min<unsigned>(startBlockX, destX - 1);
	unsigned by = std::min<unsigned>(startBlockY, destY - 1);
	while (true) {
		const unsigned vx = START_X[destX - 1][bx];
		const unsigned vy = START_Y[destY - 1][by];
		const unsigned w = START_X[destX - 1][bx + 1] - vx;
		const unsigned h = START_Y[destY - 1][by + 1] - vy;
		digitizeBlock(img, vx, vy, w, h, destX, destY, srcX, srcY);
		if (++bx == destX) {
			bx = 0;
			if (++by == destY) break;
		}
	}
}

byte SonyHBIV1::readRegister(unsigned reg, EmuTime time, bool sideEffects)
{
	switch (reg) {
	case 0: { // 0x7FFC: status
		byte st = status;
		if (sideEffects) status ^= 0x30;
		if (time < busyUntil) st |= 0x80;
		return byte(st | command);
	}
	case 1: { // 0x7FFD
		auto* vdp = getVDP();
		const bool video = vdp && vdp->getExternalVideoFrame();
		const bool field = vdp && vdp->getEvenOdd();
		return byte((field ? 0x80 : 0) | (video ? 0 : 0x10) |
		            (startBlockY << 2) | startBlockX);
	}
	case 2: // 0x7FFE
		return byte((mode << 6) | (blockSizeY << 3) | blockSizeX);
	default: { // 0x7FFF: remaining delay
		if (!pending) return 0;
		const auto fd = frameDuration();
		return byte(std::min<unsigned>(255, (pendingTime - time).divUp(fd)));
	}
	}
}

byte SonyHBIV1::peekMem(uint16_t address, EmuTime time) const
{
	if (address < 0x4000 || address >= 0xC000) return 0xFF;
	const unsigned off = address - 0x4000;
	const unsigned a = address;
	if (a >= WINDOW && a < WINDOW + 0x100) {
		return vram[vramLine * WIDTH + vramOffset];
	}
	if (a >= REG_BASE && a < REG_BASE + 4) {
		return const_cast<SonyHBIV1*>(this)->readRegister(a - REG_BASE, time, false);
	}
	return rom[off];
}

byte SonyHBIV1::readMem(uint16_t address, EmuTime time)
{
	if (address < 0x4000 || address >= 0xC000) return 0xFF;
	update(time);
	const unsigned a = address;
	if (a >= WINDOW && a < WINDOW + 0x100) {
		byte value = vram[vramLine * WIDTH + vramOffset];
		if (++vramOffset == 0) {
			if (++vramLine == HEIGHT) vramLine = 0;
		}
		return value;
	}
	if (a >= REG_BASE && a < REG_BASE + 4) {
		return readRegister(a - REG_BASE, time, true);
	}
	return rom[address - 0x4000];
}

const byte* SonyHBIV1::getReadCacheLine(uint16_t address) const
{
	if (address < 0x4000 || address >= 0xC000) return unmappedRead.data();
	// the cache line(s) with the digitizer VRAM window and registers are
	// not cacheable
	if (address >= 0x7E00 && address < 0x8000) return nullptr;
	return &rom[address - 0x4000];
}

void SonyHBIV1::writeMem(uint16_t address, byte value, EmuTime time)
{
	if (address < REG_BASE || address >= REG_BASE + 4) return;
	update(time);
	switch (address - REG_BASE) {
	case 0:
		command = value & 3;
		vramOffset = 0;
		vramLine = 0;
		switch (command) {
		case 0:
			pending = false;
			busyUntil = EmuTime::zero();
			break;
		case 1:
			pending = false;
			digitize();
			busyUntil = time + frameDuration();
			break;
		case 2:
			if (delay == 0) {
				pending = false;
				digitize();
				busyUntil = time + frameDuration();
			} else {
				pending = true;
				pendingTime = time + frameDuration() * unsigned(delay);
			}
			break;
		default:
			break;
		}
		break;
	case 1:
		startBlockY = (value >> 2) & 3;
		startBlockX = (value >> 0) & 3;
		break;
	case 2:
		mode = (value >> 6) & 3;
		blockSizeY = (value >> 3) & 7;
		blockSizeX = (value >> 0) & 7;
		break;
	default:
		delay = value;
		break;
	}
}

byte* SonyHBIV1::getWriteCacheLine(uint16_t /*address*/)
{
	return nullptr; // ROM, registers: always go through writeMem()
}

template<typename Archive>
void SonyHBIV1::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("command", command,
	             "startBlockX", startBlockX,
	             "startBlockY", startBlockY,
	             "blockSizeX", blockSizeX,
	             "blockSizeY", blockSizeY,
	             "mode", mode,
	             "delay", delay,
	             "vramOffset", vramOffset,
	             "vramLine", vramLine,
	             "status", status,
	             "pending", pending,
	             "pendingTime", pendingTime,
	             "busyUntil", busyUntil);
	ar.serialize_blob("vram", vram);
}
INSTANTIATE_SERIALIZE_METHODS(SonyHBIV1);
REGISTER_MSXDEVICE(SonyHBIV1, "SonyHBIV1");

} // namespace openmsx
