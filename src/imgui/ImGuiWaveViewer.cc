#include "ImGuiWaveViewer.hh"

#include "ImGuiPlot.hh"

#include "MSXMixer.hh"
#include "MSXMotherBoard.hh"
#include "SoundDevice.hh"

#include "FFTReal.hh"
#include "fast_log2.hh"
#include "halfband.hh"
#include "hammingWindow.hh"
#include "narrow.hh"
#include "ranges.hh"
#include "xrange.hh"
#include "yin.hh"

#include "imgui.h"
#include "imgui_internal.h" // for ImLerp

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <numeric>
#include <ranges>

namespace openmsx {

#define SHOW_CHIP_KEY "showChip"

void ImGuiWaveViewer::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
	for (const auto& chip: openChips) {
		buf.appendf(SHOW_CHIP_KEY"=%s\n", chip.c_str());
	}
}

void ImGuiWaveViewer::loadStart()
{
	openChips.clear();
}

void ImGuiWaveViewer::loadLine(std::string_view name, zstring_view value)
{
	if (loadOnePersistent(name, value, *this, persistentElements)) {
		// already handled
	} else if (name == SHOW_CHIP_KEY) {
		openChips.emplace(value);
	}
}

static void paintVUMeter(std::span<const float>& buf, float factor, bool muted)
{
	// skip if not visible
	gl::vec2 pos = ImGui::GetCursorScreenPos();
	ImGui::SetNextItemWidth(-FLT_MIN); // full cell-width
	auto width = ImGui::CalcItemWidth();
	auto height = ImGui::GetFrameHeight();
	ImGui::Dummy(gl::vec2{width, height});
	if (!ImGui::IsItemVisible()) return;
	if (buf.size() <= 2) return;

	// calculate the average power of the signal
	auto len = float(buf.size());
	auto avg = std::reduce(buf.begin(), buf.end()) / len;
	auto squaredSum = std::transform_reduce(buf.begin(), buf.end(), 0.0f,
		std::plus<>{}, [avg](float x) { auto norm = x - avg; return norm * norm; });
	if (squaredSum == 0.0f) {
		buf = {}; // allows to skip waveform and spectrum calculations
		return;
	}
	auto power = fast_log2((squaredSum * factor * factor) / len);

	// transform into a value for how to draw this [0, 1]
	static constexpr auto convertLog = std::numbers::ln10_v<float> / std::numbers::ln2_v<float>; // log2 vs log10
	static constexpr auto range = 6.0f * convertLog; // 60dB
	auto clamped = std::clamp(power, -range, 0.0f);
	auto f = (clamped + range) / range;

	// draw gradient
	auto size = gl::vec2{f * width, height};
	auto colorL = muted ? ImVec4{0.2f, 0.2f, 0.2f, 1.0f} : ImVec4{0.0f, 1.0f, 0.0f, 1.0f}; // green
	auto colorR = muted ? ImVec4{0.7f, 0.7f, 0.7f, 1.0f} : ImVec4{1.0f, 0.0f, 0.0f, 1.0f}; // red
	auto color1 = ImGui::ColorConvertFloat4ToU32(colorL);
	auto color2 = ImGui::ColorConvertFloat4ToU32(ImLerp(colorL, colorR, f));
	auto* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilledMultiColor(
		pos, pos + size,
		color1, color2, color2, color1);
}

static void paintWave(std::span<const float> buf)
{
	// skip if not visible
	auto pos = ImGui::GetCursorPos();
	ImGui::SetNextItemWidth(-FLT_MIN); // full cell-width
	auto size = gl::vec2{ImGui::CalcItemWidth(), ImGui::GetFrameHeight()};
	ImGui::Dummy(size);
	if (!ImGui::IsItemVisible()) return;
	ImGui::SetCursorPos(pos);

	if (buf.size() < 2) {
		// otherwise PlotLines() doesn't draw anything, we want a line in the middle
		static constexpr std::array<float, 2> silent = {0.0f, 0.0f};
		buf = silent;
	}
	auto peak = std::transform_reduce(buf.begin(), buf.end(), 0.0f,
		[](auto x, auto y) { return std::max(x, y); },
		[](auto x) { return std::abs(x); });
	if (peak == 0.0f) peak = 1.0f; // force line in the middle

	plotLines(buf, -peak, peak, size);
}

