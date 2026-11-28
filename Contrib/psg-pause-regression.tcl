# Run an assertions-enabled build (OPENMSX_FLAVOUR=devel):
#   openmsx -machine C-BIOS_MSX2 -script Contrib/psg-pause-regression.tcl
# C-BIOS is bundled; no other ROM is needed. Use the SDL sound driver
# (SDL's dummy audio driver is fine if no audio device is available).
# Exits 0 on success; an assertion or a recording whose length doesn't
# match emulated time fails the test.

namespace eval psg_pause_regression {
    variable cycles 0
    variable pausedAt
    variable start
    variable wav {}

    proc cleanup {} {
        variable wav
        if {$wav ne {}} { file delete -- $wav }
    }

    proc checked {callback} {
        if {[catch {$callback} message]} {
            puts stderr "PSG pause regression: $message"
            cleanup
            exit 1
        }
    }

    proc psg {reg value} {
        debug write ioports 0xa0 $reg
        debug write ioports 0xa1 $value
    }

    proc start_tone {} {
        # Steady tone on channel A, with no noise or envelope.
        foreach {reg value} {0 112 1 0 7 0x3e 8 15} { psg $reg $value }
        after time 0.0613 [namespace code {checked pause_next}]
    }

    proc pause_next {} {
        variable cycles
        variable pausedAt
        variable start
        variable wav
        if {$cycles == 100} {
            # The first 100 pauses exercise ordinary playback, separately
            # from the synchronous mode used during the next 20 pauses.
            close [file tempfile temp]
            set wav ${temp}.wav
            file delete -- $temp
            set start [machine_info time]
            record start -audioonly -mono $wav
        } elseif {$cycles == 120} {
            record stop
            set elapsed [expr {[machine_info time] - $start}]
            set file [open $wav rb]
            set header [read $file 44]
            close $file
            if {[binary scan $header @24i@40i rate bytes] != 2 || $rate <= 0 || $bytes <= 0} {
                error "invalid or empty WAV"
            }
            set samples [expr {$bytes / 2}]
            set expected [expr {$elapsed * $rate}]
            if {abs($samples - $expected) > 1.0} {
                error [format {WAV has %d samples; expected %.1f} $samples $expected]
            }
            cleanup
            puts stderr "PSG pause regression passed (120 pauses, $samples recorded samples)"
            exit
        }
        set pausedAt [machine_info time]
        set ::pause on
        after realtime 0.004 [namespace code {checked resume}]
    }

    proc resume {} {
        variable cycles
        variable pausedAt
        if {[machine_info time] != $pausedAt} { error "emulated time advanced during pause" }
        set ::pause off
        incr cycles
        # Vary the offset relative to the audio buffer boundaries.
        after time [expr {0.0061 + 0.00371 * ($cycles % 17)}] [namespace code {checked pause_next}]
    }

    proc setup {} {
        set ::save_settings_on_exit off
        set ::sound_driver sdl
        set ::mute off
        set ::master_volume 75
        set ::frequency 44100
        set ::samples 1024
        set ::throttle on
        set ::speed 100
        set ::resampler hq
        set ::pause off
        after time 0.75 [namespace code {checked start_tone}]
    }

    checked setup
}
