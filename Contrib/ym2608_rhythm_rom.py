#!/usr/bin/env python3
"""Build a YM2608 rhythm ROM image from six WAV files.

The YM2608 (OPNA) plays its six rhythm sounds from a ROM inside the chip.
openMSX does not come with that ROM. This script makes a substitute: it
encodes one WAV file per sound to ADPCM-A and puts each one where the chip
expects it.

    python3 ym2608_rhythm_rom.py --bd bd.wav --sd sd.wav --top top.wav \\
        --hh hh.wav --tom tom.wav --rim rim.wav -o ym2608_adpcm_rom.bin

Put the result in the same directory as Makoto.xml (share/extensions in the
openMSX installation). openMSX finds it there by its file name. A
'systemroms' directory does not work: openMSX looks files up there by sha1,
and only an image of the real chip's ROM has the sha1 in Makoto.xml. Expect
a warning that the sha1 does not match.

Each sound has a fixed place and a fixed length in the ROM, so it has a
fixed duration at its playback rate:

    sound            rate       duration
    bass drum (bd)   18519 Hz    48 ms
    snare (sd)       18519 Hz    69 ms
    top cymbal (top) 18519 Hz   643 ms
    hi-hat (hh)      18519 Hz    41 ms
    tom (tom)         9259 Hz   138 ms
    rim shot (rim)    9259 Hz    28 ms

The WAV files may be 8, 16, 24 or 32 bit PCM, mono or stereo, at any rate.
Stereo is mixed to mono and the sound is resampled to the rate above. A
file at the target rate (within 1 Hz) is used as is. A shorter sound is
padded with silence; a longer one is cut and faded out over its last 64
samples. Only the standard library is used.

This script is distributed under the same license as openMSX (GNU GPL v2).
"""

import argparse
import math
import struct
import sys
import wave

CLOCK = 8_000_000
# One ADPCM-A step every 3 FM samples (clock / 144 / 3). The tom and the rim
# shot take a step on every other one of those.
FULL_RATE = CLOCK / 144 / 3
HALF_RATE = FULL_RATE / 2

# (option, name, first byte, last byte, rate), in ROM order. The chip sets
# these addresses itself on reset.
SOUNDS = [
    ('bd',  'bass drum',  0x0000, 0x01bf, FULL_RATE),
    ('sd',  'snare drum', 0x01c0, 0x043f, FULL_RATE),
    ('top', 'top cymbal', 0x0440, 0x1b7f, FULL_RATE),
    ('hh',  'hi-hat',     0x1b80, 0x1cff, FULL_RATE),
    ('tom', 'tom',        0x1d00, 0x1f7f, HALF_RATE),
    ('rim', 'rim shot',   0x1f80, 0x1fff, HALF_RATE),
]
ROM_SIZE = 0x2000

STEPS = [
     16,  17,   19,   21,   23,   25,   28,
     31,  34,   37,   41,   45,   50,   55,
     60,  66,   73,   80,   88,   97,  107,
    118, 130,  143,  157,  173,  190,  209,
    230, 253,  279,  307,  337,  371,  408,
    449, 494,  544,  598,  658,  724,  796,
    876, 963, 1060, 1166, 1282, 1411, 1552,
]
STEP_INC = [-1, -1, -1, -1, 2, 5, 7, 9]


def decode_step(acc, index, code):
    """One ADPCM-A decoder step, as the chip does it.

    acc is the 12-bit accumulator (0..4095, two's complement), index the
    position in STEPS. Returns the new (acc, index).
    """
    delta = (2 * (code & 7) + 1) * STEPS[index] // 8
    if code & 8:
        delta = -delta
    acc = (acc + delta) & 0xfff
    index = min(max(index + STEP_INC[code & 7], 0), 48)
    return acc, index


def signed12(acc):
    return acc - 0x1000 if acc & 0x800 else acc