struct ReduceResult {
	std::span<float> result; // down-sampled input
	std::span<float> extendedResult; // zero-padded to power-of-2
	float normalize = 1.0f; // down-sampling changes amplitude
	float reducedSampleRate = 0.0f;
};
static ReduceResult reduce(std::span<const float> buf, std::span<float> work, size_t fftLen, float sampleRate)
{
	// allocate part of the workBuffer (like a monotonic memory allocator)
	auto allocate = [&](size_t size) {
		assert(size <= work.size());
		auto result = work.first(size);
		work = work.subspan(size);
		return result;
	};

	// This function reduces (or rarely extends) the given buffer to exactly
	// 2048 samples, while retaining the lower part of the frequency
	// spectrum.
	//
	// Typically 'buf' contains too many samples. E.g. the PSG produces
	// sound at ~224kHz, so 1/7 second takes ~31.9k samples. We're not
	// interested in those very high frequencies. And we don't want to run a
	// needlessly expensive FFT operation. Therefor this routine:
	// * Filters away the upper-half of the frequency spectrum (via a
	//   half-band FIR filter).
	// * Then drops every other sample (this doesn't change the spectrum if
	//   the filter has high enough quality).
	// * Repeat until the buffer has 'fftLen' samples or less.
	// * Finally zero-pad the result to exactly 'fftLen' samples (this does not
	//   change the spectrum).
	float normalize = 1.0f;
	std::span<float> extended;
	if (buf.size() <= fftLen) {
		extended = allocate(fftLen);
		auto buf2 = extended.subspan(0, buf.size());
		copy_to_range(buf, buf2);
		buf = buf2;
	} else {
		assert(buf.size() >= HALF_BAND_EXTRA);
		extended = allocate(std::max((buf.size() - HALF_BAND_EXTRA) / 2, fftLen));
		do {
			static_assert(HALF_BAND_EXTRA & 1);
			if ((buf.size() & 1) == 0) {
				buf = buf.subspan(1); // drop oldest sample (need an odd number)
			}
			assert(buf.size() >= HALF_BAND_EXTRA);
			auto buf2 = extended.subspan(0, (buf.size() - HALF_BAND_EXTRA) / 2);

			halfBand(buf, buf2); // possibly inplace
			normalize *= 0.5f;
			sampleRate *= 0.5f;

			buf = buf2;
		} while (buf.size() > fftLen);
		extended = extended.subspan(0, fftLen);
	}
	std::ranges::fill(extended.subspan(buf.size()), 0.0f);
	auto result = extended.subspan(0, buf.size());
	return {.result = result,
		.extendedResult = extended,
		.normalize = normalize,
		.reducedSampleRate = sampleRate};
}

// format with "Hz" or "kHz" suffix and 3 significant digits
static std::string formatFreq(float freq)
{
	auto note = freq2note(freq);
	auto iFreq = int(std::lround(freq));
	if (iFreq < 1000) {
		return strCat(iFreq, "Hz  ", note);
	} else {
		auto k = iFreq / 1000;
		auto t = (iFreq % 1000) / 10;
		char t1 = char(t / 10) + '0';
		char t2 = char(t % 10) + '0';
		return strCat(k, '.', t1, t2, "kHz  ", note);
	}
}

