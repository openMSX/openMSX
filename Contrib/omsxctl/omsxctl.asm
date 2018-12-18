;------------------------------------
; MSXDOS openMSX controller program ;
;------------------------------------
DOS 				.equ	0x5
D_PUTCHR			.equ	0x2
D_ARG_SIZE 			.equ	0x0080
D_ARG_DATA 			.equ	0x0081
SWIO_DEVICE 		.equ	0x40
SWIO_DEVICE_ID 		.equ	  30
SWIO_WR_PUTCHR 		.equ	0x41
SWIO_WR_EXECUTE 	.equ	0x42
SWIO_WR_CLEAR	 	.equ	0x43
SWIO_WR_RESP_IDX 	.equ	0x44
SWIO_RD_STATUS 		.equ	0x41
SWIO_RD_RESPONSE 	.equ	0x42
SWIO_STATUS_IDLE 	.equ	0xFF

.area	_CODE
	JP		RUN

	.db		0x0D	; type omsxctl.com
	.db		0x0D,0x0A
	.str	"Embedded openMSX control interface."
	.db		0x0D,0x0A
	.db		0x1A	; end txt

RUN:
	CALL	CHK_DEV
	OR		A
	JR		Z,RUN_DEV	; 0 = dev found
	LD		HL,#TXT_ERR_NO_DEVICE
	JP		PUT_TXT
RUN_DEV:
	LD		A,(#D_ARG_SIZE)
	OR		A
	JR		NZ,RUN_ARG	; 0 = no args
	LD		HL,#TXT_ERR_NO_ARG
	JP		PUT_TXT

RUN_ARG:
	LD		B,A
	LD		HL,#D_ARG_DATA
	INC		HL	; skip first space
	DEC		B
RUN_TX:
	LD		A,(HL)
	OUT		(#SWIO_WR_PUTCHR),A
	INC		HL
	DEC		B
	JR		NZ,RUN_TX

	OUT		(#SWIO_WR_EXECUTE),A	; flag execution
RUN_RD:
	IN		A,(#SWIO_RD_STATUS)
	CP		#SWIO_STATUS_IDLE
	RET		Z ; finished reading

	IN		A,(#SWIO_RD_RESPONSE)
	LD		E,A
	LD  	C,#D_PUTCHR
	CALL 	DOS
	JR		RUN_RD


;------------------------------------
; Sub routines                      ;
;------------------------------------


; result A = zero then device
CHK_DEV:
    LD		B,#1
    LD		A,#SWIO_DEVICE_ID
    OUT		(#SWIO_DEVICE),A	; set dev id
    IN		A,(#SWIO_DEVICE)
    CPL
    CP		#SWIO_DEVICE_ID ; compare complement value
    JR		NZ,CHK_DEV_ERR	; no device found
    LD		B,#0
CHK_DEV_ERR:
	LD		A,B
	RET

PUT_TXT:
	LD		A,(HL)
	CP		#0x1D
	RET		Z
	LD		E,A
	LD		C,#D_PUTCHR
	PUSH	HL
	CALL	DOS
	POP		HL
	INC		HL
	JR		PUT_TXT

TXT_ERR_NO_ARG:
	.str	"No arguments given."
	.db		0x1D

TXT_ERR_NO_DEVICE:
	.str	"No openMSX commanddevice found."
	.db		0x1D

.area	_DATA
