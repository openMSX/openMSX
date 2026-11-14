#include "ExternalAudio.hh"

#include "DeviceConfig.hh"
#include "BooleanSetting.hh"
#include "FilenameSetting.hh"
#include "FileOperations.hh"
#include "HardwareConfig.hh"
#include "IntegerSetting.hh"
#include "MSXMotherBoard.hh"
#include "MSXMixer.hh"
#include "Observer.hh"
#include "SoundDevice.hh"
#include "XMLElement.hh"
#include "serialize.hh"

#include "narrow.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// Layout of the ring buffer file, little-endian (see external_video_bridge.py):
//   0   4  magic "MXA1"
//   4   4  sample rate
//   8   2  channels
//   10  2  format (1 = signed 16 bit)
//   12  4  capacity in frames
//   16  8  total number of frames written so far
//   32  .  circular sample data

namespace openmsx {

static constexpr unsigned INPUT_RATE   = 48000;
static constexpr unsigned HEADER_SIZE  = 32;
static constexpr unsigned FRAME_BYTES  = 4; // 2 channels * 16 bit
static constexpr int      DEFAULT_DELAY_MS = 120;

class ExternalAudioSound final : public SoundDevice, private Observer<Setting>
{
public:
	explicit ExternalAudioSound(MSXMotherBoard& motherBoard);
	~ExternalAudioSound();

	// SoundDevice
	[[nodiscard]] bool isExternalSource() const override { return true; }
	void setOutputRate(unsigned hostSampleRate, double speed) override;
	void setRouting(bool left, bool right, EmuTime time) {
		updateStream(time); // the audio until now was routed in the old way
		routeLeft = left;
		routeRight = right;
	}
	bool updateBuffer(size_t length, float* buffer, EmuTime /*time*/) override {
		return mixChannels(buffer, length);
	}

protected:
	[[nodiscard]] float getAmplificationFactorImpl() const override {
		return 1.0f; // samples are already in [-1, 1]
	}
	void generateChannels(std::span<float*> buffers, unsigned num) override;

private:
	// Observer<Setting>
	void update(const Setting& setting) noexcept override;
	void applyMix();

	[[nodiscard]] bool ensureOpen();
	[[nodiscard]] bool readWritePosition(uint64_t& result);
	[[nodiscard]] bool readFrames(uint64_t first, size_t count);
	void closeFile();

private:
	MSXMixer& mixer;
	FilenameSetting fileSetting;
	IntegerSetting delaySetting;
	IntegerSetting mixSetting;
	BooleanSetting followSetting;
	float externalGain = 1.0f; // from the mix slider
	bool routeLeft = true;     // from the audio routing bits of port 0xF7
	bool routeRight = true;

	std::ifstream file;
	std::string openedName;
	std::chrono::steady_clock::time_point lastOpenTry{};
	uint32_t capacity = 0;
	unsigned hostRate = 44100;

