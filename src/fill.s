r0  := $02
r0l := $02
r0h := $03
vera_data0 := $9f23
.export _fill_vera
.import popa
.segment "CODE"
_fill_vera:
	sta vera_fill_data ; Save the value to fill the vera with
	jsr popa ; Get the length parameter
	sta max ; Store the max value
	cmp #0  ; Make sure it's not 0
	beq @end ; If it is, we're already done
@fill:
	ldx #0 ; Initialize X
	lda vera_fill_data ; Load the fill value
@loop:
	sta vera_data0 ; Store the fill value
	inx ; Increment X
	cpx max ; Compare it against the maximum value
	bne @loop ; If it's not the max yet, do it again
@end:
	rts ; We're done!
.segment "BSS"
max:
	.res 1
vera_fill_data:
	.res 1