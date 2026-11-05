; MSX1 16 KiB cartridge ROM
; A simple test to check if setting BC = 1 in the step_back function
; causes confusion.

        ORG     $4000

        DEFB    "AB"
        DEFW    INIT
        DEFW    0
        DEFW    0
        DEFW    0

INIT:   DI

        LD      HL,$B0ED        ; RAM source
        LD      DE,$C000        ; RAM destination
        LD      BC,$0001        ; size
        LDIR

FOREVER:
        JR      FOREVER

        DS      $8000-$, $FF
