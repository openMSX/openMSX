#ifndef HALFBAND_HH
#define HALFBAND_HH

#include <cassert>
#include <span>

// Remove the upper-half of the spectrum (low-pass filter)
// and then halve the sample-rate.
//
// The input must contain twice the number of output samples plus 29 extra.
//   in.size() == 2 * out.size() + 29
static constexpr size_t HALF_BAND_EXTRA = 29;
inline void halfBand(std::span<const float> in, std::span<float> out)
{
	// The low-pass filter is implemented via a 31-point FIR filter.
	//   [c1 0  c2 0  ... c7 0  c8  1  c8 0  c7 ... 0  c2 0  c1]
	// Note:
	// * The filter is symmetrical.
	// * The middle coefficient is 1.
	// * Several coefficients are 0.
	//  -> This allows for an efficient implementation (only 8 multiplications per output)
	// * The sum of the coefficients is 2.
	//  -> This implies an amplification by a factor 2. That's undesired,
	//     but saves 1 multiplication (and usually not a big problem).
	//
	// After filtering we drop every 2nd sample (decimate).
	// (obviously we don't need to compute the outputs that will be dropped).
	//
	// Schematic representation:
	//           x0  x1  x2  x3  x4  x5  x6  x7  x8  x9  x10 x11 x12 x13 x14 x15 x16 x17 x18 ...
	//     y0 = [c1   0  c2   0  c3   0  c4   0  c5   0  c6   0  c7   0  c8   1  c8   0  c7  ...
	//     y1 =         [c1   0  c2   0  c3   0  c4   0  c5   0  c6   0  c7   0  c8   1  c8  ...
	//     y2 =                 [c1   0  c2   0  c3   0  c4   0  c5   0  c6   0  c7   0  c8  ...

	// See script below for how to calculate these coefficients.
	static constexpr float c1 = -0.008743987728765f;
	static constexpr float c2 =  0.016581151738734f;
	static constexpr float c3 = -0.027946152941873f;
	static constexpr float c4 =  0.044437771332192f;
	static constexpr float c5 = -0.069397194777923f;
	static constexpr float c6 =  0.111547511726324f;
	static constexpr float c7 = -0.203192672267484f;
	static constexpr float c8 =  0.636713572918795f;

	size_t n = out.size();
	assert(in.size() == (2 * n + HALF_BAND_EXTRA));

	for (size_t i = 0; i < n; ++i) {
		out[i] =    in[2 * i + 15]
		    + c1 * (in[2 * i +  0] + in[2 * i + 30])
		    + c2 * (in[2 * i +  2] + in[2 * i + 28])
		    + c3 * (in[2 * i +  4] + in[2 * i + 26])
		    + c4 * (in[2 * i +  6] + in[2 * i + 24])
		    + c5 * (in[2 * i +  8] + in[2 * i + 22])
		    + c6 * (in[2 * i + 10] + in[2 * i + 20])
		    + c7 * (in[2 * i + 12] + in[2 * i + 18])
		    + c8 * (in[2 * i + 14] + in[2 * i + 16]);
	}
}

#endif

// The filter coefficients were obtained via this octave (matlab) script:
/* ----- 8< ----- 8< ----- 8< ----- 8< ----- 8< ----- 8< ----- 8< -----

pkg load signal

ntaps = 4 * 7 + 3;
beta = 3;
resolution = 1024;        % relates to the 'resolution' of the plot
samplefreq = 100;


N= ntaps-1;
n= -N/2:N/2;
sinc= sin(n*pi/2)./(n*pi+eps);      % truncated impulse response; eps= 2E-16
sinc(N/2 +1)= 1/2;                  % value for n --> 0
win= kaiser(ntaps, beta);           % window function
b= sinc.*win';
b(2:2:ntaps/2)=0;
b((ntaps+5)/2:2:ntaps)=0;

b2=b;
b2((ntaps+1)/2)=0;
b2 = b2 / (2 * sum(b2));
b2((ntaps+1)/2)=0.5;
b=b2;

b

x = b;
x(N+1:resolution) = 0;
y = fft(x);

xx = [0 : samplefreq/(resolution) : samplefreq/2];
plot(xx, 20 * log(abs(y(1:resolution/2+1))));
xlabel("frequency [kHz]");
ylabel("attenuation [dB]");

----- 8< ----- 8< ----- 8< ----- 8< ----- 8< ----- 8< ----- 8< ----- */
