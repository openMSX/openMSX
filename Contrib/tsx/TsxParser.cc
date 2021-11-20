// Based on code written by:
//   (2017) NataliaPC aka @ishwin74
//   Under GPL License

#include "TsxParser.hh"
#include "xrange.hh"
#include <cstring>
#include <sstream>

// Current version supports these TZX 1.20 blocks plus the #4B TSX block:
static constexpr uint8_t B10_STD_BLOCK      = 0x10; // Standard Speed Block (Turbo normal)
static constexpr uint8_t B11_TURBO_BLOCK    = 0x11; // Turbo Speed Block (Turbo speed)
static constexpr uint8_t B12_PURE_TONE      = 0x12; // Pure Tone Block
static constexpr uint8_t B13_PULSE_SEQUENCE = 0x13; // Pulse sequence Block
static constexpr uint8_t B15_DIRECT_REC     = 0x15; // Direct recording Block
static constexpr uint8_t B20_SILENCE_BLOCK  = 0x20; // Silence Block
static constexpr uint8_t B30_TEXT_DESCRIP   = 0x30; // Text description Block
static constexpr uint8_t B32_ARCHIVE_INFO   = 0x32; // Archive info Block
static constexpr uint8_t B35_CUSTOM_INFO    = 0x35; // Custom info Block
static constexpr uint8_t B4B_KCS_BLOCK      = 0x4B; // KCS (Kansas City Standard) Block

// These blocks are detected, but ignored
static constexpr uint8_t B21_GRP_START      = 0x21; // Group start Block
static constexpr uint8_t B22_GRP_END        = 0x22; // Group end Block
static constexpr uint8_t B5A_GLUE_BLOCK     = 0x5A; // Glue Block

// These blocks are not supported at all (trigger an error when encountered)
static constexpr uint8_t B14_PURE_DATA      = 0x14; // Pure data Block
static constexpr uint8_t B18_CSW_RECORDING  = 0x18; // CSW recording block
static constexpr uint8_t B19_GEN_DATA       = 0x19; // Generalized data block
static constexpr uint8_t B23_JUMP_BLOCK     = 0x23; // Jump Block
static constexpr uint8_t B24_LOOP_START     = 0x24; // Loop start Block
static constexpr uint8_t B25_LOOP_END       = 0x25; // Loop end Block
static constexpr uint8_t B26_CALL_SEQ       = 0x26; // Call sequence Block
static constexpr uint8_t B27_RET_SEQ        = 0x27; // Return sequence Block
static constexpr uint8_t B28_SELECT_BLOCK   = 0x28; // Select block
static constexpr uint8_t B2A_STOP_TAPE      = 0x2A; // Stop the tape if in 48K mode
static constexpr uint8_t B2B_SIGNAL_LEVEL   = 0x2B; // Set signal level
static constexpr uint8_t B31_MSG_BLOCK      = 0x31; // Message Block
static constexpr uint8_t B33_HARDWARE_TYPE  = 0x33; // Hardware type


[[nodiscard]] static std::string toHex(int x)
{
	std::stringstream ss;
	ss << std::hex << x;
	return ss.str();
}

TsxParser::TsxParser(span<const uint8_t> file)
{
	buf = file.data();
	remaining = file.size();

	// Check for a TZX header
	static constexpr uint8_t TSX_HEADER[8] = { 'Z','X','T','a','p','e','!', 0x1A };
	if ((memcmp(get<uint8_t>(sizeof(TSX_HEADER)), TSX_HEADER, sizeof(TSX_HEADER)) != 0)) {
		error("Invalid TSX header");
	}

	// Check version >= 1.21
	auto version = Endian::read_UA_B16(get<uint8_t>(2));
	if (version < 0x0115) {
		error("TSX version below 1.21");
	}

	while (remaining) {
		accumBytes = 0.f;
		auto blockId = *get<uint8_t>();
		switch (blockId) {
		case B10_STD_BLOCK:
			processBlock10(get<Block10>());
			break;
		case B11_TURBO_BLOCK:
			processBlock11(get<Block11>());
			break;
		case B12_PURE_TONE:
			processBlock12(get<Block12>());
			break;
		case B13_PULSE_SEQUENCE:
			processBlock13(get<Block13>());
			break;
		case B15_DIRECT_REC:
			processBlock15(get<Block15>());
			break;
		case B20_SILENCE_BLOCK:
			processBlock20(get<Block20>());
			break;
		case B21_GRP_START:
			processBlock21(get<Block21>());
			break;
		case B22_GRP_END:
			// ignore (block has no data)
			break;
		case B30_TEXT_DESCRIP:
			processBlock30(get<Block30>());
			break;
		case B32_ARCHIVE_INFO:
			processBlock32(get<Block32>());
			break;
		case B35_CUSTOM_INFO:
			processBlock35(get<Block35>());
			break;
		case B4B_KCS_BLOCK:
			processBlock4B(get<Block4B>());
			break;
		case B5A_GLUE_BLOCK:
			get<uint8_t>(10); // skip (ignore) this block
			break;
		case B14_PURE_DATA:
		case B18_CSW_RECORDING:
		case B19_GEN_DATA:
		case B23_JUMP_BLOCK:
		case B24_LOOP_START:
		case B25_LOOP_END:
		case B26_CALL_SEQ:
		case B27_RET_SEQ:
		case B28_SELECT_BLOCK:
		case B2A_STOP_TAPE:
		case B2B_SIGNAL_LEVEL:
		case B31_MSG_BLOCK:
		case B33_HARDWARE_TYPE:
			// TODO not yet implemented, useful?
			[[fallthrough]];
		default:
			error("Unsupported block: #" + toHex(blockId));
		}
	}
}

