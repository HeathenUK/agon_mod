#include <agon/vdp_vdu.h>
#include <agon/vdp_key.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <mos_api.h>

#define CHUNK_SIZE 1024 //Sample upload chunk size in bytes

//ez80 defines

#define TMR0_CTL		0x80
#define TMR0_DR_L		0x81
#define TMR0_RR_L		0x81
#define TMR0_DR_H		0x82
#define TMR0_RR_H		0x82
#define PRT0_IVECT		0x0A

volatile void *timer0_prevhandler;
volatile uint24_t ticker = 0;
extern void timer_handler();

#pragma pack(push, 1)

typedef struct {

	char SAMPLE_NAME[22];
	uint16_t SAMPLE_LENGTH;
	uint8_t FINE_TUNE;
	uint8_t VOLUME;
	uint16_t LOOP_START;
	uint16_t LOOP_LENGTH;

} mod_sample;

typedef struct {

	char name[20];
	mod_sample sample[31];
	uint8_t num_orders;
	uint8_t discard_byte;
	uint8_t order[128];
	char sig[4];

} mod_file_header;

typedef struct {

	mod_file_header header;
	uint8_t pattern_max;
	uint8_t channels;
	uint8_t *pattern_buffer;
	uint8_t current_speed;
	uint8_t current_bpm;
	uint8_t current_order;
	uint8_t current_row;
	bool	pattern_break_pending;
	bool	order_break_pending;
	uint8_t new_order;
	uint8_t new_row;
	uint8_t sample_total;

} mod_header;

typedef struct {

	uint16_t latched_sample;
	uint8_t latched_volume;
	int16_t current_volume;
	uint8_t current_effect;
	uint8_t current_effect_param;
	uint16_t current_period;
	uint16_t target_period;
	uint8_t slide_rate;
	uint24_t latched_offset;
	int8_t vibrato_position;
	uint8_t vibrato_speed;
	uint8_t vibrato_depth;	
	int8_t tremolo_position;
	uint8_t tremolo_speed;
	uint8_t tremolo_depth;		

} channel_data;

uint8_t sine_table[] = {
	0, 24, 49, 74, 97,120,141,161,
	180,197,212,224,235,244,250,253,
	255,253,250,244,235,224,212,197,
	180,161,141,120, 97, 74, 49, 24};

#pragma pack(pop)

mod_header mod;
channel_data *channels_data = NULL;

// Parameters:
// - argc: Argument count
// - argv: Pointer to the argument string - zero terminated, parameters separated by spaces
//

static volatile SYSVAR *sv;

unsigned char get_port(uint8_t port) {
    unsigned char output;
    __asm__ volatile (
        "ld b, 0 \n"
        "ld c, %1 \n"
        "in a, (c) \n"
        "ld %0, a"
        : "=d"(output)
        : "d"(port)
		: "cc", "memory", "b", "c", "a"
    );
    return output;
}

void set_port(uint8_t port, uint8_t value) {
    __asm__ volatile (
        "ld b, 0 \n"
		"ld a, %1 \n"
        "ld c, %0 \n"
        "out (c), a"
        :
        : "r"(port), "r"(value)
        : "cc", "memory", "b", "c", "a"
    );
}

void timer0_begin(uint16_t reload_value, uint16_t clk_divider) {

	//timer0_period (in SECONDS) = (reload_value * clk_divider) / system_clock_frequency (which is 18432000 Hz)
    
	unsigned char clkbits = 0;
    unsigned char ctl;

    timer0_prevhandler = mos_setintvector(PRT0_IVECT, timer_handler);

    switch (clk_divider) {
        case 4:   clkbits = 0x00; break;
        case 16:  clkbits = 0x04; break;
        case 64:  clkbits = 0x08; break;
        case 256: clkbits = 0x0C; break;
    }
    ctl = 0x53 | clkbits; // Continuous mode, reload and restart enabled, and enable the timer    

    set_port(TMR0_CTL, 0x00); // Disable the timer and clear all settings
	ticker = 0;
    set_port(TMR0_RR_L, (unsigned char)(reload_value));
    set_port(TMR0_RR_H, (unsigned char)(reload_value >> 8));
    set_port(TMR0_CTL, ctl);

}

void timer0_end() {
	
	set_port(TMR0_CTL, 0x00);
	set_port(TMR0_RR_L, 0x00);
	set_port(TMR0_RR_H, 0x00);
	mos_setintvector(PRT0_IVECT, timer0_prevhandler);
	ticker = 0;

}

