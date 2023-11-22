.export _cx16_k_macptr
.import popa
r0 := $02
r0l:= $02
r0h:= $03
r1 := $04
r1l:= $04
r1h:= $05
.segment "CODE"
; ASM wrapper for MACPTR
_cx16_k_macptr:
	sta r0l ; Store pointer in ZP
	stx r0h
	jsr popa ; Store increment boolean
	sta r1l
	jsr popa ; Store size byte
	sta r1h
	ldx r0l ; Load parameters
	ldy r0h 
	lda r1l
	beq @increment ; If A is 0, set the carry flag
	clc ; Otherwise clear it
	jmp @loadregs ; And don't set it again
@increment:
	sec ; Set the carry flag
@loadregs:
	lda r1h ; Load the size parameter
	jsr $FF44 ; Call MACPTR
	bcs @error ; If there was an error, return 0
	txa ; Otherwise, set up registers to hold the return value
	sty r0
	ldx r0
	rts
@error:
	lda #0 ; Return 0 in case of error
	ldx #0
	rts
