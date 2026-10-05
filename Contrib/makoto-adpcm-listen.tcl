# Self-contained Makoto ADPCM listening cases. No music/sample download needed.
# Start openMSX with -ext Makoto -script Contrib/makoto-adpcm-listen.tcl,
# then enter "makoto_adpcm_test 1" in the console (F10).
# Cases 1..6: bass drum, snare, cymbal, hi-hat, tom, rim shot (ADPCM-A).
# Case 7: synthetic 500 Hz RAM sample once (ADPCM-B); 8: repeat until stopped.
# Optional second argument: stereo, left, right. Case 0 stops both engines.
# The script does not start playback until explicitly invoked.

namespace eval makoto_adpcm {
    variable names {stop {bass drum} snare cymbal hi-hat tom {rim shot} {RAM sample once} {RAM sample repeat}}
    variable sample {}

    proc write {reg value} {
        set port [expr {$reg >= 256 ? 0x16 : 0x14}]
        debug write ioports $port [expr {$reg & 255}]
        debug write ioports [expr {$port + 1}] $value
    }

    # Encode an original sine wave with the YM2608 ADPCM-B quantizer.
    # 4096 samples at 16 kHz: 256 ms, exactly 128 cycles at 500 Hz.
    proc make_sample {} {
        variable sample
        if {[llength $sample]} { return }
        set accumulator 0
        set step 127
        set byte 0
        for {set i 0} {$i < 4096} {incr i} {
            set target [expr {round(8000 * sin(2 * acos(-1) * $i / 32))}]
            set sign [expr {$target < $accumulator ? 8 : 0}]
            set best 0
            set error 1000000
            for {set magnitude 0} {$magnitude < 8} {incr magnitude} {
                set delta [expr {(2 * $magnitude + 1) * $step / 8}]
                set value [expr {$accumulator + ($sign ? -$delta : $delta)}]
                set distance [expr {abs($target - $value)}]
                if {$distance < $error} {set error $distance; set best $magnitude}
            }
            set delta [expr {(2 * $best + 1) * $step / 8}]
            set accumulator [expr {$accumulator + ($sign ? -$delta : $delta)}]
            if {$accumulator < -32768} {set accumulator -32768}
            if {$accumulator > 32767} {set accumulator 32767}
            set factor [lindex {57 57 57 57 77 102 128 153} $best]
            set step [expr {$step * $factor / 64}]
            if {$step < 127} {set step 127}
            if {$step > 24576} {set step 24576}
            set nibble [expr {$sign | $best}]
            if {$i & 1} {
                lappend sample [expr {$byte | $nibble}]
            } else {
                set byte [expr {$nibble << 4}]
            }
        }
    }

    proc play {number {pan stereo}} {
        variable names
        variable sample
        if {![string is integer -strict $number] || $number < 0 || $number > 8} {
            error {Use makoto_adpcm_test 0..8 ?stereo|left|right?}
        }
        switch -- $pan {
            stereo {set mask 0xc0}
            left {set mask 0x80}
            right {set mask 0x40}
            default {error {Pan must be stereo, left or right}}
        }
        if {![info exists ::Makoto_volume]} {ext Makoto}
        write 0x10 0xbf
        write 0x100 1
        if {$number == 0} {return stopped}
        # Isolate the test from FM/SSG notes on this cartridge.
        foreach c {0 1 2 4 5 6} {write 0x28 $c}
        foreach reg {8 9 10} {write $reg 0}
        if {$number <= 6} {
            write 0x11 63
            write [expr {0x18 + $number - 1}] [expr {$mask | 31}]
            write 0x10 [expr {1 << ($number - 1)}]
        } else {
            make_sample
            write 0x101 0xc0
            write 0x102 0
            write 0x103 0
            write 0x104 0xff
            write 0x105 1
            write 0x10c 0xff
            write 0x10d 0xff
            write 0x100 0x60
            foreach value $sample {write 0x108 $value}
            write 0x100 1
            write 0x101 $mask
            write 0x109 0xba
            write 0x10a 0x49
            write 0x10b 0xff
            write 0x110 0
            write 0x100 [expr {$number == 8 ? 0xb0 : 0xa0}]
        }
        return "Case $number: [lindex $names $number], $pan"
    }
}

proc makoto_adpcm_test {number {pan stereo}} {
    makoto_adpcm::play $number $pan
}
