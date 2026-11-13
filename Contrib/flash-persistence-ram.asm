; Byte program, sector erase and DQ7/DQ5 status routines by Max Iwamoto.
; Execute entirely in page 3 RAM. Commands use Flash page 1; destination
; is in Flash page 2. Caller disables interrupts and uses a RAM stack.
; Test entry points: C200 program, C203 erase, C206 status.
        ORG     0C200h
        JP      _flash_program
        JP      _flash_erase
        JP      _check_flash_status

; HL = RAM source, DE = mapped Flash destination, BC = nonzero byte count.
; Carry clear = success, set = failure.
_flash_program:
        LD      A,0AAh
        LD      (04AAAh),A
        LD      A,055h
        LD      (04555h),A
        LD      A,0A0h
        LD      (04AAAh),A
        LD      A,(HL)
        LD      (DE),A
        CALL    _check_flash_status
        RET     C
        INC     HL
        INC     DE
        DEC     BC
        LD      A,B
        OR      C
        JR      NZ,_flash_program
        RET

_flash_erase:
        LD      HL,04AAAh
        LD      DE,04555h
        LD      BC,08030h
        LD      (HL),L
        LD      A,E
        LD      (DE),A
        LD      (HL),B
        LD      (HL),L
        LD      (DE),A
        LD      A,C
        LD      (BC),A
        LD      A,0FFh
        LD      D,B
        LD      E,C

_check_flash_status:
        PUSH    BC
        LD      C,A
.loop:
        LD      A,(DE)
        XOR     C
        JP      P,.exit
        XOR     C
        AND     020h
        JR      Z,.loop
        LD      A,(DE)
        XOR     C
        JP      P,.exit
        SCF
        LD      A,0F0h
        LD      (DE),A
.exit:
        POP     BC
        RET
