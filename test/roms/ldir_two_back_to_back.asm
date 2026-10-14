; MSX1 16 KiB cartridge ROM
; Two LDIRs back-to-back
        ORG     $4000

        DEFB    "AB"
        DEFW    INIT
        DEFW    0
        DEFW    0
        DEFW    0

INIT:   DI
        ; First LDIR
        LD      HL,$4000
        LD      DE,$C000
        LD      BC,$0010
        LDIR
        ; Second LDIR
        LD      HL,$4010
        LD      DE,$C010
        LD      BC,$0008
        LDIR
FOREVER:
        JR      FOREVER

        DS      $8000-$, $FF
