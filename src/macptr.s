.export _cx16_k_macptr
.import popa
r0 := $02
r0l:= $02
r0h:= $03
r1 := $04
r1l:= $04
r1h:= $05
.segment "CODE"
_cx16_k_macptr:
	sta r0l
	stx r0h
	jsr popa
	sta r1l
	jsr popa
	sta r1h
	ldx r0l
	ldy r0h
	lda r1l
	beq @increment
	clc
	jmp @loadregs
@increment:
	sec
@loadregs:
	lda r1h
	jsr $FF44
	bcs @error
	txa
	sty r0
	ldx r0
	rts
@error:
	lda #0
	ldx #0
	rts