uint16_t swap_word(uint16_t num) {
    return ((num >> 8) & 0xFF) | ((num & 0xFF) << 8);
}

uint32_t swap32(uint32_t num) {

	return ((num & 0xFF000000) >> 24) |
	((num & 0x00FF0000) >> 8) |
	((num & 0x0000FF00) << 8) |
	((num & 0x000000FF) << 24);

}

void on_tick()
{
	ticker++;

}

void write16bit(uint16_t w)
{
	putch(w & 0xFF); // write LSB
	putch(w >> 8);	 // write MSB	
}

void write24bit(uint24_t w)
{
	putch(w & 0xFF); // write LSB
	putch(w >> 8);	 // write middle	
    putch(w >> 16);	 // write MSB	
}

void add_stream_to_buffer(uint16_t buffer_id, char* buffer_content, uint16_t buffer_size) {	

	putch(23);
	putch(0);
	putch(0xA0);
	write16bit(buffer_id);
	putch(0);
	write16bit(buffer_size);
	
    mos_puts(buffer_content, buffer_size, 0);

}

void sample_from_buffer(uint16_t buffer_id, uint8_t format) {

	putch(23);
	putch(0);
	putch(0x85);	
	putch(0);
	putch(5);
	putch(2);
	write16bit(buffer_id);
	putch(format);

}

void tuneable_sample_from_buffer(uint16_t buffer_id, uint16_t frequency) {

	putch(23);
	putch(0);
	putch(0x85);	
	putch(0); //Ignored
	putch(5);
	putch(2);
	write16bit(buffer_id);
	putch(24);
	write16bit(frequency);

}

void enable_channel(uint8_t channel) {

	//VDU 23, 0, &85, channel, 8
	putch(23);
	putch(0);
	putch(0x85);
	putch(channel);
	putch(8);

}

void assign_sample_to_channel(uint16_t sample_id, uint8_t channel_id) {
	
	putch(23);
	putch(0);
	putch(0x85);
	putch(channel_id);
	putch(4);
	putch(8);
	write16bit(sample_id);
	
}

void play_sample(uint16_t sample_id, uint8_t channel, uint8_t volume, uint16_t duration, uint16_t frequency) {

	assign_sample_to_channel(sample_id, channel);

	putch(23);
	putch(0);
	putch(0x85);
	putch(channel);
	putch(0);
	putch(volume);
	write16bit(frequency);
	write16bit(duration);

}

void set_volume(uint8_t channel, uint8_t volume) {

	//VDU 23, 0, &85, channel, 2, volume

	putch(23);
	putch(0);
	putch(0x85);
	putch(channel);
	putch(2);
	putch(volume);

}

void set_frequency(uint8_t channel, uint16_t frequency) {

	//VDU 23, 0, &85, channel, 3, frequency;

	putch(23);
	putch(0);
	putch(0x85);
	putch(channel);
	putch(3);
	write16bit(frequency);

}

void set_position(uint8_t channel, uint24_t position) {

	//VDU 23, 0, &85, channel, 11, position; positionHighByte

	putch(23);
	putch(0);
	putch(0x85);
	putch(channel);
	putch(11);
	write24bit(position);

}

void play_channel(uint8_t channel, uint8_t volume, uint24_t duration, uint16_t frequency) {

	putch(23);
	putch(0);
	putch(0x85);
	putch(channel);
	putch(0);
	putch(volume);
	write16bit(frequency);
	write16bit(duration);

}

void clear_buffer(uint16_t buffer_id) {
	
	putch(23);
	putch(0);
	putch(0xA0);
	write16bit(buffer_id);
	putch(2);
	
}

void set_sample_frequency(uint16_t buffer_id, uint16_t frequency) {
	
	putch(23);
	putch(0);
	putch(0x85);
	putch(0); //Ignored channel
	putch(5);
	putch(4);
	write16bit(buffer_id);
	write16bit(frequency);
	
}

void set_channel_rate(uint8_t channel, uint16_t sample_rate) {
	
	putch(23);
	putch(0);
	putch(0x85);
	putch(channel);
	putch(13);
	write16bit(sample_rate);
	
}

void reset_channel(uint8_t channel) {
	
	putch(23);
	putch(0);
	putch(0x85);
	putch(channel);
	putch(10);
	
}

void set_sample_duration_and_play(uint8_t channel, uint24_t duration) {
	
	putch(23);
	putch(0);
	putch(0x85);
	putch(channel);
	putch(12);
	write24bit(duration);
	
}

void wait_tick(uint16_t ticks) {

	while((ticker + ticks) < ticker) {}

}

