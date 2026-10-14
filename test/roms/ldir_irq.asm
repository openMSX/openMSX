; MSX1 16 KiB cartridge ROM
; Designed for a 60 Hz NTSC MSX1 / Z80 at ~3.58 MHz.
; Synchronization sequence:
;   EI + HALT -> video IRQ
;   short NOP/DJNZ delay
;   LDIR with BC=$2000 (8192 bytes, ~49 ms) copying ROM to RAM
;
; Note: BC=$2000 is the initial counter value, so a valid step_back
; landing must keep BC at $2000.

        ORG     $4000

        DEFB    "AB"
        DEFW    INIT
        DEFW    0
        DEFW    0
        DEFW    0

INIT:   DI
        IM      1
        EI
        HALT                    ; synchronize to video IRQ
        CALL    DELAY           ; ~16 ms (so the next 60 Hz IRQ hits mid-LDIR)

        LD      HL,$4000        ; ROM source
        LD      DE,$C000        ; RAM destination
        LD      BC,$2000        ; 8192 bytes
LDIR_START:
        LDIR                    ; IRQ is intended to occur here
FOREVER:
        JR      FOREVER

DELAY:  LD      A,16            ; ~16 ms total (including cartridge-ROM wait states)
DELAY_OUTER:
        LD      B,200
DELAY_INNER:
        NOP
        DJNZ    DELAY_INNER
        DEC     A
        JR      NZ,DELAY_OUTER
        RET

        DS      $8000-$, $FF
