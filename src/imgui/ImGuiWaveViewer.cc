#include "ImGuiWaveViewer.hh"

#include "MSXMixer.hh"
#include "MSXMotherBoard.hh"
#include "SoundDevice.hh"

#include "FFTReal.hh"
#include "hammingWindow.hh"
#include "fast_log2.hh"
#include "halfband.hh"
#include "narrow.hh"
#include "ranges.hh"
#include "view.hh"
#include "xrange.hh"

#include "imgui.h"
#include "imgui_internal.h" // for ImLerp

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <numeric>

namespace openmsx {

// Simplified/optimized version of ImGui::PlotLines()
static void plotLines(std::span<const float> values, float scale_min, float scale_max, gl::vec2 outer_size)
{
	const auto& style = ImGui::GetStyle();

	gl::vec2 pos = ImGui::GetCursorScreenPos();
	auto outer_tl = pos;              // top-left
	auto outer_br = pos + outer_size; // bottom-right
	auto inner_tl = outer_tl + gl::vec2(style.FramePadding);
	auto inner_br = outer_br - gl::vec2(style.FramePadding);

	ImGui::RenderFrame(outer_tl, outer_br, ImGui::GetColorU32(ImGuiCol_FrameBg),
	                   true, style.FrameRounding);

	int num_values = narrow<int>(values.size());
	if (num_values < 2) return;
	int num_items = num_values - 1;

	int inner_width = std::max(1, static_cast<int>(inner_br.x - inner_tl.x));
	int res_w = std::min(inner_width, num_items);

	float t_step = 1.0f / (float)res_w;
	float scale_range = scale_max - scale_min;
	float inv_scale = (scale_range != 0.0f) ? (1.0f / scale_range) : 0.0f;

	auto color = ImGui::GetColorU32(ImGuiCol_PlotLines);

	auto valueToY = [&](float v) {
		return 1.0f - std::clamp((v - scale_min) * inv_scale, 0.0f, 1.0f);
	};
	float t = 0.0f;
	auto tp0 = gl::vec2(t, valueToY(values[0]));
	auto pos0 = ImLerp(inner_tl, inner_br, tp0);

	auto* drawList = ImGui::GetWindowDrawList();
	for (int n = 0; n < res_w; n++) {
		auto idx = static_cast<int>(t * float(num_items) + 0.5f);
		assert(0 <= idx && idx < num_values);
		float v = values[(idx + 1) % num_values];

		t += t_step;
		auto tp1 = gl::vec2(t, valueToY(v));
		auto pos1 = ImLerp(inner_tl, inner_br, tp1);
		drawList->AddLine(pos0, pos1, color);
		pos0 = pos1;
	}
}

// Simplified/optimized version of ImGui::PlotHistogram()
static void plotHistogram(std::span<const float> values, float scale_min, float scale_max, gl::vec2 outer_size)
{
	const auto& style = ImGui::GetStyle();

	gl::vec2 pos = ImGui::GetCursorScreenPos();
	auto outer_tl = pos;              // top-left
	auto outer_br = pos + outer_size; // bottom-right
	auto inner_tl = outer_tl + gl::vec2(style.FramePadding);
	auto inner_br = outer_br - gl::vec2(style.FramePadding);

	ImGui::RenderFrame(outer_tl, outer_br, ImGui::GetColorU32(ImGuiCol_FrameBg),
	                   true, style.FrameRounding);
	if (values.empty()) return;

	int num_values = narrow<int>(values.size());
	int inner_width = std::max(1, static_cast<int>(inner_br.x - inner_tl.x));
	int res_w = std::min(inner_width, num_values);

	float t_step = 1.0f / static_cast<float>(res_w);
	float scale_range = scale_max - scale_min;
	float inv_scale = (scale_range != 0.0f) ? (1.0f / scale_range) : 0.0f;

	auto valueToY = [&](float v) {
		return 1.0f - std::clamp((v - scale_min) * inv_scale, 0.0f, 1.0f);
	};
	float zero_line = (scale_min * scale_max < 0.0f)
	                ? (1 + scale_min * inv_scale)
	                : (scale_min < 0.0f ? 0.0f : 1.0f);

	auto color = ImGui::GetColorU32(ImGuiCol_PlotHistogram);

	float t0 = 0.0f;
	auto* drawList = ImGui::GetWindowDrawList();
	drawList->PrimReserve(6 * res_w, 4 * res_w);

	for (int n = 0; n < res_w; n++) {
		auto idx = static_cast<int>(t0 * float(num_values) + 0.5f);
		assert(0 <= idx && idx < num_values);
		float y0 = valueToY(values[idx]);
		float t1 = t0 + t_step;

		auto pos0 = ImLerp(inner_tl, inner_br, gl::vec2(t0, y0));
		auto pos1 = ImLerp(inner_tl, inner_br, gl::vec2(t1, zero_line));
		drawList->PrimRect(pos0, pos1, color);

		t0 = t1;
	}
}


void ImGuiWaveViewer::save(ImGuiTextBuffer& buf)
{
	savePersistent(buf, *this, persistentElements);
}

void ImGuiWaveViewer::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
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
		ranges::copy(buf, buf2);
		buf = buf2;
	} else {
		assert(buf.size() >= HALF_BAND_EXTRA);
		extended = allocate(std::max((buf.size() - HALF_BAND_EXTRA) / 2, size_t(fftLen)));
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
	ranges::fill(extended.subspan(buf.size()), 0.0f);
	auto result = extended.subspan(0, buf.size());
	return {result, extended, normalize, sampleRate};
}