void set_sample_loop_start(uint16_t sample_id, uint24_t start) {

	//VDU 23, 0, &85, channel, 5, 6, bufferId; repeatStart; repeatStartHighByte

	putch(23);
	putch(0);
	putch(0x85);
	putch(0);
	putch(5);
	putch(6);
	write16bit(sample_id);
	write24bit(start);

}

void set_sample_loop_length(uint16_t sample_id, uint24_t length) {

	//VDU 23, 0, &85, channel, 5, 8, bufferId; repeatLength; repeatLengthHighByte

	putch(23);
	putch(0);
	putch(0x85);
	putch(0);
	putch(5);
	putch(8);
	write16bit(sample_id);
	write24bit(length);

}

bool verbose = true;
bool extra_verbose = false;

const char* period_to_note(uint16_t period) {

    switch (period) {
	  case 0  : return "---";

	  case 856: return "C-1";
	  case 808: return "C#1";
	  case 762: return "D-1";
	  case 720: return "D#1";
	  case 678: return "E-1";
	  case 640: return "F-1";
	  case 604: return "F#1";
	  case 570: return "G-1";
	  case 538: return "G#1";
	  case 508: return "A-1";
	  case 480: return "A#1";
	  case 453: return "B-1";
	  case 428: return "C-2";
	  case 404: return "C#2";
	  case 381: return "D-2";
	  case 360: return "D#2";
	  case 339: return "E-2";
	  case 320: return "F-2";
	  case 302: return "F#2";
	  case 285: return "G-2";
	  case 269: return "G#2";
	  case 254: return "A-2";
	  case 240: return "A#2";
	  case 226: return "B-2";
	  case 214: return "C-3";
	  case 202: return "C#3";
	  case 190: return "D-3";
	  case 180: return "D#3";
	  case 170: return "E-3";
	  case 160: return "F-3";
	  case 151: return "F#3";
	  case 143: return "G-3";
	  case 135: return "G#3";
	  case 127: return "A-3";
	  case 120: return "A#3";
	  case 113: return "B-3";
	 
	  default: return "???";
    }
	
}

