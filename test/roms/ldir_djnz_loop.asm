; MSX1 16 KiB cartridge ROM
; LDIR inside a DJNZ loop (outer loop counter preserved around LDIR,
; since DJNZ uses B while LDIR uses BC).
;   DI (no IRQ interruption) so each LDIR run completes then DJNZ loops.
; Exercises step_back's handling of a block-repeat instruction that is
; re-executed many times at the same PC (earlier loop passes).

        ORG     $4000

        DEFB    "AB"
        DEFW    INIT
        DEFW    0
        DEFW    0
        DEFW    0

INIT:   DI
        IM      1
        LD      B,4             ; outer DJNZ counter: 4 passes
LOOP:   PUSH    BC              ; preserve outer counter while LDIR uses BC
        LD      HL,$4000        ; source (ROM)
        LD      DE,$C000        ; destination (RAM)
        LD      BC,$2000        ; 8192 bytes per pass
LDIR_START:
        LDIR
        POP     BC              ; restore outer counter
        DJNZ    LOOP
FOREVER:
        JR      FOREVER

        DS      $8000-$, $FF
