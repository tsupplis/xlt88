; BALLOON.ASM
; -----------
; By Tony Estep
; The source code listing apepared in Byte Magazine, Nov 79, p. 152
;
; SOL-20 -- BALLOON
;
; Source code reconstructed by:
;
; Mr. Emmanuel ROCHE
; 17 rue Gutenberg
; 17430 Tonnay-Charente
; FRANCE
;
;--------------------------------
	ORG	0100H		; Standard CP/M COMmand file
;--------------------------------
; Zero Page location used.

WarmS	EQU	0000H		; Warm Start

;--------------------------------
; Screen dimensions used.

row	EQU	16
col	EQU	64

;--------------------------------
; PT VDM Screen locations used.

VDMBAS	EQU	0CC00H		; VDM (video) Base Address
MIDL	EQU	0CE1EH		; Middle of line
ClScn	EQU	0C0D5H		; Clear screen routine
ScBot	EQU	0CFC0H		; Middle of last line

;--------------------------------
; SOL-20 keys used.

LAR	EQU	81H		; Left  ARrow
RARr	EQU	93H		; Right ARrow
UAR	EQU	97H		; Up    ARrow
DAR	EQU	9AH		; Down  ARrow

;--------------------------------

	LXI	SP,stack+46
	CALL	ClScn		; Clear screen
	LXI	H,VDMBAS

	MVI	M,' '
begin:	CALL	wait

go:	XRA	A		; Initialize flags
	STA	G1
	STA	Flg1
	STA	MSCR
	STA	PSCR

	MVI	A,1
	STA	BFLG1
	STA	RAFL
	STA	STRTF

	CALL	ClScn		; Clear screen

	LXI	H,VDMBAS
	MVI	M,' '
	LXI	H,MIDL
	SHLD	CORNR
	CALL	SHIP
	CALL	DELAY
	CALL	DELAY
	CALL	DELAY
	CALL	DELAY
	CALL	DELAY
	CALL	DELAY
	CALL	DELAY

; Main loop starts here.

RunIt:	CALL	TakOf		; Put blanks in ship image
	CALL	SHIP		; Put it on screen
	LHLD	LR
	PUSH	H
	LHLD	UD
	PUSH	H
	LHLD	CORNR
	POP	B
	DAD	B
	POP	D
	DAD	D
	SHLD	CORNR		; The location for ship
	CALL	PutOn		; Put graphix in ship image
	CALL	SHIP		; Put it on screen
	CALL	KyChk		; Check keyboard
	CALL	TopB		; Check to see if we are at top or bottom of screen
	CALL	BlnCh		; Is a balloon dropping?
	CALL	DELAY
	CALL	Pea1
	CALL	PeaCh		; Is there a shooter on screen?
	CALL	PeaOf		; Take it off, maybe

	XRA	A
	STA	STRTF		; ???
SCORIT:	CALL	score		; Update score
	JMP	RunIt

;--------------------------------
SHIP:	LHLD	CORNR		; Moves memory image of ship
	CALL	hit		; 

	LDA	Lend
	MOV	M,A
	INX	H
	CALL	hit

	LDA	blak
	MOV	M,A
	INX	H
	CALL	hit

	LDA	whit
	MOV	M,A
	INX	H
	CALL	hit

	LDA	Rend
	MOV	M,A
	RET

;--------------------------------
PutOn:
	MVI	A,10H		; The graphics which make up the ship
	STA	whit		; are made into a picture in memory
	MVI	A,90H
	STA	blak
	MVI	A,'<'
	STA	Lend
	MVI	A,'>'
	STA	Rend
	RET

;--------------------------------
TakOf:	MVI	A,' '		; Replaces ship graphics with blanks
	STA	whit		; so that 'ship' routine will blank
	STA	blak		; out picture of ship
	STA	Lend
	STA	Rend
	RET

;--------------------------------
; These input routines are for SOL.

status:
	IN	0FAH		; ?
	CMA
	ANI	01H		; ?
	RET

inp:	IN	0FCH		; ?
	RET

;--------------------------------
KyChk: 				; SOL-20 Keyboard check.
	CALL	status
	RZ
	CALL	inp
	CPI	RARr		; = Right Arrow
	JZ	right
	CPI	LAR		; = Left Arrow
	JZ	left
	CPI	UAR		; = Up Arrow
	JZ	up
	CPI	DAR		; = Down Arrow
	JZ	down
	CPI	' '		; Space drops balloon
	JZ	BlnSet
	RET