void process_note(uint8_t *buffer, size_t pattern_no, size_t row, uint8_t enabled)  {

	size_t offset = (mod.channels * 4 * 64) * pattern_no + (row * 4 * mod.channels);

	uint8_t *noteData = buffer + offset;

	//mod.pattern_break_pending = false;

	if (verbose) {
	
		putch(17);
		putch(15);

		//printf("\r\n%02u ", row);
		printf("\r\n    %02u ", row);

	}

	uint8_t sample_number;
	uint8_t effect_number;
	uint8_t effect_param;
	uint16_t period;
	uint16_t hz;

	for (uint8_t i = 0; i < mod.channels; i++) {

		if (verbose) {

			putch(17);
			putch(9 + i);
			
		}

		sample_number = (noteData[0] & 0xF0) + (noteData[2] >> 4);
		effect_number = (noteData[2] & 0xF);
		effect_param = noteData[3];
		period = ((uint16_t)(noteData[0] & 0xF) << 8) | (uint16_t)noteData[1];

		if (effect_number == 0x03 && effect_param > 0) { //Log the note as the effect's target, but don't use it now.
			channels_data[i].slide_rate = effect_param; //If the effect has a parameter, use it as slide rate.
			channels_data[i].target_period = period;
		} else if (period > 0) {
			channels_data[i].current_period = period;
		}

		if (effect_number == 0x09 && effect_param > 0) channels_data[i].latched_offset = effect_param * 256;

		hz = channels_data[i].current_period > 0 ? 187815 / channels_data[i].current_period : 0;

		if (effect_param || effect_number) {
			channels_data[i].current_effect = effect_number;
			channels_data[i].current_effect_param = effect_param;
		} else channels_data[i].current_effect = 0xFF;

		// Output the decoded note information
		// Ref: void play_sample(uint16_t sample_id, uint8_t channel, uint8_t volume, uint16_t duration, uint16_t frequency)
		
		if ((enabled & (1 << i)) != 0) {
		
			if (sample_number > 0) {
				
				channels_data[i].latched_sample = sample_number;
				channels_data[i].latched_volume = (mod.header.sample[sample_number - 1].VOLUME * 2) - 1;
				channels_data[i].current_volume = channels_data[i].latched_volume;

				if ((period > 0) && (effect_number != 0x03)) {	
					
					if (swap_word(mod.header.sample[channels_data[i].latched_sample - 1].LOOP_LENGTH) > 0) play_sample(channels_data[i].latched_sample, i, channels_data[i].latched_volume, -1, hz);
					else play_sample(channels_data[i].latched_sample, i, channels_data[i].latched_volume, 0, hz);
					channels_data[i].current_volume = channels_data[i].latched_volume;
					if (effect_number == 0x09) set_position(i, channels_data[i].latched_offset);

				}

			} else if ((period > 0) && (effect_number != 0x03)) {

				if (channels_data[i].latched_sample > 0) {
					
					if (swap_word(mod.header.sample[channels_data[i].latched_sample - 1].LOOP_LENGTH) > 0) play_sample(channels_data[i].latched_sample, i, channels_data[i].latched_volume, -1, hz);
					else play_sample(channels_data[i].latched_sample, i, channels_data[i].latched_volume, 0, hz);
					channels_data[i].current_volume = channels_data[i].latched_volume;
					if (effect_number == 0x09) set_position(i, channels_data[i].latched_offset);

				}

			}

		}

		if (verbose) {
		
		//printf("%03u/%04u %02u %02X %X%02X", period, hz, sample_number, channels_data[i].current_volume, effect_number, effect_param);
		//printf("%s %02u %02X %X%02X", period_to_note(period), sample_number, channels_data[i].current_volume, effect_number, effect_param);
		printf("%s %02u %02X %X%02X", period_to_note(period), sample_number, channels_data[i].current_volume, effect_number, effect_param);

		putch(17);
		putch(7);
		//if (i != mod.channels - 1) printf("|");
		if (i != mod.channels - 1) printf(" | ");

		}

		noteData += 4;

		//Process effects that should happen immediately

		if (channels_data[i].current_effect != 0xFF) {

			switch (channels_data[i].current_effect) {

				case 0x04: {//Vibrato

					uint8_t param_x = channels_data[i].current_effect_param >> 4;
					uint8_t param_y = channels_data[i].current_effect_param & 0x0F;
					
					channels_data[i].vibrato_position = 0;
					if (param_x) channels_data[i].vibrato_speed = param_x;
					if (param_y) channels_data[i].vibrato_depth = param_y;

				} break;	

				case 0x07: {//Tremolo

					uint8_t param_x = channels_data[i].current_effect_param >> 4;
					uint8_t param_y = channels_data[i].current_effect_param & 0x0F;
					
					channels_data[i].tremolo_position = 0;
					if (param_x) channels_data[i].tremolo_speed = param_x;
					if (param_y) channels_data[i].tremolo_depth = param_y;

				} break;					

				case 0x0B: {//Skip to order xx

					uint8_t param_x = channels_data[i].current_effect_param >> 4;
					uint8_t param_y = channels_data[i].current_effect_param & 0x0F;
					
					uint8_t new_order = (param_x * 16) + param_y;

					if (mod.order_break_pending == false) {
						mod.new_order = new_order;
						mod.order_break_pending = true;
					} 

				} break;				

				case 0x0C: {//Set channel volume to xx

					uint8_t param_x = channels_data[i].current_effect_param >> 4;
					uint8_t param_y = channels_data[i].current_effect_param & 0x0F;

					uint8_t new_vol = (param_x * 16) + param_y;

					channels_data[i].current_volume = (new_vol * 2) - 1;
					if (channels_data[i].current_volume < 0) channels_data[i].current_volume = 0;
					else if (channels_data[i].current_volume > 127) channels_data[i].current_volume = 127;
					set_volume(i, channels_data[i].current_volume);
					if (extra_verbose) printf("\r\nSetting channel %u volume to %u (%u).", i, channels_data[i].current_effect_param, channels_data[i].current_volume);

				} break;				

				case 0x0D: {//Pattern break - Skip to next pattern, row xx

					uint8_t param_x = channels_data[i].current_effect_param >> 4;
					uint8_t param_y = channels_data[i].current_effect_param & 0x0F;
					
					uint8_t new_row = (param_x * 10) + param_y;

					if (mod.pattern_break_pending == false) {
						mod.new_row = new_row;
						mod.pattern_break_pending = true;
					}

				} break;

				case 0x0F: {//Set speed or tempo

					if (channels_data[i].current_effect_param < 0x20) { //<0x20 means speed (i.e. ticks per row)

						mod.current_speed = channels_data[i].current_effect_param;
						if (extra_verbose) printf("\r\nTempo set to %u.", channels_data[i].current_effect_param);

					}

				} break;				

				default:
					break;

			}		

		}		

	}

}

