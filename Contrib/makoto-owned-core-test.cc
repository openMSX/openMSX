// Register-driven FM/rhythm/ADPCM differential fixture. Build twice: with the
// pre-experiment MakotoYM2608 wrapper (-DMAKOTO_REFERENCE), and current engines.
// SSG waveform/level validation belongs to the full-emulator tests.
#ifdef MAKOTO_REFERENCE
#include "MakotoYM2608.hh"
#else
#include "ymfm_fm.h"
#include "ymfm_adpcm.h"
#endif
#include "sound/YM2608AdpcmRom.hh"
#include <array>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

struct Interface : ymfm::ymfm_interface {
#ifdef MAKOTO_REFERENCE
    void timer() { m_engine->engine_timer_expired(0); }
#endif
    uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override {
        return type == ymfm::ACCESS_ADPCM_A ? openmsx::YM2608_ADPCM_ROM[address & 8191]
            : uint8_t((address * 73 + (address >> 4) * 19) ^ 0x5a);
    }
};

#ifdef MAKOTO_REFERENCE
struct Chip {
    Interface intf;
    openmsx::MakotoYM2608 core{intf};
    void reset() { core.reset(); }
    void write(unsigned reg, uint8_t value) { core.writeRegister(uint16_t(reg), value); }
    void generate(std::array<float*, 13>& buffers, unsigned n) { core.generateFM(buffers, n); }
    void timer() { intf.timer(); }
    uint8_t status() { return core.peek(2, false) & 0x2c; }
};
#else
struct Chip {
    Interface intf;
    ymfm::fm_engine_base fm{intf};
    ymfm::adpcm_a_engine a{intf};
    ymfm::adpcm_b_engine b{intf};
    uint8_t mask = 0x1f;
    void reset() {
        fm.reset(); a.reset(); b.reset(); mask = 0x1f;
        constexpr std::array starts{0x0000, 0x01c0, 0x0440, 0x1b80, 0x1d00, 0x1f80};
        constexpr std::array ends{0x01bf, 0x043f, 0x1b7f, 0x1cff, 0x1f7f, 0x1fff};
        for (unsigned c = 0; c < 6; ++c) a.set_start_end(c, starts[c], ends[c]);
    }
    void write(unsigned reg, uint8_t value) {
        if (reg >= 0x10 && reg < 0x20) a.write(reg & 15, value);
        else if (reg >= 0x100 && reg < 0x110) b.write(reg & 15, value);
        else if (reg == 0x29) mask = value;
        else if (reg == 0x110) {} // Fixture observes unmasked raw ADPCM status.
        else fm.write(uint16_t(reg), value);
    }
    void generate(std::array<float*, 13>& buffers, unsigned n) {
        if (b.silent()) buffers[6] = nullptr;
        for (unsigned c = 0; c < 6; ++c) if (a.silent(c)) buffers[c+7] = nullptr;
        uint32_t env = fm.envelope_counter();
        fm.generate(std::span<float*, 6>(buffers.data(), 6), n, (mask & 128) ? 63 : 7);
        b.generate(buffers[6], n);
        a.generate(std::span<float*, 6>(buffers.data()+7, 6), n, env);
    }
    void timer() { fm.engine_timer_expired(0); }
    uint8_t status() {
        auto s = b.status();
        return uint8_t(((s & 1) ? 4 : 0) | ((s & 2) ? 8 : 0) | ((s & 4) ? 32 : 0));
    }
};
#endif

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) { std::cerr << "Usage: engine-render output.bin block-size [fm|rhythm|adpcm]\n"; return 2; }
    unsigned block = unsigned(std::stoul(argv[2]));
    if (block < 1 || block > 4096) return 2;
    std::ofstream out(argv[1], std::ios::binary);
    if (!out) return 2;
    Chip chip;
    chip.reset();
    auto write = [&](unsigned r, unsigned v) { chip.write(r, uint8_t(v)); };
    write(0x29, 0x9f); write(0x110, 0);
    for (unsigned c = 0; c < 6; ++c) {
        unsigned r = (c/3)*256+c%3;
        for (unsigned s : {0U, 4U, 8U, 12U}) {
            write(r+s+0x30, 1); write(r+s+0x40, 20);
            write(r+s+0x50, 31); write(r+s+0x60, 3);
            write(r+s+0x70, 0); write(r+s+0x80, 15);
        }
        write(r+0xb0, 0x38 | c); write(r+0xb4, 0xc0 | 0x37);
        write(r+0xa4, 0x22); write(r+0xa0, 0x60+c*10);
        write(0x28, 0xf0+(c/3)*4+c%3);
    }
    write(0x11, 63);
    for (unsigned c = 0; c < 6; ++c) write(0x18+c, 0xdf);
    write(0x10, 0x3f);
    write(0x101, 0xc0); write(0x102, 0); write(0x103, 0);
    write(0x104, 0x7f); write(0x105, 0); write(0x10c, 0xff); write(0x10d, 0xff);
    write(0x109, 0x76); write(0x10a, 0x35); write(0x10b, 0xff); write(0x100, 0xb0);
    std::array<bool, 13> heard{};
    std::array<std::vector<float>, 13> voice;
    for (auto& v : voice) v.resize(block*2);
    constexpr unsigned phaseSamples = 12347;
    constexpr unsigned phases = 20;
    for (unsigned phase = 0; phase < phases; ++phase) {
        // Writes always occur at identical sample edges, independently of block size.
        switch (phase) {
            case 1: write(0x22, 0x0f); break; // enabled LFO, all FM algorithms
            case 2: write(0x101, 0); write(0xb4, 0); break; // closed pan still advances
            case 3: write(0x101, 0xc0); write(0xb4, 0xf7); break;
            case 4: write(0x10b, 0); write(0x11, 0); break; // level-zero still advances
            case 5: write(0x10b, 255); write(0x11, 63); write(0x10, 63); break;
            case 6: write(0x100, 0xa0); break; // non-repeating ADPCM EOS
            case 7: write(0x100, 1); write(0x109, 0); write(0x10a, 0); write(0x100, 0xa0); break;
            case 8: write(0x109, 0xff); write(0x10a, 0xff); break;
            case 9: for (unsigned c=0;c<6;++c) write(0x28,(c/3)*4+c%3); write(0x10, 0xbf); break;
            case 10: break; // entirely quiet/release tails
            case 11: for (unsigned c=0;c<6;++c) write(0x28,0xf0+(c/3)*4+c%3); write(0x10,63); break;
            case 12: write(0x29, 0x1f); break; // three-channel compatibility mode
            case 13: write(0x29, 0x9f); break;
            case 14: write(0x22, 0); write(0x100, 1); write(0x100, 0x80); break; // CPU-fed ADPCM
            case 15: write(0x108, 0x7f); write(0x10b, 0); break;
            case 16: write(0x108, 0xa5); write(0x10b, 255); write(0x101, 0x80); break;
            case 17: write(0x101, 0x40); write(0x10, 63); break;
            case 18:
                for (unsigned c=0;c<6;++c) write(0x28,(c/3)*4+c%3);
                write(0x24, 0xff); write(0x25, 3); write(0x27, 0x85);
                break; // Timed CSM pulses, independent of rendering block size.
            case 19: write(0x27, 0); write(0x28, 0xf2); break;
        }
        if (phase == 2 && argc == 4) {
            std::string control = argv[3];
            if (control == "fm") write(0x28, 0);
            else if (control == "rhythm") write(0x10, 0xbf);
            else if (control == "adpcm") write(0x100, 1);
            else return 2;
        }
        for (unsigned offset = 0; offset < phaseSamples;) {
            unsigned n = std::min(block, phaseSamples-offset);
            if (phase == 18) {
                if (offset % 97 == 0) chip.timer();
                n = std::min(n, 97-offset%97);
            }
            std::array<float*, 13> buffers;
            for (unsigned c=0;c<13;++c) { std::fill(voice[c].begin(),voice[c].end(),0.0f); buffers[c]=voice[c].data(); }
            chip.generate(buffers,n);
            for (unsigned i=0;i<n;++i) for (unsigned c=0;c<13;++c) {
                for (unsigned side=0;side<2;++side) {
                    int32_t v=int32_t(voice[c][i*2+side]);
                    heard[c] = heard[c] || v != 0;
                    out.write(reinterpret_cast<const char*>(&v), sizeof(v));
                }
            }
            offset += n;
        }
        uint8_t status = chip.status();
        // Fixed-width record keeps phase status independently comparable.
        int32_t statusRecord = status;
        out.write(reinterpret_cast<const char*>(&statusRecord), sizeof(statusRecord));
    }
    for (unsigned c=0;c<13;++c) if (!heard[c]) { std::cerr << "Unexercised voice " << c << '\n'; return 3; }
    std::cout << "13 voices; " << phaseSamples*phases << " stereo source frames; block=" << block << '\n';
}