	// Read position in input frames (fractional), see generateChannels().
	double readPos = 0.0;
	double fillError = 0.0; // smoothed difference with the wanted delay (frames)
	bool synced = false;
	uint64_t staleUntil = 0; // frames before this one were played already (or are too old)
	uint64_t lastWritePos = 0;
	std::chrono::steady_clock::time_point lastAdvance{};
	std::vector<int16_t> scratch; // raw frames read from the file
};

static DeviceConfig makeConfig(MSXMotherBoard& motherBoard)
{
	static XMLElement* xml = [] {
		auto& doc = XMLDocument::getStaticDocument();
		XMLElement* result = doc.allocateElement("externalaudio");
		// 32767: no extra scaling (the volume setting of the device does that)
		result->setFirstChild(doc.allocateElement("sound"))
		      ->setFirstChild(doc.allocateElement("volume", "32767"));
		return result;
	}();
	assert(motherBoard.getMachineConfig());
	return DeviceConfig(*motherBoard.getMachineConfig(), *xml);
}

ExternalAudioSound::ExternalAudioSound(MSXMotherBoard& motherBoard)
	: SoundDevice(motherBoard.getMSXMixer(), "external-audio",
	              "Audio of the external video source", 1, 44100, true)
	, mixer(motherBoard.getMSXMixer())
	, fileSetting(motherBoard.getCommandController(), "external_audio_file",
	              "ring buffer file with the audio of the external video "
	              "source, written by the external video bridge", "")
	, delaySetting(motherBoard.getCommandController(), "external_audio_delay",
	               "delay in milliseconds of the audio of the external "
	               "video source", DEFAULT_DELAY_MS, 0, 1000)
	, mixSetting(motherBoard.getCommandController(), "external_audio_mix",
	             "audio mix slider: 0 = only the sound of the computer (INT), "
	             "50 = both at full level, 100 = only the external audio (EXT)",
	             50, 0, 100)
	, followSetting(motherBoard.getCommandController(), "external_audio_follow_set_video",
	                "obey the audio routing of SET VIDEO (port 0xF7 of the NMS 8280): "
	                "after a reset no external audio is mixed in until SET VIDEO "
	                "selects it. Off: the external audio is always played",
	                false)
{
	registerSound(makeConfig(motherBoard));
	mixSetting.attach(*this);
	applyMix();
}

ExternalAudioSound::~ExternalAudioSound()
{
	mixSetting.detach(*this);
	mixer.setInternalGain(1.0f);
	unregisterSound();
}

void ExternalAudioSound::update(const Setting& setting) noexcept
{
	if (&setting == &mixSetting) applyMix();
}

// The slider of the NMS 8280 mixes the external audio (EXT) and the sound of
// the computer (INT); without external audio it works as the volume of the
// computer. The middle position plays both at full level.
void ExternalAudioSound::applyMix()
{
	const float position = float(mixSetting.getInt()) * 0.01f;
	externalGain = std::clamp(2.0f * position, 0.0f, 1.0f);
	mixer.setInternalGain(std::clamp(2.0f * (1.0f - position), 0.0f, 1.0f));
}

void ExternalAudioSound::closeFile()
{
	file.close();
	file.clear();
	openedName.clear();
	synced = false;
	staleUntil = 0;
	lastWritePos = 0;
}

bool ExternalAudioSound::ensureOpen()
{
	std::string filename = FileOperations::expandTilde(std::string(fileSetting.getString()));
	if (filename.empty()) {
		if (file.is_open()) closeFile();
		return false;
	}
	if (file.is_open() && filename == openedName) return true;

	// Not open (or another file now): retry at most twice per second, the
	// bridge might not have created the file yet.
	auto now = std::chrono::steady_clock::now();
	if (now - lastOpenTry < std::chrono::milliseconds(500)) return false;
	lastOpenTry = now;

	closeFile();
	file.open(filename, std::ios::binary);
	if (!file) {
		file.clear();
		return false;
	}
	std::array<char, HEADER_SIZE> header{};
	file.read(header.data(), header.size());
	if (!file || std::memcmp(header.data(), "MXA1", 4) != 0) {
		closeFile();
		return false;
	}
	uint32_t rate = 0, cap = 0;
	uint16_t channels = 0, format = 0;
	std::memcpy(&rate, &header[4], 4);
	std::memcpy(&channels, &header[8], 2);
	std::memcpy(&format, &header[10], 2);
	std::memcpy(&cap, &header[12], 4);
	if (rate != INPUT_RATE || channels != 2 || format != 1 || cap < 1024) {
		closeFile(); // not a format that we can play
		return false;
	}
	capacity = cap;
	openedName = filename;
	synced = false;
	return true;
}

bool ExternalAudioSound::readWritePosition(uint64_t& result)
{
	file.seekg(16);
	file.read(reinterpret_cast<char*>(&result), sizeof(result));
	if (!file) {
		closeFile(); // truncated or removed: try again later
		return false;
	}
	return true;
}

void ExternalAudioSound::setOutputRate(unsigned hostSampleRate, double /*speed*/)
{
	hostRate = hostSampleRate;
	setInputRate(hostSampleRate); // we produce samples at the host rate ourselves
	synced = false;
}

// Reads 'count' frames starting at the absolute frame number 'first' into
// 'scratch' (2 values per frame). Returns false if the file can't be read.
bool ExternalAudioSound::readFrames(uint64_t first, size_t count)
{
	scratch.resize(count * 2);
	size_t done = 0;
	while (done < count) {
		const uint64_t slot = (first + done) % capacity;
		const size_t n = std::min<size_t>(count - done, capacity - slot);
		file.seekg(std::streamoff(HEADER_SIZE + slot * FRAME_BYTES));
		file.read(reinterpret_cast<char*>(&scratch[done * 2]),
		          std::streamsize(n * FRAME_BYTES));
		if (!file) return false;
		done += n;
	}
	return true;
}

// Plays the audio 'delay' milliseconds after it was captured. The ring buffer
// is filled at the rate of the capture clock and emptied at the rate of the
// emulation, which are never exactly the same. Instead of jumping when they
// drift apart, the speed of reading is adjusted slightly (at most 1%) to keep
// the amount of buffered audio at the wanted delay. The 48 kHz input is
// converted to the host rate with a cubic interpolation.
void ExternalAudioSound::generateChannels(std::span<float*> buffers, unsigned num)
{
	assert(buffers.size() == 1);
	if (!ensureOpen()) {
		buffers[0] = nullptr; // silence
		return;
	}
	uint64_t writePos = 0;
	if (!readWritePosition(writePos)) {
		buffers[0] = nullptr;
		return;
	}

	const double delay = std::min<double>(
		double(delaySetting.getInt()) * INPUT_RATE / 1000.0, capacity / 2.0);
	const double baseStep = double(INPUT_RATE) / double(hostRate);
	const double wanted = std::max(delay, 2.0 * num * baseStep + 8.0);

	// Has the capture stopped? Then play silence, not the old audio again.
	const auto now = std::chrono::steady_clock::now();
	if (writePos != lastWritePos) {
		if (writePos < lastWritePos) staleUntil = 0; // the bridge was restarted
		lastWritePos = writePos;
		lastAdvance = now;
	} else if (now - lastAdvance > std::chrono::milliseconds(250)) {
		synced = false;
		staleUntil = writePos; // what is in the buffer was already played
		buffers[0] = nullptr;
		return;
	}

	// Where is the read position compared to the captured audio?
	double available = double(writePos) - readPos;
	if (synced && (available < 0.0 || available > capacity - 4.0 * num * baseStep - 16.0 ||
	               std::abs(available - wanted) > 0.5 * INPUT_RATE)) {
		synced = false; // long pause or hopelessly out of sync
		staleUntil = 0; // jump to the most recent audio
	}
	if (!synced) {
		// Start playing 'wanted' frames behind the newest audio, but never
		// from audio that was already played: after a gap wait until enough
		// new audio was captured.
		const auto wantedFrames = uint64_t(wanted);
		// (at least one frame of margin: the interpolation also looks at the
		// frame before the read position)
		if (writePos <= wantedFrames || writePos - wantedFrames < staleUntil) {
			buffers[0] = nullptr; // not enough (new) audio captured yet
			return;
		}
		readPos = double(writePos - wantedFrames);
		fillError = 0.0;
		synced = true;
		available = double(wantedFrames);
	}

	// Smoothed speed correction: a buffer that is fuller than wanted is
	// emptied a bit faster, and the other way around.
	fillError += 0.02 * ((available - wanted) - fillError);
	const double correction = std::clamp(0.02 * fillError / INPUT_RATE, -0.01, 0.01);
	const double step = baseStep * (1.0 + correction);

	// Frames needed: positions readPos .. readPos + num*step, plus the
	// neighbours for the interpolation.
	const double last = readPos + double(num) * step;
	if (double(writePos) < last + 3.0) {
		// Underrun: the capture is slower than the emulation. Silence, and
		// rebuild the delay with new audio only.
		synced = false;
		staleUntil = writePos;
		buffers[0] = nullptr;
		return;
	}
	const auto first = uint64_t(std::floor(readPos)) - 1;
	const auto count = size_t(std::floor(last) + 3 - double(first)) + 1;
	if (!readFrames(first, count)) {
		closeFile();
		buffers[0] = nullptr;
		return;
	}

	const bool follow = followSetting.getBoolean();
	const std::array<float, 2> scale = {
		(!follow || routeLeft)  ? externalGain * (1.0f / 32768.0f) : 0.0f,
		(!follow || routeRight) ? externalGain * (1.0f / 32768.0f) : 0.0f,
	};
	auto sample = [&](size_t index, unsigned channel) {
		return float(scratch[index * 2 + channel]) * scale[channel];
	};
	double pos = readPos;
	for (unsigned i = 0; i < num; ++i) {
		const double whole = std::floor(pos);
		const auto t = float(pos - whole);
		const size_t k = size_t(whole) - first; // index of the frame at 'whole'
		for (unsigned channel = 0; channel < 2; ++channel) {
			const float p0 = sample(k - 1, channel);
			const float p1 = sample(k,     channel);
			const float p2 = sample(k + 1, channel);
			const float p3 = sample(k + 2, channel);
			// Catmull-Rom spline between p1 and p2
			buffers[0][i * 2 + channel] = p1 + 0.5f * t * (p2 - p0 +
				t * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3 +
				t * (3.0f * (p1 - p2) + p3 - p0)));
		}
		pos += step;
	}
	readPos = pos;
}


// MSXExternalAudio ==========================================================

MSXExternalAudio::MSXExternalAudio(const DeviceConfig& config)
	: MSXDevice(config)
	, sound(std::make_unique<ExternalAudioSound>(getMotherBoard()))
{
}

MSXExternalAudio::~MSXExternalAudio() = default;

void MSXExternalAudio::setRouting(bool left, bool right, EmuTime time)
{
	sound->setRouting(left, right, time);
}

template<typename Archive>
void MSXExternalAudio::serialize(Archive& ar, unsigned /*version*/)
{
	// The audio comes from the host; there is no emulated state to keep.
	ar.template serializeBase<MSXDevice>(*this);
}
INSTANTIATE_SERIALIZE_METHODS(MSXExternalAudio);
REGISTER_MSXDEVICE(MSXExternalAudio, "ExternalAudio");

} // namespace openmsx