[[nodiscard]] static float tStates2samples(float tStates)
{
	return tStates * TsxParser::OUTPUT_FREQUENCY / TsxParser::TZX_Z80_FREQ;
}

void TsxParser::writeSample(uint32_t tStates, int8_t value)
{
	accumBytes += tStates2samples(tStates);
	output.insert(end(output), int(accumBytes), value);
	accumBytes -= int(accumBytes);
}

void TsxParser::writePulse(uint32_t tStates)
{
	writeSample(tStates, currentValue);
	currentValue = -currentValue;
}

void TsxParser::writePulses(uint32_t count, uint32_t tStates)
{
	repeat(count, [&] { writePulse(tStates); });
}

void TsxParser::writeSilence(int ms)
{
	if (!ms) return;
	output.insert(end(output), OUTPUT_FREQUENCY * ms / 1000, 0);
	currentValue = 127;
}

// Standard Speed Block
void TsxParser::processBlock10(const Block10* b)
{
	// delegate to 'block 11' but with some hardcoded values
	Block11 b11;
	b11.pilot = 2168;
	b11.sync1 = 667;
	b11.sync2 = 735;
	b11.zero = 855;
	b11.one = 1710;
	b11.pilotLen = 3223;
	b11.lastBits = 8;
	b11.pauseMs = b->pauseMs;
	b11.len = b->len;
	processBlock11(&b11);
}

// Turbo Speed Block
void TsxParser::processBlock11(const Block11* b)
{
	if ((b->len < 1) || (b->lastBits < 1) || (b->lastBits > 8)) {
		error("Invalid block #11");
	}

	currentValue = -127;
	writePulses(b->pilotLen, b->pilot);
	writePulse(b->sync1);
	writePulse(b->sync2);

	auto writeByte = [&](uint8_t d, int nBits) {
		for (auto bit : xrange(nBits)) {
			if (d & (128 >> bit)) {
				writePulses(2, b->one);
			} else {
				writePulses(2, b->zero);
			}
		}
	};
	uint32_t len = b->len;
	const auto* data = get<uint8_t>(len);
	repeat(len - 1, [&] { writeByte(*data++, 8); });
	writeByte(*data, b->lastBits);

	if (b->pauseMs != 0) writePulse(2000);
	writeSilence(b->pauseMs);
}

// Pure Tone Block
void TsxParser::processBlock12(const Block12* b)
{
	auto n = b->pulses & ~1; // round down to even
	writePulses(n, b->len);
}

// Pulse sequence Block
void TsxParser::processBlock13(const Block13* b)
{
	const auto* pulses = get<Endian::UA_L16>(b->num);
	for (auto i : xrange(b->num)) {
		writePulse(pulses[i]);
	}
}

// Direct Recording
void TsxParser::processBlock15(const Block15* b)
{
	if ((b->len < 1) || (b->lastBits < 1) || (b->lastBits > 8)) {
		error("Invalid block #15");
	}
	const auto* samples = get<uint8_t>(b->len);

	auto writeBit = [&](uint8_t& sample) {
		writeSample(b->bitTstates, (sample & 128) ? 127 : -127);
		sample <<= 1;
	};
	auto writeByte = [&](int nBits) {
		auto sample = *samples++;
		repeat(nBits, [&]{ writeBit(sample); });
	};
	repeat(int(b->len - 1), [&] { writeByte(8); });
	writeByte(b->lastBits);

	writeSilence(b->pauseMs);
}

// Silence Block
void TsxParser::processBlock20(const Block20* b)
{
	writeSilence(b->pauseMs);
}

// Group start Block
void TsxParser::processBlock21(const Block21* b)
{
	get<uint8_t>(b->len); // ignore group name
}

