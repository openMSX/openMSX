#include <cmath>
#include <iostream>

// This defines the constants
//   BLIP_SAMPLE_BITS, BLIP_PHASE_BITS, BLIP_IMPULSE_WIDTH
#include "BlipConfig.hh"

int main()
{
	static const int HALF_SIZE = BLIP_RES / 2 * (BLIP_IMPULSE_WIDTH - 1);
	double fimpulse[HALF_SIZE + 2 * BLIP_RES];
	double* out = &fimpulse[BLIP_RES];

	// generate sinc, apply hamming window
	double oversample = ((4.5 / (BLIP_IMPULSE_WIDTH - 1)) + 0.85);
	double to_angle = M_PI / (2.0 * oversample * BLIP_RES);
	double to_fraction = M_PI / (2 * (HALF_SIZE - 1));
	for (int i = 0; i < HALF_SIZE; ++i) {
		double angle = ((i - HALF_SIZE) * 2 + 1) * to_angle;
		out[i] = sin(angle) / angle;
		out[i] *= 0.54 - 0.46 * cos((2 * i + 1) * to_fraction);
	}

	// need mirror slightly past center for calculation
	for (int i = 0; i < BLIP_RES; ++i) {
		out[HALF_SIZE + i] = out[HALF_SIZE - 1 - i];
	}

	// starts at 0
	for (int i = 0; i < BLIP_RES; ++i) {
		fimpulse[i] = 0.0;
	}

	// find rescale factor
	double total = 0.0;
	for (int i = 0; i < HALF_SIZE; ++i) {
		total += out[i];
	}
	int kernelUnit = 1 << (BLIP_SAMPLE_BITS - 16);
	double rescale = kernelUnit / (2.0 * total);

	// integrate, first difference, rescale, convert to int
	static const int IMPULSES_SIZE = BLIP_RES * (BLIP_IMPULSE_WIDTH / 2) + 1;
	static int imp[IMPULSES_SIZE];
	double sum = 0.0;
	double next = 0.0;
	for (int i = 0; i < IMPULSES_SIZE; ++i) {
		imp[i] = int(floor((next - sum) * rescale + 0.5));
		sum += fimpulse[i];
		next += fimpulse[i + BLIP_RES];
	}

	// sum pairs for each phase and add error correction to end of first half
	for (int p = BLIP_RES; p-- >= BLIP_RES / 2; /* */) {
		int p2 = BLIP_RES - 2 - p;
		int error = kernelUnit;
		for (int i = 1; i < IMPULSES_SIZE; i += BLIP_RES) {
			error -= imp[i + p ];
			error -= imp[i + p2];
		}
		if (p == p2) {
			// phase = 0.5 impulse uses same half for both sides
			error /= 2;
		}
		imp[IMPULSES_SIZE - BLIP_RES + p] += error;
	}

	// dump header plus checking code
	std::cout << "// This is a generated file. DO NOT EDIT!\n";
	std::cout << "\n";
	std::cout << "// The table was generated for the following constants:\n";
	std::cout << "STATIC_ASSERT(BLIP_PHASE_BITS    == " <<
	                            BLIP_PHASE_BITS  << ");\n";
	std::cout << "STATIC_ASSERT(BLIP_SAMPLE_BITS   == " <<
	                            BLIP_SAMPLE_BITS << ");\n";
	std::cout << "STATIC_ASSERT(BLIP_IMPULSE_WIDTH == " <<
	                            BLIP_IMPULSE_WIDTH << ");\n";
	std::cout << "\n";

	// dump actual table: reshuffle values to a more cache friendly order
	std::cout << "static const int impulses[BLIP_RES][BLIP_IMPULSE_WIDTH] = {\n";
	for (int phase = 0; phase < BLIP_RES; ++phase) {
		const int* imp_fwd = &imp[BLIP_RES - phase];
		const int* imp_rev = &imp[phase];

		std::cout << "\t{ ";
		for (int i = 0; i < BLIP_IMPULSE_WIDTH / 2; ++i) {
			std::cout << imp_fwd[BLIP_RES * i] << ", ";
		}
		for (int i = BLIP_IMPULSE_WIDTH / 2 - 1; i > 0; --i) {
			std::cout << imp_rev[BLIP_RES * i] << ", ";
		}
		std::cout << imp_rev[BLIP_RES * 0] << " },\n";
	}
	std::cout << "};\n";
}
