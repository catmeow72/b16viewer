r0  := $02
r0l := $02
r0h := $03
vera_data0 := $9f23
.export _fill_vera
.import popax
.segment "CODE"
_fill_vera:
	sta vera_fill_data
	jsr popax
	sta max
	stx max+1
	lda max
	cmp #0
	bne @fill
@max_l_zero:
	lda max+1
	cmp #0
	beq @end
@fill:
	ldx #0
	lda vera_fill_data
@loopx:
	ldy #0
@loopy:
	sta vera_data0
	iny
	cpy max
	bne @loopy
@loopxend:
	inx
	cpx max+1
	bne @loopx
@end:
	rts
.segment "BSS"
tmp:
	.res 2
max:
	.res 2
vera_fill_data:
	.res 1