// Text description Block
void TsxParser::processBlock30(const Block30* b)
{
	const char* text = get<char>(b->len);
	messages.emplace_back(text, b->len);
}

// Archive info Block
void TsxParser::processBlock32(const Block32* b)
{
	uint16_t len = b->blockLen;
	auto extra = sizeof(Block32) - sizeof(uint16_t);
	if (len < extra) error("Invalid block #32");
	len -= extra; // number of available bytes in the data block

	const char* data = get<char>(len);
	repeat(uint32_t(b->num), [&] {
		if (len < 2) error("Invalid block #32");
		uint8_t textId  = data[0];
		uint8_t textLen = data[1];
		data += 2;
		len -= 2;

		if (len < textLen) error("Invalid block #32");
		if (textId == 0) {
			messages.emplace_back(data, textLen);
		}
		data += textLen;
		len -= textLen;
	});
	if (len != 0) error("Invalid block #32");
}

// Custom info Block
void TsxParser::processBlock35(const Block35* b)
{
	get<uint8_t>(b->len); // just skip (ignore) the data
}

// MSX KCS Block
void TsxParser::processBlock4B(const Block4B* b)
{
	static constexpr uint8_t ASCII_HEADER [10] = { 0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA,0xEA };
	static constexpr uint8_t BINARY_HEADER[10] = { 0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0 };
	static constexpr uint8_t BASIC_HEADER [10] = { 0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3,0xD3 };

	// get data block
	uint32_t len = b->blockLen;
	auto extra = sizeof(Block4B) - sizeof(uint32_t);
	if (len < extra) error("Invalid block #4B: invalid length");
	len -= extra; // number of available bytes in the data block
	const auto* data = get<uint8_t>(len);

	// determine file type
	if (!firstFileType && (len == 16)) {
		if (memcmp(data, ASCII_HEADER, sizeof(ASCII_HEADER)) == 0) {
			firstFileType = FileType::ASCII;
		} else if (memcmp(data, BINARY_HEADER, sizeof(BINARY_HEADER)) == 0) {
			firstFileType = FileType::BINARY;
		} else if (memcmp(data, BASIC_HEADER, sizeof(BASIC_HEADER)) == 0) {
			firstFileType = FileType::BASIC;
		} else {
			firstFileType = FileType::UNKNOWN;
		}
	}

	// read the block
	uint32_t pulsePilot = b->pilot;
	uint32_t pulseOne   = b->bit1len;
	uint32_t pulseZero  = b->bit0len;

	auto decodeBitCfg = [](uint8_t x) { return x ? x : 16; };
	auto numZeroPulses = decodeBitCfg(b->bitCfg >> 4); // 2 for MSX
	auto numOnePulses  = decodeBitCfg(b->bitCfg & 15); // 4 for MSX

	auto numStartBits = (b->byteCfg & 0b11000000) >> 6; // 1 for MSX
	bool startBitVal  = (b->byteCfg & 0b00100000) >> 5; // 0 for MSX
	auto numStopBits  = (b->byteCfg & 0b00011000) >> 3; // 2 for MSX
	bool stopBitVal   = (b->byteCfg & 0b00000100) >> 2; // 1 for MSX
	bool msb          = (b->byteCfg & 0b00000001) >> 0; // 0 (LSB first) for MSX
	if (b->byteCfg & 0b00000010) {
		error("Invalid block #4B: unsupported byte-cfg: " + toHex(b->byteCfg));
	}

	// write a header signal
	writePulses(b->pulses, pulsePilot);

	// write KCS bytes
	auto write_01 = [&](bool bit) {
		if (bit) {
			writePulses(numOnePulses, pulseOne);
		} else {
			writePulses(numZeroPulses, pulseZero);
		}
	};
	auto write_N_01 = [&](unsigned n, bool bit) {
		repeat(n, [&] { write_01(bit); });
	};
	for (auto i : xrange(len)) {
		// start bit(s)
		write_N_01(numStartBits, startBitVal);
		// 8 data bits
		uint8_t d = data[i];
		for (auto bit : xrange(8)) {
			auto mask = uint8_t(1) << (msb ? (7 - bit) : bit);
			write_01(d & mask);
		}
		// stop bit(s)
		write_N_01(numStopBits, stopBitVal);
	}
	writeSilence(b->pauseMs);
}

template<typename T>
const T* TsxParser::get(size_t count)
{
	static_assert(alignof(T) == 1, "T must be unaligned");

	auto bytes = count * sizeof(T);
	if (bytes > remaining) error("Invalid TSX file, read beyond end of file");
	remaining -= bytes;

	const T* result = reinterpret_cast<const T*>(buf);
	buf += bytes;
	return result;
}

void TsxParser::error(std::string msg)
{
	throw msg;
}
