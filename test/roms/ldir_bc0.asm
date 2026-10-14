; MSX1 16 KiB cartridge ROM
; LDIR with BC=0 (65536 iterations)
        ORG     $4000

        DEFB    "AB"
        DEFW    INIT
        DEFW    0
        DEFW    0
        DEFW    0

INIT:   DI
        LD      HL,$4000        ; source (ROM)
        LD      DE,$C000        ; destination (RAM)
        LD      BC,0            ; 65536 iterations
LDIR_START:
        LDIR
FOREVER:
        JR      FOREVER

        DS      $8000-$, $FF
