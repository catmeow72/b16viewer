r0  := $02
r0l := $02
r0h := $03
vera_data0 := $9f23
.export _fill_vera
.import popa
.segment "CODE"
_fill_vera:
	sta vera_fill_data
	jsr popa
	sta max
	lda max
	cmp #0
	beq @end
@fill:
	ldx #0
	lda vera_fill_data
@loop:
	sta vera_data0
	inx
	cpx max
	bne @loop
@end:
	rts
.segment "BSS"
max:
	.res 1
vera_fill_data:
	.res 1