void process_tick() {

	for (uint8_t i = 0; i < mod.channels; i++) {

		if (channels_data[i].current_effect != 0xFF) {

			switch (channels_data[i].current_effect) {

				case 0x01: { //Pitch slide (porta) up

					uint8_t param_x = channels_data[i].current_effect_param >> 4;
					uint8_t param_y = channels_data[i].current_effect_param & 0x0F;

					uint8_t slide = (param_x * 16) + param_y;

					channels_data[i].current_period -= slide;
					set_frequency(i, 187815 / channels_data[i].current_period);
					if (extra_verbose) printf("\r\nSlide period down %u to %u (%uHz)", channels_data[i].current_effect_param, channels_data[i].current_period, 187815 / channels_data[i].current_period);

				} break;

				case 0x02: { //Pitch slide (porta) down

					uint8_t param_x = channels_data[i].current_effect_param >> 4;
					uint8_t param_y = channels_data[i].current_effect_param & 0x0F;

					uint8_t slide = (param_x * 16) + param_y;			

					channels_data[i].current_period += slide;
					set_frequency(i, 187815 / channels_data[i].current_period);
					if (extra_verbose) printf("\r\nSlide period up %u to %u (%uHz)", channels_data[i].current_effect_param, channels_data[i].current_period, 187815 / channels_data[i].current_period);

				} break;

				case 0x03: { //Pitch slide toward target note (tone portamento)

					//target_period, latched_period, current_period

					if (channels_data[i].target_period > channels_data[i].current_period) {

						channels_data[i].current_period += channels_data[i].slide_rate;
						set_frequency(i, 187815 / channels_data[i].current_period);
						if (extra_verbose) printf("\r\nSliding %u to %u (toward %u) on channel %u", channels_data[i].slide_rate, channels_data[i].current_period, channels_data[i].target_period, i);

					} else if (channels_data[i].target_period < channels_data[i].current_period) {

						channels_data[i].current_period -= channels_data[i].slide_rate;
						set_frequency(i, 187815 / channels_data[i].current_period);
						if (extra_verbose) printf("\r\nSliding %u to %u (toward %u) on channel %u", channels_data[i].slide_rate, channels_data[i].current_period, channels_data[i].target_period, i);

					}

				} break;				

				case 0x04: {//Vibrato
					
					uint8_t delta = sine_table[abs(channels_data[i].vibrato_position)];
  					delta *= channels_data[i].vibrato_depth;
					delta >>= 7; //Divide by 128

					if (channels_data[i].vibrato_position < 0) set_frequency(i, 187815 / (channels_data[i].current_period - delta));
					else if (channels_data[i].vibrato_position >= 0) set_frequency(i, 187815 / (channels_data[i].current_period + delta));
		
					if (extra_verbose) printf("\r\nVibrato on %u with speed %u and depth %u, sine pos %i meaning delta %u.", i, channels_data[i].vibrato_speed, channels_data[i].vibrato_depth, channels_data[i].vibrato_position, delta);

					channels_data[i].vibrato_position += channels_data[i].vibrato_speed;
					if (channels_data[i].vibrato_position > 31) channels_data[i].vibrato_position -= 64;

				} break;

				case 0x05: {//Volume Slide + Tone Portamento
					
					uint8_t slide_x = channels_data[i].current_effect_param >> 4;
					uint8_t slide_y = channels_data[i].current_effect_param & 0x0F;

					if (slide_x) {

						uint8_t slide_adjusted = ((slide_x * 2) - 1);
						channels_data[i].current_volume += slide_adjusted;
						if (channels_data[i].current_volume > 127) channels_data[i].current_volume = 127;
						if (extra_verbose) printf("\r\nSlide tick on %u, increase by %u (%u) to %u.", i, slide_x, slide_adjusted, channels_data[i].current_volume);
						set_volume(i, channels_data[i].current_volume);

					} else {

						uint8_t slide_adjusted = ((slide_y * 2) - 1);
						channels_data[i].current_volume -= slide_adjusted;
						if (channels_data[i].current_volume < 0) channels_data[i].current_volume = 0;
						if (extra_verbose) printf("\r\nSlide tick on %u, decrease by %u (%u) to %u.", i, slide_y, slide_adjusted, channels_data[i].current_volume);
						set_volume(i, channels_data[i].current_volume);
					}					

					if (channels_data[i].target_period > channels_data[i].current_period) {

						channels_data[i].current_period += channels_data[i].slide_rate;
						set_frequency(i, 187815 / channels_data[i].current_period);
						if (extra_verbose) printf("\r\nSliding %u to %u (toward %u) on channel %u", channels_data[i].slide_rate, channels_data[i].current_period, channels_data[i].target_period, i);

					} else if (channels_data[i].target_period < channels_data[i].current_period) {

						channels_data[i].current_period -= channels_data[i].slide_rate;
						set_frequency(i, 187815 / channels_data[i].current_period);
						if (extra_verbose) printf("\r\nSliding %u to %u (toward %u) on channel %u", channels_data[i].slide_rate, channels_data[i].current_period, channels_data[i].target_period, i);

					}					

				} break;	

				case 0x06: {//Volume Slide + Vibrato

					uint8_t slide_x = channels_data[i].current_effect_param >> 4;
					uint8_t slide_y = channels_data[i].current_effect_param & 0x0F;

					if (slide_x) {

						uint8_t slide_adjusted = ((slide_x * 2) - 1);
						channels_data[i].current_volume += slide_adjusted;
						if (channels_data[i].current_volume > 127) channels_data[i].current_volume = 127;
						if (extra_verbose) printf("\r\nSlide tick on %u, increase by %u (%u) to %u.", i, slide_x, slide_adjusted, channels_data[i].current_volume);
						set_volume(i, channels_data[i].current_volume);

					} else {

						uint8_t slide_adjusted = ((slide_y * 2) - 1);
						channels_data[i].current_volume -= slide_adjusted;
						if (channels_data[i].current_volume < 0) channels_data[i].current_volume = 0;
						if (extra_verbose) printf("\r\nSlide tick on %u, decrease by %u (%u) to %u.", i, slide_y, slide_adjusted, channels_data[i].current_volume);
						set_volume(i, channels_data[i].current_volume);
					}						

					uint8_t delta = sine_table[channels_data[i].vibrato_position & 31];
  					delta *= channels_data[i].vibrato_depth;
					delta >>= 7; //Divide by 128

					if (channels_data[i].vibrato_position < 0) set_frequency(i, 187815 / (channels_data[i].current_period - delta));
					else if (channels_data[i].vibrato_position >= 0) set_frequency(i, 187815 / (channels_data[i].current_period + delta));
		
					if (extra_verbose) printf("\r\nVibrato on %u with speed %u and depth %u, sine pos %i meaning delta %u.", i, channels_data[i].vibrato_speed, channels_data[i].vibrato_depth, channels_data[i].vibrato_position, delta);

					channels_data[i].vibrato_position += channels_data[i].vibrato_speed;
					if (channels_data[i].vibrato_position > 31) channels_data[i].vibrato_position -= 64;

				} break;	

				case 0x07: {//Tremolo
					
					uint8_t delta = sine_table[channels_data[i].tremolo_position & 31];
  					delta *= channels_data[i].tremolo_depth;
					delta >>= 6; //Divide by 64

					if (channels_data[i].tremolo_position < 0) set_volume(i, channels_data[i].current_volume - (delta * 2));
					else if (channels_data[i].tremolo_position >= 0) set_volume(i, channels_data[i].current_volume + (delta * 2));
					if (channels_data[i].current_volume > 127) channels_data[i].current_volume = 127;
					if (channels_data[i].current_volume < 0) channels_data[i].current_volume = 0;
		
					//if (extra_verbose) printf("\r\nTremolo on %u with speed %u and depth %u, sine pos %i meaning delta %u.", i, channels_data[i].tremolo_speed, channels_data[i].tremolo_depth, channels_data[i].tremolo_position, delta);

					channels_data[i].tremolo_position += channels_data[i].tremolo_speed;
					if (channels_data[i].tremolo_position > 31) channels_data[i].tremolo_position -= 64;

				} break;				

				case 0x0A: { //Volume slide

					uint8_t slide_x = channels_data[i].current_effect_param >> 4;
					uint8_t slide_y = channels_data[i].current_effect_param & 0x0F;
					
					if (slide_x) {

						uint8_t slide_adjusted = ((slide_x * 2) - 1);
						channels_data[i].current_volume += slide_adjusted;
						if (channels_data[i].current_volume > 127) channels_data[i].current_volume = 127;
						if (extra_verbose) printf("\r\nSlide tick on %u, increase by %u (%u) to %u.", i, slide_x, slide_adjusted, channels_data[i].current_volume);
						set_volume(i, channels_data[i].current_volume);

					} else {

						uint8_t slide_adjusted = ((slide_y * 2) - 1);
						channels_data[i].current_volume -= slide_adjusted;
						if (channels_data[i].current_volume < 0) channels_data[i].current_volume = 0;
						if (extra_verbose) printf("\r\nSlide tick on %u, decrease by %u (%u) to %u.", i, slide_y, slide_adjusted, channels_data[i].current_volume);
						set_volume(i, channels_data[i].current_volume);
					}

				} break;

				default:
					break;

			}		

		}

	}

}

