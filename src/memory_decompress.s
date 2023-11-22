.export _memory_decompress
.import popax
r0  := $02
r0l := $02
r0h := $03
r1l := $04
r1h := $05
.segment "CODE"
_memory_decompress:
	jsr popax
	sta r1l
	stx r1h
	jsr popax
	sta r0l
	stx r0h
	jsr $FEED
	lda r1l
	ldx r1h
	rts