;--------------------------------
; These routines update the offsets to the ship positions.

right:	LHLD	LR
	LXI	D,1
	DAD	D
	SHLD	LR
	RET

left:	LHLD	LR
	LXI	D,-1
	DAD	D
	SHLD	LR
	RET

up:	LHLD	UD
	LXI	D,-col		; 64 character wide screen, making it
	DAD	D		; go up one line
	SHLD	UD
	RET

down:	LHLD	UD
	LXI	D,col		; down one line
	DAD	D
	SHLD	UD
	RET

Baln:	MVI	A,1
	STA	Blnf
	LHLD	CORNR
	LXI	D,col+1		; down and right one
	DAD	D
	SHLD	Blnr

bln1:	LHLD	Blnr		; blank out balloon
	MVI	M,' '
	LXI	D,col		; move it down a line
	DAD	D
	SHLD	Blnr
	MVI	M,8CH		; ?
	MOV	A,H
	CPI	0D0H		; ?
	JZ	Bdwn		; hit bottom
	RET

Bdwn:	MVI	A,0
	STA	Blnd
	STA	Blnf
	RET

BlnSet:	MVI	A,1
	STA	Blnd
	RET

BlnCh:	LDA	Blnf
	CPI	1
	JZ	Bln1
	LDA	Blnd
	CPI	1
	RNZ
	CALL	Baln

DELAY:	PUSH	H		; A useful all-purpose timing routine.
	LHLD	speed
	XCHG
dela1:	DCR	D
	JNZ	dela1
	DCR	E
	JNZ	dela1
	POP	H
	RET

;--------------------------------
wait:	LXI	H,VDMBAS+474	; 474 = 7*64 + 26
	LXI	D,msg		; '** BALLOON **'
	CALL	print
;
	LXI	H,VDMBAS+532	; 532 = 8*64 + 20
	LXI	D,msg2		; 'Copyright 1979 Tony Estep'
	CALL	print
;
	LXI	H,VDMBAS+976	; 976 = 15*64 + 16
	LXI	D,msg1		; '**SET SPEED, 1 THRU 4, 1=FASTEST **'
	CALL	print
;
in1:	CALL	status
	JZ	in1
	CALL	inp
	CPI	'0'
	JZ	WarmS		; Reboot CP/M
	CPI	'1'
	JZ	fast
	CPI	'2'
	JZ	med
	CPI	'3'
	JZ	slow
	CPI	'4'
	JZ	spastic
	JMP	wait		; Got a bad char

;--------------------------------
; Here, we set parameters for delay loop.

fast:	LXI	H,0019H		; = ?
	SHLD	speed
	RET

med:	LXI	H,0024H		; = ?
	SHLD	speed
	RET

slow:	LXI	H,0032H		; = ?
	SHLD	speed
	RET

spastic:
	LXI	H,0038H		; = ?
	SHLD	speed
	RET

;--------------------------------
; Did we hit the top or bottom of screen?

TopB:	LHLD	CORNR
	MOV	A,H
	CPI	0CCH		; Top 2 digits of VDMBAS
	JZ	Top
	CPI	0CFH		; Bottom of screen
	JZ	Bot
	RET

Top:	LXI	H,col
	SHLD	UD
	RET

Bot:	LXI	H,-col
	SHLD	UD
	RET

LR:	DW	0000H		; left/right
UD:	DW	0000H		; up/down
CORNR:	DW	0CE1EH		; Starts at middle of screen

whit:	DB	10H		; ship graphics
blak:	DB	90H
Lend:	DB	3CH
Rend:	DB	3EH

Blnd:	DB	00H
Blnf:	DB	00H
Blnr:	DW	0000H

;--------------------------------

Pea1:	LDA	G1
	CPI	1
	JZ	shot1
	CALL	rnd
	SUI	0F0H		; ?
	RC
	ADD	A
	ADD	A
	MOV	E,A
	MVI	D,0
	LXI	H,ScBot		; Middle of bottom of screen
	DAD	D
	SHLD	Ploc1
	MVI	M,18H		; ?
	INX	H
	MVI	M,18H		; ?
	INX	H
	MVI	M,18H		; ?
	LXI	D,-col-1	; up and left one
	DAD	D
	MVI	M,96H		; ?
	LXI	D,-col
	DAD	D
	DAD	D
	SHLD	Py1
	MVI	A,1
	STA	G1
	XRA	A
	STA	BFLG1
	CALL	OFF1
	RET