void delay_cents(uint16_t ticks_end) { //100ms ticks
	
	uint16_t ticks = 0;
	ticks_end *= 6;
	while(true) {
		
		waitvblank();
		ticks++;
		if(ticks >= ticks_end) break;
		
	}
	
}

void header_line() {

	//printf("\r\nOrder %u (Pattern %u)\r\n", mod.current_order, mod.header.order[mod.current_order]);
	printf("%02u %03u Frq Sa Vo Eff | Frq Sa Vo Eff | Frq Sa Vo Eff | Frq Sa Vo Eff", mod.current_order, mod.header.order[mod.current_order]);
	
}

int main(int argc, char * argv[])
//int main(void)
{
	sv = vdp_vdu_init();
	if ( vdp_key_init() == -1 ) return 1;

	FILE *file = fopen(argv[1], "rb");
    if (file == NULL) {
        printf("Could not open file.\r\n");
        return 0;
    }

	if (sv->scrMode > 0) {
		putch(22);
		putch(0);
	}

	//set_channel_rate(-1, 16384);

	fread(&mod.header, sizeof(mod_file_header), 1, file);

	if (strncmp(mod.header.sig, "M.K.", 4) == 0) mod.channels = 4; //Classic 4 channels
	else if (strncmp(mod.header.sig, "6CHN", 4) == 0) mod.channels = 6;
	else if (strncmp(mod.header.sig, "8CHN", 4) == 0) mod.channels = 8;
	else {

		printf("Unknown .mod format.\r\n");
		return 0;

	}

	channels_data = (channel_data*) malloc(sizeof(channel_data) * mod.channels);
	for (uint8_t i = 0; i < mod.channels; i++) {
		enable_channel(i);
		channels_data[i].latched_sample = 0;
		channels_data[i].latched_volume = 0;
		channels_data[i].current_effect = 0xFF;
		channels_data[i].current_effect_param = 0;
		
	}
	mod.pattern_max = 0;
	mod.current_speed = (argc == 3) ? atoi(argv[2]) : 6;
	mod.current_bpm = 125;
	mod.pattern_break_pending = false;
	mod.order_break_pending = false;

	for (uint8_t i = 0; i < 127; i++) if (mod.header.order[i] > mod.pattern_max) mod.pattern_max = mod.header.order[i];

	//Number of patterns * number of channels * number of bytes per note per channel * number of notes per pattern (i.e. 1024 bytes per 4 channel pattern)
	mod.pattern_buffer = (uint8_t*) malloc(sizeof(uint8_t) * (mod.pattern_max + 1) * mod.channels * 4 * 64);
	fread(mod.pattern_buffer, sizeof(uint8_t), (mod.pattern_max + 1) * mod.channels * 4 * 64, file);

	printf("Module name: %s\r\n", mod.header.name);
	printf("Song length: %u\r\n", mod.header.num_orders);
	printf("Mod patterns: %u\r\n", mod.pattern_max);
	printf("Module signature: %c%c%c%c (%u channels)\r\n", mod.header.sig[0], mod.header.sig[1], mod.header.sig[2], mod.header.sig[3], mod.channels);
	printf("Pattern buffer size: %u Bytes\r\n", (mod.pattern_max + 1) * mod.channels * 4 * 64);	

	uint8_t *temp_sample_buffer;
	mod.sample_total = 0;
	
	for (uint8_t i = 1; i < 31; i++) {

		uint16_t sample_length_swapped = swap_word(mod.header.sample[i - 1].SAMPLE_LENGTH);
		uint16_t sample_loop_start_swapped = swap_word(mod.header.sample[i - 1].LOOP_START);
		uint16_t sample_loop_length_swapped = swap_word(mod.header.sample[i - 1].LOOP_LENGTH);

		if (sample_length_swapped > 0) {

			mod.sample_total++;

			printf("Uploading sample %u", i);
			if (extra_verbose) {
				printf(" (%02u KB) with default volume %02X", (sample_length_swapped * 2) / 10, mod.header.sample[i - 1].VOLUME);			
				if (sample_loop_length_swapped) printf(", loop start %05u", sample_loop_start_swapped * 2);
			}
			printf("\r\n");

			uint24_t remaining_data = sample_length_swapped * 2;
			uint16_t chunk;

			clear_buffer(i);
			temp_sample_buffer = (uint8_t*) malloc(sizeof(uint8_t) * CHUNK_SIZE);
			if (temp_sample_buffer == NULL) {
				printf("\r\nMemory allocation failed (%u KB).\r\n", CHUNK_SIZE);
				return 0;	
			}

			while (remaining_data > 0) {
				
				if (remaining_data > CHUNK_SIZE) {
					chunk = CHUNK_SIZE;
				} else chunk = remaining_data;
				
				fread(temp_sample_buffer, sizeof(uint8_t), chunk, file);
				
				add_stream_to_buffer(i, (char *)temp_sample_buffer, chunk);
				
				remaining_data -= chunk;
			
			}

			tuneable_sample_from_buffer(i, 8363);

		}

		if (sample_loop_start_swapped > 0) {

			set_sample_loop_start(i, sample_loop_start_swapped * 2);
			set_sample_loop_length(i, sample_loop_length_swapped * 2);

		} //else set_sample_loop_length(i, sample_loop_length_swapped * 2);

	}

	free(temp_sample_buffer);
	
	verbose = true;
	extra_verbose = false;

	ticker = 0;
	//timer0_begin(23040, 16);
	timer0_begin(22850, 16); //Slightly faster time to offset other cycles swallowed.
	mod.current_order = 0, mod.current_row = 0;
	uint24_t old_ticker = ticker;
	uint8_t enabled = 255;
	uint16_t old_key_count = sv->vkeycount;
	
	printf("\r\nOrder %u (Pattern %u)\r\n", mod.current_order, mod.header.order[mod.current_order]);
	process_note(mod.pattern_buffer, mod.header.order[mod.current_order], mod.current_row++, enabled);

	uint24_t tick = 0;
	uint8_t mid_tick;

	bool do_ticks = true;

	while (1) {

		if ((ticker - old_ticker) >= mod.current_speed) {

			old_ticker = ticker, tick = ticker;
			mid_tick = 0;

			if (sv->vkeycount != old_key_count) {

				if (sv->keyascii == 27) break;

				if (sv->keyascii == 't') do_ticks = !do_ticks;
				
				if (sv->keyascii == '1') enabled = enabled ^ 0x01;
				if (sv->keyascii == '2') enabled = enabled ^ 0x02;
				if (sv->keyascii == '3') enabled = enabled ^ 0x04;
				if (sv->keyascii == '4') enabled = enabled ^ 0x08;
				if (sv->keyascii == '0') enabled = 255;

				old_key_count = sv->vkeycount;

			}

			if (mod.order_break_pending && mod.order_break_pending) { //Combined order and pattern breaks (0x0B and 0x0D on the same row)

				mod.current_order = mod.new_order;
				mod.current_row = mod.new_row;
				mod.order_break_pending = false;
				mod.pattern_break_pending = false;
				
				printf("\r\nOrder %u (Pattern %u)\r\n", mod.current_order, mod.header.order[mod.current_order]);

			} else if (mod.order_break_pending) { //Just an order break (0x0B)

				mod.current_order = mod.new_order;
				mod.current_row = 0;
				mod.order_break_pending = false;

				printf("\r\nOrder %u (Pattern %u)\r\n", mod.current_order, mod.header.order[mod.current_order]);

			} else if (mod.pattern_break_pending) { //Just a pattern break (0x0D)

				mod.current_order++;
				mod.current_row = mod.new_row;
				mod.pattern_break_pending = false;

				printf("\r\nOrder %u (Pattern %u)\r\n", mod.current_order, mod.header.order[mod.current_order]);

			}			

			process_note(mod.pattern_buffer, mod.header.order[mod.current_order], mod.current_row++, enabled);

			if (mod.current_row == 64) {

				mod.current_order++;
				if (mod.current_order >= mod.header.num_orders - 1) mod.current_order = 0;
				printf("\r\nOrder %u (Pattern %u)\r\n", mod.current_order, mod.header.order[mod.current_order]);
				mod.current_row = 0;

			}

		} else if (ticker - tick > 0) { //We're in between rows

			if (mid_tick++ < mod.current_speed) {

				tick = ticker;
				//printf("\r\nTick %u", mid_tick);
				if (do_ticks) process_tick();

			}

		}

	}

	for (uint8_t i = 0; i < mod.channels; i++) reset_channel(i);

	free(channels_data);

	for (uint8_t i = 0; i < mod.sample_total; i++) clear_buffer(i);

	fclose(file);
	timer0_end();

	//set_channel_rate(-1, 16384);
	putch(17);
	putch(15);
	printf("\r\n");

	return 0;
}
