#include "makoto-reference/ReferenceYM2608.hh"
// Verify raw voice sums, including deliberately overdriven FM output.
#include "makoto-reference/MakotoNativeChip.hh"
#include <algorithm>
#include <cstdlib>
#include <iostream>

struct Interface : ymfm::ymfm_interface {};

int main()
{
    for (unsigned level : {40U, 0U}) {
        Interface intf;
        openmsx::MakotoNativeChip chip(intf);
        std::array<int32_t, 32> voices{};
        chip.set_channel_output(voices.data());
        chip.reset();
        auto write = [&](unsigned reg, uint8_t value) {
            unsigned port = reg >= 256 ? 2 : 0;
            chip.write(port, uint8_t(reg));
            chip.write(port + 1, value);
        };
        write(0x29, 0x80);
        for (unsigned channel = 0; channel < 6; ++channel) {
            unsigned reg = (channel / 3) * 256 + channel % 3;
            for (unsigned slot : {0U, 4U, 8U, 12U}) {
                write(reg + slot + 0x30, 1);
                write(reg + slot + 0x40, uint8_t(level));
                write(reg + slot + 0x50, 31);
                write(reg + slot + 0x60, 0);
                write(reg + slot + 0x70, 0);
                write(reg + slot + 0x80, 15);
            }
            write(reg + 0xb0, 7);
            write(reg + 0xb4, 0xc0);
            write(reg + 0xa4, 0x22);
            write(reg + 0xa0, 0x69);
            write(0x28, uint8_t(0xf0 + (channel / 3) * 4 + channel % 3));
        }
        unsigned overrange = 0;
        int peak = 0;
        for (unsigned sample = 0; sample < 100000; ++sample) {
            const auto output = chip.clockFM();
            for (unsigned side = 0; side < 2; ++side) {
                int total = 0;
                for (unsigned channel = 0; channel < 16; ++channel)
                    total += voices[2 * channel + side];
                if (output[side] != total) return 1;
                peak = std::max(peak, std::abs(total));
                overrange += total < -32768 || total > 32767;
            }
        }
        std::cout << "TL=" << level << " peak=" << peak
                  << " overrange_sides=" << overrange << '\n';
        if ((level == 0) != (overrange != 0)) return 2;
    }
}
