; MSX1 16 KiB cartridge ROM
; Outer LDIR (BC=$2000) interrupted by 60 Hz video IRQs whose H.TIMI hook
; runs its own INIR block (B=4 reads of the VDP status port). The outer
; block is therefore suspended and resumed several times, AND a nested
; block executes inside its handler at a different PC.
; Exercises step_back's handling of nested block-repeat executions: the
; inner INIR markers must not disturb rewinding the outer LDIR to before
; its first iteration (a single PC slot would log a spurious outer entry
; on resume and land mid-block instead).

        ORG     $4000

        DEFB    "AB"
        DEFW    INIT
        DEFW    0
        DEFW    0
        DEFW    0

INIT:   DI
        IM      1
        LD      A,$C3            ; install H.TIMI hook: JP HOOK
        LD      ($FD9F),A
        LD      HL,HOOK
        LD      ($FDA0),HL
        EI
        HALT                    ; synchronize to video IRQ
        CALL    DELAY           ; ~16 ms (so IRQs hit mid-LDIR)

        LD      HL,$4000        ; ROM source
        LD      DE,$C000        ; RAM destination
        LD      BC,$2000        ; 8192 bytes (~49 ms, several IRQs land inside)
LDIR_START:
        LDIR                    ; IRQs (with nested INIR) occur here
FOREVER:
        JR      FOREVER

; H.TIMI hook: runs on every video IRQ, including mid-LDIR. Preserves
; AF/BC/HL (the BIOS preserves the rest around the hook call).
HOOK:   PUSH    AF
        PUSH    BC
        PUSH    HL
        LD      HL,$C100        ; scratch RAM sink
        LD      B,4
        LD      C,$99           ; VDP status port (reads have no side effects)
        INIR                    ; the nested block-repeat instruction
        POP     HL
        POP     BC
        POP     AF
        RET

DELAY:  LD      A,16
DELAY_OUTER:
        LD      B,200
DELAY_INNER:
        NOP
        DJNZ    DELAY_INNER
        DEC     A
        JR      NZ,DELAY_OUTER
        RET

        DS      $8000-$, $FF
