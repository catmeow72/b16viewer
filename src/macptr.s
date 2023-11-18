.export _cx16_k_macptr
.import popa
r0 := $02
.segment "CODE"
_cx16_k_macptr:
	sta r0
	txa
	tay
	lda r0
	tax
	jsr popa
	clc
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