;--------------------------------

shot1:	CALL	JetOn
;
	CALL	ON1
;
	CALL	Rnd4
;
	CPI	1
	JZ	Jet1
;
	CPI	2
	JZ	Jet1
;
temp:	LHLD	Py1
	SHLD	BL1
	MVI	A,1
	STA	Flg1
	CALL	Rnd4
;
	MOV	E,A
	MVI	D,0
	LXI	H,-col-5
	DAD	D
	SHLD	Incr1
SHB1:	LHLD	Incr1
	PUSH	H
	POP	D
	LHLD	BL1
	MVI	M,' '
	LXI	B,col
	DAD	B
	MVI	M,' '
	LHLD	BL1
	DAD	D
	MOV	A,H
	CPI	0CBH		; Missile is off top of screen
	JZ	OFF1
;
	MVI	M,07H		; ?
	SHLD	BL1
	LXI	D,col
	DAD	D
	MVI	M,0AH		; 10 ?
	RET

;--------------------------------

ON1:	LDA	Flg1
	CPI	1
	RNZ
	POP	PSW
	JMP	SHB1

OFF1:	MVI	A,0
	STA	Flg1
	RET

Flg1:	DB	00H
Incr1:	DW	0000H
Py1:	DW	0000H
G1:	DB	00H

;--------------------------------
Rnd:
; A random num routine which does
; not repeat for 40,000 tries...
	LXI	H,RndM
	XCHG
	LXI	H,Rnd1
	MOV	A,M
	INR	A
	RRC
	MOV	B,A
	LDAX	D
	RLC
	ADD	B
	MOV	M,A
	MOV	A,B
	STAX	D
	RET

Rnd4:	CALL	Rnd
	RAR
	RAR
	ANI	07H		; value is 0-7
	ADI	01H		; value is 1-8
	RET

RndM:	DW	0000H
Rnd1:	DW	0000H

;--------------------------------

SheL1:	JMP	temp

;--------------------------------

BL1:	DW	ScBot

PeaCh:	LDA	BFLG1
	CPI	1
	RZ
;
	LDA	STRTF
	CPI	1
	RZ
;
	LHLD	Ploc1
	MOV	A,M
	CPI	' '
	JZ	XplD1
;
	INX	H
	MOV	A,M
	CPI	' '
	JZ	XplD1
;
	INX	H
	MOV	A,M
	CPI	' '
	JZ	XplD1
	RET

;--------------------------------
XplD1:

; A very dumb-looking explosion.

	SHLD	Blow
	MVI	A,1
	STA	BFLG1

; These are characters which indicate a hit.

	MVI	A,'+'
	CALL	Blop
	CALL	DELAY
;
	MVI	A,'#'
	CALL	Blop
	CALL	DELAY
;
	MVI	A,' '
	CALL	Blop
;
	LHLD	BL1
	MOV	M,A
	LXI	B,col
	DAD	B
	MOV	M,A

	MVI	A,0
	STA	G1

	LDA	PSCR		; Dropper score
	ADI	1		; Increment it
	STA	PSCR
	STA	Flg1
	RET

Blow:	DW	0

Blop:	MVI	B,05H		; 5 ?
	LHLD	Blow
	LXI	D,-4		; ?
Blp1:	DAD	D
	MOV	M,A
	INX	H
	MOV	M,A
	INX	H
	MOV	M,A
	INX	H
	MOV	M,A
	MOV	M,A		; Twice???
	INX	H
	MOV	M,A
	INX	H
	MOV	M,A
	INX	H
	MOV	M,A
	LXI	D,-col-6
	DCR	B
	RZ
	JMP	Blp1

;--------------------------------

Ploc1:	DW	ScBot

BFLG1:	DB	0

hit:	MOV	A,M
	CPI	' '
	RZ
;
	CPI	10H		; 16 ?
	RZ
;
	CPI	90H		; 90h ?
	RZ