def encode(samples):
    """Encode 12-bit samples; returns the list of 4-bit codes.

    Each code is chosen by trying all 16 of them, together with the best code
    for the sample after it, and keeping the pair with the smallest squared
    error. A sound that the chip itself produced comes out unchanged.
    """
    acc, index = 0, 0
    codes = []
    for n, target in enumerate(samples):
        following = samples[n + 1] if n + 1 < len(samples) else None
        best = None
        for code in range(16):
            a, i = decode_step(acc, index, code)
            cost = (signed12(a) - target) ** 2
            if following is not None:
                cost += min((signed12(decode_step(a, i, c)[0]) - following) ** 2
                            for c in range(16))
            if best is None or cost < best[0]:
                best = (cost, code, a, i)
        _, code, acc, index = best
        codes.append(code)
    return codes


def read_wav(path):
    """Returns (rate, mono samples as floats in -1 .. 1)."""
    with wave.open(path, 'rb') as w:
        if w.getcomptype() != 'NONE':
            sys.exit(f'{path}: only uncompressed PCM is supported')
        channels = w.getnchannels()
        width = w.getsampwidth()
        rate = w.getframerate()
        raw = w.readframes(w.getnframes())
    if width == 1:
        values = [(b - 128) / 128 for b in raw]
    elif width in (2, 3, 4):
        scale = float(1 << (8 * width - 1))
        values = [int.from_bytes(raw[i:i + width], 'little', signed=True) / scale
                  for i in range(0, len(raw), width)]
    else:
        sys.exit(f'{path}: unsupported sample width ({width} bytes)')
    mono = [sum(values[i:i + channels]) / channels
            for i in range(0, len(values), channels)]
    return rate, mono


def resample(samples, src_rate, dst_rate):
    """Windowed-sinc resampling; also low-pass filters when going down."""
    if abs(src_rate - dst_rate) < 1:
        return list(samples)
    ratio = dst_rate / src_rate
    cutoff = min(1.0, ratio)
    taps = 32
    width = taps / cutoff
    out = []
    n_out = round(len(samples) * ratio)
    for n in range(n_out):
        center = n / ratio
        lo = max(0, int(math.floor(center - width)))
        hi = min(len(samples) - 1, int(math.ceil(center + width)))
        acc = 0.0
        for k in range(lo, hi + 1):
            x = (k - center) * cutoff
            if abs(x) >= taps:
                continue
            sinc = 1.0 if x == 0 else math.sin(math.pi * x) / (math.pi * x)
            window = 0.5 + 0.5 * math.cos(math.pi * x / taps)
            acc += samples[k] * sinc * window
        out.append(acc * cutoff)
    return out


def fit(samples, length, name):
    if len(samples) >= length:
        if len(samples) > length:
            print(f'  {name}: cut from {len(samples)} to {length} samples')
            samples = samples[:length]
            fade = min(64, length)
            for i in range(fade):
                samples[length - fade + i] *= (fade - 1 - i) / fade
        return samples
    print(f'  {name}: padded from {len(samples)} to {length} samples')
    return samples + [0.0] * (length - len(samples))


def main():
    parser = argparse.ArgumentParser(
        description='Build a YM2608 rhythm ROM image from six WAV files.')
    for opt, name, *_ in SOUNDS:
        parser.add_argument(f'--{opt}', required=True, metavar='WAV',
                            help=f'the {name}')
    parser.add_argument('-o', '--output', default='ym2608_adpcm_rom.bin',
                        help='output file (default: %(default)s)')
    args = parser.parse_args()

    rom = bytearray(ROM_SIZE)
    for opt, name, first, last, rate in SOUNDS:
        src_rate, samples = read_wav(getattr(args, opt))
        samples = resample(samples, src_rate, rate)
        length = 2 * (last - first + 1)
        samples = fit(samples, length, name)
        clipped = sum(1 for s in samples if abs(s) > 1.0)
        if clipped:
            print(f'  {name}: {clipped} samples clipped')
        targets = [max(-2048, min(2047, round(s * 2048))) for s in samples]
        codes = encode(targets)
        for i in range(0, length, 2):
            # The high nibble is played first.
            rom[first + i // 2] = (codes[i] << 4) | codes[i + 1]
    with open(args.output, 'wb') as f:
        f.write(rom)
    print(f'wrote {args.output} ({ROM_SIZE} bytes)')


if __name__ == '__main__':
    main()
