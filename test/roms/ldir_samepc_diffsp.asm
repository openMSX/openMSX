; MSX1 16 KiB cartridge ROM
; Same LDIR PC called from different call depths (different SP)
        ORG     $4000

        DEFB    "AB"
        DEFW    INIT
        DEFW    0
        DEFW    0
        DEFW    0

INIT:   DI
        ; First call
        CALL    LDIR_SUB
        ; Second call with different stack depth
        PUSH    HL
        PUSH    DE
        PUSH    BC
        CALL    LDIR_SUB
        POP     BC
        POP     DE
        POP     HL
FOREVER:
        JR      FOREVER

; Shared LDIR subroutine at same PC
LDIR_SUB:
        LD      HL,$4000
        LD      DE,$C000
        LD      BC,$0005        ; same BC
        LDIR
        RET

        DS      $8000-$, $FF