;
	CPI	'<'
	RZ
;
	CPI	'>'
	RZ
;
	SHLD	Blow
	MVI	A,'*'
	CALL	Blop
	CALL	DELAY
;
	MVI	A,'O'		; upper case O
	CALL	Blop
	CALL	DELAY
;
	MVI	A,' '		; [ ]
	CALL	Blop
;
	LDA	MSCR		; Shooter score
	ADI	1		; Increment
	STA	MSCR
;
	LXI	H,0		; Reset to zero
	SHLD	UD
	SHLD	LR
;
	LXI	H,MIDL
	SHLD	CORNR
	RET

;--------------------------------

MSCR:	DB	00H
PSCR:	DB	00H

score:	LXI	H,VDMBAS+4
	LXI	D,Dmsg		; 'DROPPER'
	CALL	print
;
	INX	H
	LDA	PSCR		; Dropper score
	CALL	ScOut
;
	LXI	H,VDMBAS+48
	LXI	D,Smsg		; 'SHOOTER'
	CALL	print
;
	INX	H
	LDA	MSCR		; Shooter score
	CALL	ScOut
;
	RET

ScOut:

; A very dumb hex-to-dec convertor.

	CPI	10
	JNC	Ltr
;
	ADI	'0'
	MOV	M,A
	INX	H
	MVI	M,'0'
	INX	H
	MVI	M,'0'
	RET

Ltr:	CPI	20
	JNC	twen
;
	MVI	M,'1'
	INX	H
	ADI	26H
	MOV	M,A
	INX	H
	MVI	M,'0'
	INX	H
	MVI	M,'0'
	RET

twen:	MVI	M,'2'
	INX	H
	ADI	1CH		; ???
	MOV	M,A
	INX	H
	MVI	M,'0'
	INX	H
	MVI	M,'0'
	CPI	'5'
	JZ	over
	RET

over:	LXI	H,VDMBAS+408	; 408 = 6*64 + 24
	LXI	D,FinMsg	; '** FINAL SCORE **'
	CALL	print
	JMP	begin

;--------------------------------

STRTF:	DB	0

Jet1:	MVI	A,3
	STA	RAFL
	CALL	Rnd4
;
	MOV	E,A
	MVI	D,0
	LXI	H,-col-2
	DAD	D
	SHLD	Incr1
	LHLD	Incr1
	PUSH	H
	POP	D
	LHLD	Py1
	MVI	B,12
rx2:	DAD	D
	MVI	M,4
	DCR	B
	JNZ	rx2
	RET

JetOn:	LDA	RAFL
	CPI	1
	RZ
;
	DCR	A
	STA	RAFL
	CPI	1
	JNZ	rx5
;
	LHLD	Incr1
	PUSH	H
	POP	D
	LHLD	Py1
	MVI	B,12
rx3:	DAD	D
	MVI	M,' '
	DCR	B
	JNZ	rx3
	RET

RAFL:	DB	01H

rx5:	POP	PSW
	POP	PSW
	JMP	RunIt

;--------------------------------

PeaOf:	CALL	rnd
;
	SUI	0D2H		; = ?
	RC
;
	SUI	3		; ?
	RNC
;
	LHLD	Ploc1
	MVI	B,' '
	MOV	M,B
	INX	H
	MOV	M,B
	INX	H
	MOV	M,B
	LXI	D,-col-1
	DAD	D
	MOV	M,B
	XRA	A
	STA	G1
	STA	Flg1
	MVI	A,1
	STA	BFLG1
	LHLD	BL1
	MOV	M,B
	LXI	D,col
	DAD	D
	MOV	M,B
	RET

;--------------------------------

print:	LDAX	D
	CPI	0
	RZ
	MOV	M,A
	INX	H
	INX	D
	JMP	print

;--------------------------------

speed:	DW	0000H

msg1:	DB	'**SET SPEED, 1 THRU 4, 1=FASTEST **', 0

msg:	DB	'** BALLOON **', 0

msg2:	DB	'Copyright 1979 Tony Estep', 0

Dmsg:	DB	'DROPPER', 0

Smsg:	DB	'SHOOTER', 0

FinMsg:	DB	'** FINAL SCORE **', 0

stack	DS	50

;--------------------------------

	END	100H		; Standard CP/M COMmand file
