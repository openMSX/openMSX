; MSX1 16 KiB cartridge ROM
; CPIR that matches early
        ORG     $4000

        DEFB    "AB"
        DEFW    INIT
        DEFW    0
        DEFW    0
        DEFW    0

INIT:   DI
        LD      A,$42          ; search for this value
        LD      HL,$4100        ; search area
        LD      BC,$1000        ; search up to 4096 bytes
        ; Put $42 somewhere in the search range
        LD      A,$42
        CPIR                   ; will match early
FOREVER:
        JR      FOREVER

        DS      $8000-$, $FF