static std::string freq2note(float freq)
{
	static constexpr auto a4_freq = 440.0f;
	static constexpr std::array<std::string_view, 12> names = {
		"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
	};

	auto n = int(std::lround(12.0f * fast_log2(freq / a4_freq))) + 9 + 4 * 12;
	if (n < 0) return ""; // these are below 20Hz, so inaudible
	auto note = n % 12;
	auto octave = n / 12;
	return strCat(names[note], octave);
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
		for (auto [s, w] : view::zip_equal(signal, window)) {
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

	simpleToolTip([&]() -> std::string {
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

		// format with "Hz" or "kHz" suffix and 3 significant digits
		auto freq = std::lround(sampleRate * 0.5f * mouseX);
		auto note = freq2note(float(freq));
		if (freq < 1000) {
			return strCat(freq, "Hz  ", note);
		} else {
			auto k = freq / 1000;
			auto t = (freq % 1000) / 10;
			char t1 = char(t / 10) + '0';
			char t2 = char(t % 10) + '0';
			return strCat(k, '.', t1, t2, "kHz  ", note);
		}
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
	auto [factorL_, factorR_] = device.getAmplificationFactor();
	auto factorL = factorL_; // pre-clang-16 workaround
	auto factorR = factorR_;
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
			if (!ImGui::CollapsingHeader(name.c_str())) continue;
			HelpMarker("Right-click column header to (un)hide columns.\n"
			           "Drag to reorder or resize columns.");

			int flags = ImGuiTableFlags_RowBg |
			            ImGuiTableFlags_BordersV |
			            ImGuiTableFlags_BordersOuter |
			            ImGuiTableFlags_Resizable |
			            ImGuiTableFlags_Reorderable |
			            ImGuiTableFlags_Hideable |
			            ImGuiTableFlags_SizingStretchProp;
			im::Table("##table", 5, flags, [&]{ // note: use the same id for all tables
				ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
				ImGui::TableSetupColumn("ch.", ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed);
				// TODO: make this work here, or add an alternative: simpleToolTip("channel number");
				ImGui::TableSetupColumn("mute", ImGuiTableColumnFlags_DefaultHide | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("VU-meter", 0, 1.0f);
				ImGui::TableSetupColumn("Waveform", 0, 2.0f);
				ImGui::TableSetupColumn("Spectrum", 0, 3.0f);
				ImGui::TableHeadersRow();
				im::ID(name, [&]{
					paintDevice(device, info.channelSettings);
				});
			});
		}
	});
}

} // namespace openmsx
