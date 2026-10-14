; MSX1 16 KiB cartridge ROM
; Designed for a 60 Hz NTSC MSX1 / Z80 at ~3.58 MHz.
; Synchronization sequence:
;   EI + HALT -> video IRQ
;   short NOP/DJNZ delay
;   OTIR with B=0 (256 iterations) writing to the PSG (slow port)
;   The next 60 Hz IRQ should therefore occur while OTIR is executing.
;
; Note: B-only (the counter is B, C holds the fixed I/O port), so this
; exercises the B-only counter case.

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
        CALL    DELAY           ; short delay

        LD      HL,BUF          ; output source
        LD      B,0             ; B=0 -> 256 OTIR iterations
        LD      C,$A0           ; PSG register-select port
OTIR_START:
        OTIR                    ; IRQ is intended to occur here
FOREVER:
        JR      FOREVER

DELAY:  LD      A,1
DELAY_OUTER:
        LD      B,64
DELAY_INNER:
        NOP
        DJNZ    DELAY_INNER
        DEC     A
        JR      NZ,DELAY_OUTER
        RET

BUF:    DEFB    $FF            ; any byte value, output repeatedly
        DS      $8000-$, $FF
