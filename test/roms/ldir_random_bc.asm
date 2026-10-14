; MSX1 16 KiB cartridge ROM
; A long, unrolled series of LDIR runs with pseudo-random BC values,
; spanning single digits through the full 16-bit range (including 0,
; which means 65536 iterations, and $FFFF).
;
; Values generated with Python random.seed(20261029) and shuffled.
; Each LDIR is preceded by its own 'LD BC,nn' so the integration test
; can locate every run by scanning the ROM image, then use step_back
; from just after the run and check that BC is restored to that value.
;
; HL/DE are set once; later copies drift (and wrap) through the 64K
; space. The code lives in ROM, so writes to it are ignored, and the
; sequence uses no stack, so overwriting RAM is harmless.

        ORG     $4000

        DEFB    "AB"
        DEFW    INIT
        DEFW    0
        DEFW    0
        DEFW    0

INIT:   DI
        LD      HL,$4000        ; ROM source
        LD      DE,$C000        ; RAM destination

        LD      BC,$1AD5
        LDIR
        LD      BC,$0020
        LDIR
        LD      BC,$00BC
        LDIR
        LD      BC,$0004
        LDIR
        LD      BC,$0B8B
        LDIR
        LD      BC,$97DD
        LDIR
        LD      BC,$A803
        LDIR
        LD      BC,$A3D8
        LDIR
        LD      BC,$99DC
        LDIR
        LD      BC,$0008
        LDIR
        LD      BC,$0008
        LDIR
        LD      BC,$00EC
        LDIR
        LD      BC,$B2B7
        LDIR
        LD      BC,$035B
        LDIR
        LD      BC,$0003
        LDIR
        LD      BC,$C0C3
        LDIR
        LD      BC,$0006
        LDIR
        LD      BC,$0076
        LDIR
        LD      BC,$004F
        LDIR
        LD      BC,$8CE6
        LDIR
        LD      BC,$04B2
        LDIR
        LD      BC,$00BB
        LDIR
        LD      BC,$0F09
        LDIR
        LD      BC,$0EFB
        LDIR
        LD      BC,$1D61
        LDIR
        LD      BC,$004C
        LDIR
        LD      BC,$0009
        LDIR
        LD      BC,$0903
        LDIR
        LD      BC,$FFFF
        LDIR
        LD      BC,$9902
        LDIR
        LD      BC,$EC75
        LDIR
        LD      BC,$8F9C
        LDIR
        LD      BC,$0000
        LDIR
        LD      BC,$B848
        LDIR
        LD      BC,$2129
        LDIR
        LD      BC,$72FE
        LDIR
        LD      BC,$5AE6
        LDIR
        LD      BC,$3E6A
        LDIR
        LD      BC,$D1C5
        LDIR
        LD      BC,$9971
        LDIR
        LD      BC,$0008
        LDIR
        LD      BC,$30AA
        LDIR
        LD      BC,$07C7
        LDIR
        LD      BC,$2F48
        LDIR
        LD      BC,$C2F6
        LDIR
        LD      BC,$00D2
        LDIR
        LD      BC,$0005
        LDIR
        LD      BC,$0F9D
        LDIR

FOREVER:
        JR      FOREVER

        DS      $8000-$, $FF
