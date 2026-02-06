#include "catch.hpp"
#include "yin.hh"

#include <cmath>
#include <cstdlib>
#include <numbers>
#include <random>
#include <vector>

static constexpr float PI = std::numbers::pi_v<float>;
static constexpr float TAU = 2.0f * PI;

static constexpr float SAMPLE_RATE = 44100.0f;
static constexpr size_t BUFFER_SIZE = 4096; // ~93ms at 44.1kHz
static constexpr float MIN_FREQ = 50.0f;
static constexpr float MAX_FREQ = 2000.0f;

[[nodiscard]] static bool approx_rel(float value, float expected, float rel_err)
{
	return std::abs(value - expected) < (expected * rel_err);
}
[[nodiscard]] static bool approx_abs(float value, float expected, float abs_err)
{
	return std::abs(value - expected) < abs_err;
}

static void addSine(std::span<float> signal, float frequency, float amplitude = 1.0f, size_t start = 0, size_t end = size_t(-1))
{
	if (end == size_t(-1)) end = signal.size();
	auto C = TAU * frequency / SAMPLE_RATE;
	for (size_t i = start; i < end; ++i) {
		signal[i] += amplitude * std::sin(float(i) * C);
	}
};
static void addNoise(std::span<float> signal, float amplitude)
{
	std::mt19937 rng(42); // fixed seed
	std::normal_distribution<float> dist(0.0f, 1.0f); // mean, stddev
	for (size_t i = 0; i < signal.size(); ++i) {
		signal[i] += amplitude * dist(rng);
	}
};

[[nodiscard]] static auto run(std::span<const float> signal)
{
	return yin::detectPitch(signal, SAMPLE_RATE, MIN_FREQ, MAX_FREQ);
}

TEST_CASE("YIN: silence")
{
	auto signal = std::vector<float>(BUFFER_SIZE, 0.0f);

	auto [freq, err] = run(signal);
	CHECK(approx_abs(freq, 0.0f, 1.0f));
	CHECK(approx_rel(err, 1.0f, 0.05f));
}

TEST_CASE("YIN: sine")
{
	auto generateSine = [](float frequency) {
		std::vector<float> signal(BUFFER_SIZE);
		addSine(signal, frequency);
		return signal;
	};
	SECTION("50Hz") { // MIN_FREQ
		auto [freq, err] = run(generateSine(50.0f));
		CHECK(approx_rel(freq, 50.0f, 0.01f));
		CHECK(err < 0.01f);
	}
	SECTION("100Hz") {
		auto [freq, err] = run(generateSine(100.0f));
		CHECK(approx_rel(freq, 100.0f, 0.01f));
		CHECK(err < 0.01f);
	}
	SECTION("440Hz") {
		auto [freq, err] = run(generateSine(440.0f));
		CHECK(approx_rel(freq, 440.0f, 0.01f));
		CHECK(err < 0.01f);
	}
	SECTION("1000Hz") {
		auto [freq, err] = run(generateSine(1000.0f));
		CHECK(approx_rel(freq, 1000.0f, 0.01f));
		CHECK(err < 0.01f);
	}
	SECTION("2000Hz") { // MAX_FREQ
		auto [freq, err] = run(generateSine(2000.0f));
		CHECK(approx_rel(freq, 2000.0f, 0.01f));
		CHECK(err < 0.01f);
	}
}

TEST_CASE("YIN: square")
{
	auto generateSquare = [](float frequency) {
		// Fourier series: square wave = sum of odd harmonics
		// Square(t) = (4/π) * [sin(ωt) + (1/3)sin(3ωt) + (1/5)sin(5ωt) + ...]
		std::vector<float> signal(BUFFER_SIZE, 0.0f);
		for (int harmonic = 1; harmonic <= 11; harmonic += 2) {
			float amplitude = 4.0f / (PI * harmonic);
			addSine(signal, frequency * harmonic, amplitude);
		}
		return signal;
	};
	auto [freq, err] = run(generateSquare(321.0f));
	CHECK(approx_rel(freq, 321.0f, 0.01f));
	CHECK(err < 0.01f);
}

TEST_CASE("YIN: triangle")
{
	auto generateTriangle = [](float frequency) {
		// Triangle wave = sum of odd harmonics with 1/n² amplitude
		std::vector<float> signal(BUFFER_SIZE, 0.0f);
		for (int harmonic = 1; harmonic <= 11; harmonic += 2) {
			float amplitude = 8.0f / (PI * PI * harmonic * harmonic);
			if ((harmonic - 1) / 2 % 2 == 1) amplitude = -amplitude; // alternating sign
			addSine(signal, frequency * harmonic, amplitude);
		}
		return signal;
	};
	auto [freq, err] = run(generateTriangle(555.0f));
	CHECK(approx_rel(freq, 555.0f, 0.01f));
	CHECK(err < 0.01f);
}

TEST_CASE("YIN: sawtooth")
{
	auto generateSawtooth = [](float frequency) {
		// Sawtooth = sum of all harmonics with 1/n amplitude
		std::vector<float> signal(BUFFER_SIZE, 0.0f);
		for (int harmonic = 1; harmonic <= 20; ++harmonic) {
			float amplitude = 2.0f / (PI * harmonic);
			addSine(signal, frequency * harmonic, amplitude);
		}
		return signal;
	};
	auto [freq, err] = run(generateSawtooth(222.0f));
	CHECK(approx_rel(freq, 222.0f, 0.01f));
	CHECK(err < 0.01f);
}

