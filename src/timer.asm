	section	.text,"ax",@progbits
	assume	adl = 1

	public _timer_handler_0
	public _timer_handler_1
	public _timer_handler_2
	public _timer_handler_3
	public _timer_handler_4
	public _timer_handler_5

_timer_handler_0:
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

_timer_handler_1:
	di
	push af
	IN0	a,(0x83)
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

_timer_handler_2:
	di
	push af
	IN0	a,(0x86)
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

_timer_handler_3:
	di
	push af
	IN0	a,(0x89)
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

_timer_handler_4:
	di
	push af
	IN0	a,(0x8C)
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

_timer_handler_5:
	di
	push af
	IN0	a,(0x8F)
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