static void paintSpectrum(std::span<const float> buf, float factor, const SoundDevice& device)
{
	static constexpr auto convertLog = std::numbers::ln10_v<float> / std::numbers::ln2_v<float>; // log2 vs log10
	static constexpr auto range = 5.0f * convertLog; // 50dB

	// skip if not visible
	auto pos = ImGui::GetCursorPos();
	ImGui::SetNextItemWidth(-FLT_MIN); // full cell-width
	auto size = gl::vec2{ImGui::CalcItemWidth(), ImGui::GetFrameHeight()};
	ImGui::Dummy(size);
	if (!ImGui::IsItemVisible()) return;
	ImGui::SetCursorPos(pos);

	const auto& style = ImGui::GetStyle();
	auto graphWidth = size.x - 2.0f * style.FramePadding.x;
	auto fftLen = std::clamp<size_t>(2 * std::bit_ceil(size_t(graphWidth)), 256, 2048);
	switch (fftLen) {
	case 256: // 128 pixels-wide or less
		buf = buf.last(buf.size() / 8); // keep last 1/8 (18ms, 56Hz bins)
		break;
	case 512: // 129..256 pixels (this is the default)
		buf = buf.last(buf.size() / 4); // keep last 1/4 (36ms, 28Hz bins)
		break;
	case 1024: // 257..512 pixels
		buf = buf.last(buf.size() / 2); // keep last 1/2 (71ms, 14Hz bins)
		break;
	default: // more than 513 pixels wide
		// keep full buffer (143ms, 7Hz bins)
		break;
	}

	float sampleRate = 0.0f;
	std::array<float, 1023> magnitude_; // ok, uninitialized
	std::span<float> magnitude;
	if (buf.size() >= 2) {
		// We want to take an FFT of fixed length (256, 512, 1024, 2048 points), reduce the
		// input (decimate + zero-pad) to this exact length.
		std::array<float, 32768> workBuf; // ok, uninitialized
		auto [signal, zeroPadded, normalize, sampleRate_] = reduce(buf, workBuf, fftLen, device.getNativeSampleRate());
		sampleRate = sampleRate_;
		assert(zeroPadded.size() == fftLen);

		// remove DC and apply window-function
		auto window = hammingWindow(narrow<unsigned>(signal.size()));
		auto avg = std::reduce(signal.begin(), signal.end()) / float(signal.size());
		for (auto [s, w] : std::views::zip(signal, window)) {
			s = (s - avg) * w;
		}

		// perform FFT
		std::array<float, 2048> tmp_; // ok, uninitialized
		std::array<float, 2048> f_;   // ok, uninitialized
		switch (fftLen) {
		case 256:
			FFTReal<8>::execute(subspan<256>(zeroPadded), subspan<256>(f_), subspan<256>(tmp_));
			break;
		case 512:
			FFTReal<9>::execute(subspan<512>(zeroPadded), subspan<512>(f_), subspan<512>(tmp_));
			break;
		case 1024:
			FFTReal<10>::execute(subspan<1024>(zeroPadded), subspan<1024>(f_), subspan<1024>(tmp_));
			break;
		default:
			FFTReal<11>::execute(subspan<2048>(zeroPadded), subspan<2048>(f_), subspan<2048>(tmp_));
			break;
		}
		auto f = subspan(f_, 0, fftLen);

		// combine real and imaginary components into magnitude (we ignore phase)
		normalize *= factor;
		auto offset = fast_log2(normalize * normalize * (1.0f / float(fftLen))) + range;
		auto halfFftLen = fftLen / 2;
		magnitude = subspan(magnitude_, 0, halfFftLen - 1);
		for (unsigned i = 0; i < halfFftLen - 1; ++i) {
			float real = f[i + 1];
			float imag = f[i + 1 + halfFftLen];
			float mag = real * real + imag * imag;
			magnitude[i] = fast_log2(mag) + offset;
		}
	}

	// actually plot the result
	plotHistogram(magnitude, 0.0f, range, size);

	simpleToolTip([&] -> std::string {
		auto scrnPosX = ImGui::GetCursorScreenPos().x + style.FramePadding.x;
		auto mouseX = (ImGui::GetIO().MousePos.x - scrnPosX) / graphWidth;
		if ((mouseX <= 0.0f) || (mouseX >= 1.0f)) return {};

		if (sampleRate == 0.0f) {
			// silent -> reduced sampleRate hasn't been calculated yet
			sampleRate = device.getNativeSampleRate();
			auto samples = device.getLastMonoBufferSize();
			switch (fftLen) {
				case 256:  samples /= 8; break;
				case 512:  samples /= 4; break;
				case 1024: samples /= 2; break;
				default: /*nothing*/     break;
			}
			while (samples > fftLen) {
				sampleRate *= 0.5f;
				if ((samples & 1) == 0) --samples;
				samples = (samples - HALF_BAND_EXTRA) / 2;
			}
		}

		return formatFreq(sampleRate * 0.5f * mouseX);
	});
}
static void paintPitch(std::span<const float> buf, const SoundDevice& device)
{
	if (buf.size() < 512) return;

	auto sampleRate = device.getNativeSampleRate();

	std::array<float, 32768> workBuf;
	static constexpr float MAX_RATE = 22'000.0f; // 22kHz
	while (sampleRate > MAX_RATE) {
		static_assert(HALF_BAND_EXTRA & 1);
		if ((buf.size() & 1) == 0) {
			buf = buf.subspan(1); // drop oldest sample (need an odd number)
		}
		assert(buf.size() >= HALF_BAND_EXTRA);

		auto outSize = (buf.size() - HALF_BAND_EXTRA) / 2;
		assert(outSize <= workBuf.size());
		auto out = std::span{workBuf}.subspan(0, outSize);

		halfBand(buf, out); // possibly inplace

		sampleRate *= 0.5f;
		buf = out;
	}
	// at this point sampleRate is between 11kHz and 22kHz
	if (buf.size() < 512) return; // need minimum size for good detection

	auto [freq, err] = yin::detectPitch(buf, sampleRate);
	err = std::max(err, 0.0f); // needed?

	auto pos = ImGui::GetCursorPos();
	ImGui::InvisibleButton("hover", {-FLT_MIN, ImGui::GetFrameHeight()});
	simpleToolTip([&]{
		auto f = err > 0.8 ? 0.0f : freq;
		return strCat(formatFreq(f), "\nconfidence=", 1.0f - err);
	});
	ImGui::SetCursorPos(pos);

	if (err > 0.2f) return; // no clear frequency, don't draw anything
	const auto& style = ImGui::GetStyle();
	auto colText = style.Colors[ImGuiCol_Text];
	auto colTextDisabled = style.Colors[ImGuiCol_TextDisabled];
	auto col = [&]{
		if (err < 0.15f) {
			return ImLerp(colText, colTextDisabled, err * (1.0f / 0.15f));
		} else {
			auto c = colTextDisabled;
			c.w = std::lerp(c.w, 0.0f, (err - 0.15f) * (1.0f / (0.20f - 0.15f)));
			return c;
		}
	}();
	im::StyleColor(ImGuiCol_Text, col, [&]{
		ImGui::TextUnformatted(formatFreq(freq));
	});
}

