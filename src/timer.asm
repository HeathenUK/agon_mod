	section	.text,"ax",@progbits
	assume	adl = 1

	public _timer_handler

_timer_handler:
	di
	push af
	IN0	a,(0x80)
	push bc
	push de
	push hl
	push ix
	push iy

	; in C code
	call _on_tick

	pop iy
	pop ix
	pop hl
	pop de
	pop bc
	pop af
	ei
	reti.lil

	extern _on_tick