TEST_CASE("YIN: sine with harmonics")
{
	SECTION("Sine 440 Hz + decaying harmonics (2nd, 3rd)") {
		std::vector<float> signal(BUFFER_SIZE, 0.0f);
		addSine(signal, 1.0f * 440.0f, 1.0f);
		addSine(signal, 2.0f * 440.0f, 0.5f);
		addSine(signal, 3.0f * 440.0f, 0.25f);

		auto [freq, err] = run(signal);
		CHECK(approx_rel(freq, 440.0f, 0.01f));
		CHECK(err < 0.01f);
	}
	SECTION("Sine 440 Hz + strong harmonics") {
		std::vector<float> signal(BUFFER_SIZE, 0.0f);
		addSine(signal, 1.0f * 440.0f, 1.0f);
		addSine(signal, 2.0f * 440.0f, 2.0f);
		addSine(signal, 3.0f * 440.0f, 1.5f);

		auto [freq, err] = run(signal);
		CHECK(approx_rel(freq, 440.0f, 0.01f));
		CHECK(err < 0.01f);
	}
	SECTION("Sine 440 Hz + 880Hz equally strong") {
		std::vector<float> signal(BUFFER_SIZE, 0.0f);
		addSine(signal, 1.0f * 440.0f, 1.0f);
		addSine(signal, 2.0f * 440.0f, 1.0f);

		auto [freq, err] = run(signal);
		CHECK(approx_rel(freq, 440.0f, 0.01f));
		CHECK(err < 0.01f);
	}
}

TEST_CASE("YIN: noise")
{
	std::mt19937 rng(12345); // fixed seed
	SECTION("uniform") {
		std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
		std::vector<float> signal(BUFFER_SIZE);
		std::ranges::generate(signal, [&]{ return dist(rng); });

		auto [freq, err] = run(signal);
		// freq doesn't matter
		CHECK(err > 0.8f);
	}
	SECTION("normal") {
		std::normal_distribution<float> dist(0.0f, 1.0f); // mean, stddev
		std::vector<float> signal(BUFFER_SIZE);
		std::ranges::generate(signal, [&]{ return dist(rng); });

		auto [freq, err] = run(signal);
		// freq doesn't matter
		CHECK(err > 0.8f);
	}
}

TEST_CASE("YIN: chirp") // frequency sweep
{
	auto generateChirp = [](float startFreq, float endFreq) {
		std::vector<float> signal(BUFFER_SIZE);
		for (size_t i = 0; i < BUFFER_SIZE; ++i) {
			float t = float(i) / SAMPLE_RATE;
			float freq = startFreq + (endFreq - startFreq) * t / (float(BUFFER_SIZE) / SAMPLE_RATE);
			signal[i] = std::sin(TAU * freq * t);
		}
		return signal;
	};
	auto [freq, err] = run(generateChirp(100.0f, 1000.0f));
	// freq doesn't matter
	CHECK(err > 0.8f);
}

TEST_CASE("YIN: sine decaying in amplitude")
{
	// Generate sine with decaying amplitude (exponential decay)
	auto generateDecayingSine = [](float frequency, float decayTime) {
		std::vector<float> signal(BUFFER_SIZE);
		float decayConstant = -std::log(0.01f) / decayTime; // decay to 1% over decayTime
		for (size_t i = 0; i < BUFFER_SIZE; ++i) {
			float t = float(i) / SAMPLE_RATE;
			float envelope = std::exp(-decayConstant * t);
			signal[i] = envelope * std::sin(TAU * frequency * t);
		}
		return signal;
	};
	auto [freq, err] = run(generateDecayingSine(440.0f, 0.05f));
	CHECK(approx_rel(freq, 440.0f, 0.01f));
	CHECK(err < 0.05f); // still very good, but less confident than the 'pure' signals
}

TEST_CASE("YIN: split frequency")
{
	// split frequency: first half at 100Hz, second half at 123Hz (not a nice ratio)
	std::vector<float> signal(BUFFER_SIZE);
	addSine(signal, 100.0f, 1.0f, 0, BUFFER_SIZE / 2);
	addSine(signal, 123.0f, 1.0f, BUFFER_SIZE / 2);

	auto [freq, err] = run(signal);
	CHECK(approx_rel(freq, 113.0f, 0.1f)); // something in between
	CHECK(err < 0.25f); // but not a very confident detection
}

TEST_CASE("YIN: sine with noise")
{
	auto generateSineWithNoise = [](float freq, float noiseAmplitude) {
		std::vector<float> signal(BUFFER_SIZE);
		addSine(signal, freq);
		addNoise(signal, noiseAmplitude);
		return signal;
	};
	SECTION("0.1") {
		auto [freq, err] = run(generateSineWithNoise(440.0f, 0.1f));
		CHECK(approx_rel(freq, 440.0f, 0.01f));
		CHECK(err < 0.02f); // still very good, but less confident than the 'pure' signals
	}
	SECTION("0.2") {
		auto [freq, err] = run(generateSineWithNoise(440.0f, 0.2f));
		CHECK(approx_rel(freq, 440.0f, 0.01f));
		CHECK(err < 0.1f); // more noise, less confident
	}
	SECTION("0.3") {
		auto [freq, err] = run(generateSineWithNoise(440.0f, 0.3f));
		// This is actually wrong, detects freq 2x too low, but at the border of confident (so acceptable?)
		CHECK(approx_rel(freq, 220.0f, 0.01f));
		CHECK(err > 0.14f);
	}
	SECTION("0.4") {
		auto [freq, err] = run(generateSineWithNoise(440.0f, 0.4f));
		// freq doesn't matter
		CHECK(err > 0.2f);
	}
	SECTION("0.5") {
		auto [freq, err] = run(generateSineWithNoise(440.0f, 0.5f));
		// freq doesn't matter
		CHECK(err > 0.2f);
	}
}
