; VDPCE4 (MSX2 and up, V9938/V9958): VDPCE2 with three options (FLAGS):
; bit 2: R#46 = 00h (STOP) after the coarse delay, so the read comes t
; cycles after that STOP; bit 1: MODE 0 counts TR (S#2 bit 7) instead of
; CE; bit 0: STOP after the read, for commands that wait for the CPU.
; In SCREEN 5 with the display off (regular slots). Each trial:
; - MODE 2: VRAM byte VADDR = 00h (CPU write), then the read address
;   VADDR - 1 (its read-ahead takes place now); MODE 3: a POINT on REF, so
;   that S#7 holds REF's colour,
; - write R#32-R#45 from REGS (through R#17), select S#2, wait for CE = 0,
;   (S#2 selected),
; - sync on HR's falling edge (S#2 bit 5); MODE 3: select S#7; send the
;   first byte of the R#46 write (the command, REGS+14),
; - OUT (99h),A with 80h+46: the command starts; then a delay of COARSE
;   (optional) plus t = i cycles, then:
;   MODE 0: IN A,(99h): class 0 (CE = 0) or 1 (CE = 1);
;   MODE 2: IN A,(98h): the read-ahead of VADDR starts; after CE = 0 the
;   next IN A,(98h) gives that byte: class 0 (still 00h) or 1;
;   MODE 3: IN A,(99h) reads S#7: class 1 if it is EXPECT, else 0,
; - count it at i: the IN starts 12 + i cycles after the start of the OUT
;   that starts the command (+ COARSE's). i = 1, 2, 3, 4 and 6 can't be
;   made and get no trials.
; The delay code is built once per run in BUF, ending at a fixed tail; per
; trial only its start (OUT, COARSE, the extra instruction) moves.
; Memory at CNTA: 2 arrays of NI 16-bit counters (class 0, 1).
; Cycle counts include the MSX M1 wait state (+1 per M1 cycle).

CHGCPU	equ	0180h
GETCPU	equ	0183h
MSXVER	equ	002Dh
NI	equ	320		; i = 0..319
NCLASS	equ	2
CNTA	equ	0A000h		; NCLASS arrays of NI 16-bit counters
BUF	equ	0B000h		; NOPs, then the tail at TAIL
TAIL	equ	BUF + 96

	output	"VDPCE4.BIN"
	org	0C000h - 7
	db	0FEh
	dw	start, end, start

start:	jp	main
reps:	db	20		; C003h: repetitions per i
mode:	db	0		; C004h: 0 = read CE, 2 = when the write lands, 3 = POINT's S#7
coarse:	db	0		; C005h: 0, or n: LD L,n / DEC L / JR NZ (8 + 18n - 5 cycles)
expect:	db	0		; C006h: MODE 3: the colour POINT reads
	db	0		; C007h: unused
done:	db	0		; C008h: set to FFh by VDPCE4.BAS when all runs are done
flags:	db	0		; C009h: see above
	ds	6
regs:	ds	15		; C010h: R#32-R#46: SX, SY, DX, DY, NX, NY, CLR, ARG, CMD
vaddr:	db	0, 0, 0		; C01Fh: MODE 2: R#14, address bits 0-7, bits 8-13
ref:	dw	0, 0		; C022h: MODE 3: SX, SY of the reference pixel
oldcpu:	db	0

main:	ld	a,(MSXVER)
	cp	3
	jr	c,.notr
	call	GETCPU
	ld	(oldcpu),a
	ld	a,80h		; Z80 mode, update the LED
	call	CHGCPU
.notr:	di
	call	vinit
	ld	hl,CNTA		; clear the counters
	ld	de,CNTA + 1
	ld	bc,2 * NI * NCLASS - 1
	ld	(hl),0
	ldir
	ld	hl,BUF		; NOPs up to the tail
	ld	de,BUF + 1
	ld	bc,TAIL - BUF - 1
	ld	(hl),0
	ldir
	ld	hl,tail0	; the tail for MODE
	ld	a,(mode)
	cp	2
	jr	c,.t
	ld	hl,tail2
	jr	z,.t
	ld	hl,tail3
.t:	ld	de,TAIL
	ld	bc,7
	ldir
	ld	a,(reps)
	ld	(repcnt),a
.rep:	ld	hl,0
	ld	(ival),hl
	xor	a		; t = 0
	ld	(tr),a
	ld	(tq),a
.pos:	ld	a,(tq)		; t possible? r = 2, 3, 4 need q >= 1, r = 1 q >= 2
	ld	b,a
	ld	a,(tr)
	or	a
	jr	z,.ok
	cp	1
	jr	nz,.q1
	ld	a,b
	cp	2
	jr	c,.skip
	jr	.ok
.q1:	ld	a,b
	or	a
	jr	z,.skip
.ok:	call	build
	call	trial		; A = class
	ld	hl,CNTA		; HL = CNTA + A * 2 * NI + 2 * i
	or	a
	jr	z,.cls
	ld	hl,CNTA + 2 * NI
.cls:	ld	de,(ival)
	add	hl,de
	add	hl,de
	inc	(hl)
	jr	nz,.next
	inc	hl
	inc	(hl)
.next:
.skip:	ld	hl,(ival)
	inc	hl
	ld	(ival),hl
	ld	a,(tr)		; t + 1: r + 1, past 4 to 0 with q + 1
	inc	a
	cp	5
	jr	c,.r
	ld	a,(tq)
	inc	a
	ld	(tq),a
	xor	a
.r:	ld	(tr),a
	ld	de,NI
	or	a
	sbc	hl,de
	jr	nz,.pos
	ld	hl,repcnt
	dec	(hl)
	jr	nz,.rep
	xor	a		; select S#0 again
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	ld	a,(MSXVER)
	cp	3
	jr	c,.notr2
	ld	a,(oldcpu)
	or	80h
	call	CHGCPU
.notr2:	ei
	ret

; the timed code for t = 5 * TQ + TR (TR = t mod 5): it starts at START1,
; OUT (99h),A [LD L,n / DEC L / JR NZ,-3] [extra] NOP * q, then the tail;
; extra: 0, 7 (INC HL), 8 (LD B,0), 9 (DJNZ not taken, B = 1) or 11
; (LD A,I) for TR = 0, 2, 3, 4, 1, taking 0, 1, 1, 1, 2 of the NOPs
build:	ld	hl,(start1)	; the previous start back to NOPs, up to TAIL
	ld	b,13
.clr:	ld	a,l
	cp	low TAIL
	jr	z,.cd
	ld	(hl),0
	inc	hl
	djnz	.clr
.cd:
	ld	a,(tr)		; HL = the extra instruction, B = its length,
	ld	c,a		; C = the NOPs it replaces
	add	a,a
	add	a,a
	ld	e,a
	ld	d,0
	ld	hl,extras
	add	hl,de
	ld	b,(hl)
	inc	hl
	ld	a,(hl)
	inc	hl
	push	hl
	ld	hl,(tq)		; L = q
	sub	l		; A = -(q - replaced) = -NOPs
	neg
	ld	e,a		; DE = TAIL - NOPs - extra length
	ld	d,0
	ld	hl,TAIL
	or	a
	sbc	hl,de
	ld	e,b
	sbc	hl,de
	ld	a,(coarse)	; - 5 for COARSE
	or	a
	jr	z,.nc
	ld	de,5
	sbc	hl,de
.nc:	ld	a,(flags)	; - 4 for the STOP
	and	4
	jr	z,.ns
	ld	de,4
	or	a
	sbc	hl,de
.ns:	dec	hl		; - 2 for the OUT
	dec	hl
	ld	(start1),hl
	ld	(hl),0D3h	; OUT (99h),A
	inc	hl
	ld	(hl),99h
	inc	hl
	ld	a,(coarse)
	or	a
	jr	z,.nc2
	ld	(hl),2Eh	; LD L,n
	inc	hl
	ld	(hl),a
	inc	hl
	ld	(hl),2Dh	; DEC L
	inc	hl
	ld	(hl),20h	; JR NZ,-3
	inc	hl
	ld	(hl),0FDh
	inc	hl
.nc2:	ld	a,(flags)	; OUT (C),D / OUT (C),E: R#46 = 00h, STOP
	and	4
	jr	z,.ns2
	ld	(hl),0EDh
	inc	hl
	ld	(hl),51h
	inc	hl
	ld	(hl),0EDh
	inc	hl
	ld	(hl),59h
	inc	hl
.ns2:	ex	de,hl		; the extra instruction
	pop	hl
	ld	a,b
	or	a
	ret	z
	ld	c,b
	ld	b,0
	ldir
	ret
extras:	db	0, 0, 0, 0	; TR = 0: none
	db	2, 2, 0EDh, 57h	; TR = 1: LD A,I (11)
	db	1, 1, 23h, 0	; TR = 2: INC HL (7)
	db	2, 1, 06h, 0	; TR = 3: LD B,0 (8)
	db	2, 1, 10h, 0	; TR = 4: DJNZ +0 (9)

tail0:	db	0DBh, 99h	; IN A,(99h) / JP rd (7 bytes)
	db	0C3h
	dw	rd
	db	0, 0
tail2:	db	0DBh, 98h	; IN A,(98h) / JP rd2
	db	0C3h
	dw	rd2
	db	0, 0
tail3:	db	0DBh, 99h	; IN A,(99h) / JP rd3
	db	0C3h
	dw	rd3
	db	0, 0

; one trial; returns A = class
trial:	call	cmdset		; (selects S#2)
.h2:	in	a,(99h)		; wait until HR = 1
	and	20h
	jr	z,.h2
.h3:	in	a,(99h)		; wait until HR = 0
	and	20h
	jr	nz,.h3
	ld	a,(mode)
	cp	3
	jr	nz,.s
	ld	a,7		; MODE 3: select S#7
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
.s:	ld	a,(regs + 14)	; first byte of the R#46 write: the command
	out	(99h),a
	ld	de,80h + 46	; D = 00h, E = 80h + 46: the STOP, through port C
	ld	c,99h
	ld	b,1		; for the DJNZ
	ld	a,80h + 46
	ld	hl,(start1)
	jp	(hl)

rd:	ld	b,a		; MODE 0: CE, or TR (FLAGS bit 1)
	ld	a,(flags)
	and	2
	ld	a,b
	jr	z,.ce
	rlca			; TR: bit 7
.ce:	and	1
	ld	(rdbk),a
	ld	a,(flags)	; STOP after the read (FLAGS bit 0)
	and	1
	jr	z,.w
	xor	a
	out	(99h),a
	ld	a,80h + 46
	out	(99h),a
.w:	call	cewait
	ld	a,(rdbk)
	ret

rd2:	ld	a,(flags)	; MODE 2: STOP first (FLAGS bit 0)
	and	1
	jr	z,.w
	xor	a
	out	(99h),a
	ld	a,80h + 46
	out	(99h),a
.w:	call	cewait		; the byte read ahead at VADDR
	in	a,(98h)
	or	a
	ret	z
	ld	a,1
	ret

rd3:	ld	b,a		; MODE 3: S#7
	ld	a,2		; select S#2 again
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	call	cewait
	ld	a,(expect)
	cp	b
	ld	a,1
	ret	z
	xor	a
	ret

; wait until CE = 0 (S#2 is selected)
cewait:	in	a,(99h)
	rra
	jr	c,cewait
	ret

; before a trial: MODE 2: VADDR = 00h; MODE 3: POINT on REF; then
; R#32-R#45 from REGS (through R#17), select S#2, wait for CE = 0
cmdset:	ld	a,2		; select S#2
	out	(99h),a
	ld	a,8Fh
	out	(99h),a
	ld	a,(mode)
	cp	2
	jr	nz,.m3
	ld	a,(vaddr)	; R#14
	out	(99h),a
	ld	a,80h + 14
	out	(99h),a
	ld	a,(vaddr + 1)
	out	(99h),a
	ld	a,(vaddr + 2)
	or	40h		; write
	out	(99h),a
	xor	a
	out	(98h),a
	ld	hl,(vaddr + 1)	; read address VADDR - 1 (R#14 stays)
	dec	hl
	ld	a,l
	out	(99h),a
	ld	a,h
	and	3Fh
	out	(99h),a
	jr	.regs
.m3:	cp	3
	jr	nz,.regs
	ld	a,32		; MODE 3: POINT on REF
	out	(99h),a
	ld	a,80h + 17
	out	(99h),a
	ld	hl,ref
	ld	bc,4 * 256 + 9Bh
	otir
	ld	a,40h
	out	(99h),a
	ld	a,80h + 46
	out	(99h),a
	call	cewait
.regs:	ld	a,32		; R#17 = 32, auto-increment
	out	(99h),a
	ld	a,80h + 17
	out	(99h),a
	ld	hl,regs
	ld	bc,14 * 256 + 9Bh
	otir
	jp	cewait

; VRAM contents the commands read (CPU writes, once per run):
; row 400 (C800h-C87Fh) AAh: the copies' source; row 410 (CD00h): 00h,
; 00h, 50h, 00h: SRCH finds colour 5 at dot 4; (0,411) (CD80h) C0h and
; (0,412) (CE00h) 30h: POINT's pixel (colour 12) and the reference (3)
vinit:	ld	hl,vtab
.e:	ld	a,(hl)		; R#14, address low, high, value, count
	cp	0FFh
	ret	z
	out	(99h),a
	ld	a,80h + 14
	out	(99h),a
	inc	hl
	ld	a,(hl)
	out	(99h),a
	inc	hl
	ld	a,(hl)
	or	40h
	out	(99h),a
	inc	hl
	ld	a,(hl)
	inc	hl
	ld	b,(hl)
	inc	hl
.w:	out	(98h),a
	push	af		; (slow enough for any slot)
	pop	af
	djnz	.w
	jr	.e
vtab:	db	3, 00h, 08h, 0AAh, 128
	db	3, 00h, 0Dh, 00h, 2
	db	3, 02h, 0Dh, 50h, 1
	db	3, 03h, 0Dh, 00h, 1
	db	3, 80h, 0Dh, 0C0h, 1
	db	3, 00h, 0Eh, 30h, 1
	db	0FFh

repcnt:	db	0
rdbk:	db	0
ival:	dw	0
tr:	db	0
tq:	db	0, 0
start1:	dw	TAIL - 13
end:
