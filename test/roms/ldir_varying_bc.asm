; MSX1 16 KiB cartridge ROM
; Shared LDIR subroutine (COPY) called with a DIFFERENT BC from three
; call sites (5, then 3, then 4).
;   DI (no IRQ interruption) for determinism: each run is exactly the
; requested number of iterations, back to back with only CALL/RET/LD
; overhead between runs.
; Exercises step_back's handling of one block-repeat PC executed several
; times with different initial counters: rewinding the last execution
; must land on ITS first iteration (BC=4), not on an earlier execution
; (BC=5 or BC=3).

        ORG     $4000

        DEFB    "AB"
        DEFW    INIT
        DEFW    0
        DEFW    0
        DEFW    0

INIT:   DI
        IM      1
        LD      HL,$4000        ; source (ROM)
        LD      DE,$C000        ; destination (RAM)
        LD      BC,5
        CALL    COPY            ; first execution, 5 iterations
        LD      BC,3
        CALL    COPY            ; second execution, 3 iterations
        LD      BC,4
        CALL    COPY            ; third (current) execution, 4 iterations
FOREVER:
        JR      FOREVER

COPY:   LDIR                    ; the shared block-repeat instruction
        RET

        DS      $8000-$, $FF
