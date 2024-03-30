// Based on code written by:
//   (2017) NataliaPC aka @ishwin74
//   Under GPL License

#ifndef TSXPARSER_HH
#define TSXPARSER_HH

#include "endian.hh"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

class TsxParser {
public:
	// output settings
	static constexpr unsigned TZX_Z80_FREQ       = 3500000; // 3.5MHz (according to TZX spec: not 3.579545MHz)
	static constexpr unsigned OUTPUT_FREQUENCY   = 58900;   // ~ = Z80_FREQ * 4 / T_STATES_MSX_PULSE
	static constexpr float    T_STATES_MSX_PULSE = 238.f;

	enum class FileType {
		ASCII, BINARY, BASIC, UNKNOWN,
	};

public:
	explicit TsxParser(std::span<const uint8_t> file);

	[[nodiscard]] std::vector<int8_t>&& stealOutput() { return std::move(output); }
	[[nodiscard]] std::optional<FileType> getFirstFileType() const { return firstFileType; }
	[[nodiscard]] const std::vector<std::string>& getMessages() const { return messages; }

private:
	struct Block10 {
		Endian::UA_L16  pauseMs;     // Pause after this block in milliseconds
		Endian::UA_L16  len;         // Length of data that follow
		//uint8_t       data[];      // Data as in .TAP files
	};
	struct Block11 {
		Endian::UA_L16  pilot;       // Length of PILOT pulse {2168}
		Endian::UA_L16  sync1;       // Length of SYNC first pulse {667}
		Endian::UA_L16  sync2;       // Length of SYNC second pulse {735}
		Endian::UA_L16  zero;        // Length of ZERO bit pulse {855}
		Endian::UA_L16  one;         // Length of ONE bit pulse {1710}
		Endian::UA_L16  pilotLen;    // Length of PILOT tone (number of pulses) {8063 header (flag<128), 3223 data (flag>=128)}
		uint8_t         lastBits;    // Used bits in the last byte (other bits should be 0) {8}
		Endian::UA_L16  pauseMs;     // Pause after this block (ms.) {1000}
		Endian::UA_L24  len;         // Length of data that follow
		//uint8_t       data[];      // Data as in .TAP files
	};
	struct Block12 {
		Endian::UA_L16  len;         // Length of one pulse in T-states
		Endian::UA_L16  pulses;      // Number of pulses
	};
	struct Block13 {
		uint8_t         num;         // Number of pulses
		//Endian::UA_L16 pulses[];   // [Array] Pulses' lengths
	};
	struct Block15 {
		Endian::UA_L16  bitTstates;  // Number of T-states per sample (bit of data)
		Endian::UA_L16  pauseMs;     // Pause after this block (ms.) {1000}
		uint8_t         lastBits;    // Used bits (samples) in last byte of data (1-8)
		Endian::UA_L24  len;         // Length of samples data
		//uint8_t       samples[];   // [Array] Samples data. Each bit represents a state on the EAR port (i.e. one sample). MSb is played first.
	};
	struct Block20 {
		Endian::UA_L16  pauseMs;     // Silence pause in milliseconds
	};
	struct Block21 {
		uint8_t         len;         // Length of the group name string
		//char          name[];      // The group name (ASCII)
	};
	struct Block30 {
		uint8_t         len;         // Length of the text description
		//char          text[];      // [Array] Text description in ASCII format
	};
	struct Block32 {
		Endian::UA_L16  blockLen;    // Length of the whole block (without these two bytes)
		uint8_t         num;         // Number of text strings
		//uint8_t       list[];      // [Array] List of text strings
	};
	struct Block35 {
		std::array<char, 0x10> label;// Identification string (in ASCII)
		Endian::UA_L32  len;         // Length of the custom info
		//uint8_t       data[];      // [Array] Custom info
	};
	struct Block4B {
		Endian::UA_L32  blockLen;    // Block length without these four bytes
		Endian::UA_L16  pauseMs;     // Pause after this block in milliseconds
		Endian::UA_L16  pilot;       // Duration of a PILOT pulse in T-states {same as ONE pulse}
		Endian::UA_L16  pulses;      // Number of pulses in the PILOT tone
		Endian::UA_L16  bit0len;     // Duration of a ZERO pulse in T-states {=2*pilot}
		Endian::UA_L16  bit1len;     // Duration of a ONE pulse in T-states {=pilot}
		uint8_t         bitCfg;
		uint8_t         byteCfg;
		//uint8_t       data[];      // [Array]
	};

	void processBlock10(const Block10* b);
	void processBlock11(const Block11* b);
	void processBlock12(const Block12* b);
	void processBlock13(const Block13* b);
	void processBlock15(const Block15* b);
	void processBlock20(const Block20* b);
	void processBlock21(const Block21* b);
	void processBlock30(const Block30* b);
	void processBlock32(const Block32* b);
	void processBlock35(const Block35* b);
	void processBlock4B(const Block4B* b);

	void writeSample(uint32_t tStates, int8_t value);
	void writePulse(uint32_t tStates);
	void writePulses(uint32_t count, uint32_t tStates);
	void writeSilence(int s);

	template<typename T> const T* get(size_t count = 1);

	void error(std::string msg);

private:
	// The parsed result is stored here
	std::vector<int8_t> output;
	std::vector<std::string> messages;
	std::optional<FileType> firstFileType;

	// The (remaining) part of the input file
	const uint8_t* buf;
	size_t remaining;

	// Intermediate state while writing the waveform
	float  accumBytes = 0.f;
	int8_t currentValue = 127;
};

#endif