static void stereoToMono(std::span<const float> stereo, float factorL, float factorR,
                         std::vector<float>& mono)
{
	assert((stereo.size() & 1) == 0);
	auto size = stereo.size() / 2;
	mono.resize(size);
	for (auto i : xrange(size)) {
		mono[i] = factorL * stereo[2 * i + 0]
		        + factorR * stereo[2 * i + 1];
	}
}

static void paintDevice(SoundDevice& device, std::span<const MSXMixer::SoundDeviceInfo::ChannelSettings> settings)
{
	std::vector<float> tmpBuf; // recycle buffer for all channels

	bool stereo = device.hasStereoChannels();
	auto [factorL, factorR] = device.getAmplificationFactor();
	auto factor = stereo ? 1.0f : factorL;

	im::ID_for_range(device.getNumChannels(), [&](int channel) {
		auto monoBuf = [&]{
			auto buf = device.getLastBuffer(channel);
			if (!stereo) return buf;
			stereoToMono(buf, factorL, factorR, tmpBuf);
			return std::span<const float>{tmpBuf};
		}();

		if (ImGui::TableNextColumn()) { // name
			ImGui::StrCat(channel + 1);
		}
		auto& muteSetting = *settings[channel].mute;
		bool muted = muteSetting.getBoolean();
		if (ImGui::TableNextColumn()) { // mute
			if (ImGui::Checkbox("##mute", &muted)) {
				muteSetting.setBoolean(muted);
			}
		}
		if (ImGui::TableNextColumn()) { // vu-meter
			paintVUMeter(monoBuf, factor, muted);
		}
		if (ImGui::TableNextColumn()) { // waveform
			paintWave(monoBuf);
		}
		if (ImGui::TableNextColumn()) { // spectrum
			paintSpectrum(monoBuf, factor, device);
		}
		if (ImGui::TableNextColumn()) { // pitch estimation
			paintPitch(monoBuf, device);
		}
	});
}

void ImGuiWaveViewer::paint(MSXMotherBoard* motherBoard)
{
	if (!show || !motherBoard) return;

	ImGui::SetNextWindowSize(gl::vec2{38, 15} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Audio channel viewer", &show, [&]{
		for (const auto& info: motherBoard->getMSXMixer().getDeviceInfos()) {
			auto& device = *info.device;
			const auto& name = device.getName();
			auto it = openChips.find(name);
			bool wasOpen = it != openChips.end();
			bool nowOpen = ImGui::CollapsingHeader(name.c_str(), wasOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None);
			if (wasOpen != nowOpen) {
			    if (nowOpen) openChips.insert(name); else openChips.erase(it);
			}
			if (!nowOpen) continue;
			HelpMarker("Right-click column header to (un)hide columns.\n"
			           "Drag to reorder or resize columns.");

			int flags = ImGuiTableFlags_RowBg |
			            ImGuiTableFlags_BordersV |
			            ImGuiTableFlags_BordersOuter |
			            ImGuiTableFlags_Resizable |
			            ImGuiTableFlags_Reorderable |
			            ImGuiTableFlags_Hideable |
			            ImGuiTableFlags_SizingStretchProp;
			im::Table("##table", 6, flags, [&]{ // note: use the same id for all tables
				ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
				ImGui::TableSetupColumn("ch.", ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed);
				// TODO: make this work here, or add an alternative: simpleToolTip("channel number");
				ImGui::TableSetupColumn("mute", ImGuiTableColumnFlags_DefaultHide | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("VU-meter", 0, 1.0f);
				ImGui::TableSetupColumn("Waveform", 0, 2.0f);
				ImGui::TableSetupColumn("Spectrum", 0, 3.0f);
				ImGui::TableSetupColumn("Pitch estimation", ImGuiTableColumnFlags_DefaultHide, 1.0f);
				ImGui::TableHeadersRow();
				im::ID(name, [&]{
					paintDevice(device, info.channelSettings);
				});
			});
		}
	});
}

} // namespace openmsx
