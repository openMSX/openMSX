; HINTZ80.COM: switches a turbo R to Z80 mode, then runs HINTTOBL.COM
; (appended) at 100h. On other machines it just runs HINTTOBL.
; HINTTOBL.COM is grauw's test program from openMSX issue #1310
; (https://github.com/openMSX/openMSX/files/5748689/bl-tests.zip); put
; hinttobl.com next to this file before assembling.
CALSLT	equ	001Ch
EXPTBL	equ	0FCC1h
	output	"HINTZ80.COM"
	org	100h
	ld	a,(EXPTBL)	; main ROM slot
	ld	hl,002Dh	; MSX version
	call	000Ch		; RDSLT
	cp	3
	jr	c,.mv
	ld	a,80h		; CHGCPU: Z80
	ld	iy,(EXPTBL - 1)
	ld	ix,0180h
	call	CALSLT
.mv:	ld	hl,mover	; the mover to 0C000h
	ld	de,0C000h
	ld	bc,moverend - mover
	ldir
	jp	0C000h
mover:	ld	hl,payload
	ld	de,100h
	ld	bc,payloadend - payload
	ldir
	jp	100h
moverend:
payload:
	incbin	"hinttobl.com"
payloadend:
