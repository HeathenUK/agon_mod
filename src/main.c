/*
 * Title:			Hello World - C example
 * Author:			Dean Belfield
 * Created:			22/06/2022
 * Last Updated:	22/11/2022
 *
 * Modinfo:
 */
 
#include <agon/vdp_vdu.h>
#include <agon/vdp_key.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <mos_api.h>

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

} mod_header;

typedef struct {

	uint16_t latched_sample;
	uint8_t latched_volume;
	uint8_t current_volume;
	uint8_t current_effect;
	uint8_t current_effect_param;
	//Much more to come!

} channel_data;

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

void process_note(uint8_t *buffer, size_t pattern_no, size_t row, bool verbose, uint8_t enabled)  {

	size_t offset = (mod.channels * 4 * 64) * pattern_no + (row * 4 * mod.channels);

	uint8_t *noteData = buffer + offset;

	if (verbose) {
	
		putch(17);
		putch(15);

		printf("\r\n%02u ", row);

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
		hz = period > 0 ? 187815 / period: 0;

		if (effect_number) {
			channels_data[i].current_effect = effect_number;
			channels_data[i].current_effect_param = effect_param;
		}

		// Output the decoded note information
		// Ref: void play_sample(uint16_t sample_id, uint8_t channel, uint8_t volume, uint16_t duration, uint16_t frequency)
		
		if ((enabled & (1 << i)) != 0) {
		
			if (sample_number > 0) {
				
				channels_data[i].latched_sample = sample_number;
				channels_data[i].latched_volume = (mod.header.sample[sample_number - 1].VOLUME * 2) - 1;


				if (period > 0) {	
					
					if (swap_word(mod.header.sample[channels_data[i].latched_sample - 1].LOOP_START) > 0) play_sample(channels_data[i].latched_sample, i, channels_data[i].latched_volume, -1, hz);
					else play_sample(channels_data[i].latched_sample, i, channels_data[i].latched_volume, 0, hz);
					channels_data[i].current_volume = channels_data[i].latched_volume;					

				}

			} else if (period > 0) {

				if (channels_data[i].latched_sample > 0) {
					
					if (swap_word(mod.header.sample[channels_data[i].latched_sample - 1].LOOP_START) > 0) play_sample(channels_data[i].latched_sample, i, channels_data[i].latched_volume, -1, hz);
					else play_sample(channels_data[i].latched_sample, i, channels_data[i].latched_volume, 0, hz);
					channels_data[i].current_volume = channels_data[i].latched_volume;

				}

			}

		}

		if (verbose) {
		
		printf("%04uHz %02u %03u %X%02X", hz, sample_number, channels_data[i].latched_volume, effect_number, effect_param);

		putch(17);
		putch(7);
		printf("|");		

		}

		noteData += 4;

	}

}

void process_tick() {

	for (uint8_t i = 0; i < mod.channels; i++) {

		if (channels_data[i].current_effect != 0xFF)
		
			switch (channels_data[i].current_effect) {
				case 0x0A:
				{

					uint8_t slide_x = channels_data[i].current_effect_param >> 4;
					uint8_t slide_y = channels_data[i].current_effect_param & 0x0F;
					
					if (slide_x) printf("\r\nSlide tick on %u, increase by %u per tick.", i, slide_x);
					else printf("\r\nSlide tick on %u, decrease by %u per tick.", i, slide_y);

				}			
				default:
					break;

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

	set_channel_rate(-1, 8363);

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
	
	for (uint8_t i = 1; i < 31; i++) {

		uint16_t sample_length_swapped = swap_word(mod.header.sample[i - 1].SAMPLE_LENGTH);
		uint16_t sample_loop_start_swapped = swap_word(mod.header.sample[i - 1].LOOP_START);
		uint16_t sample_loop_length_swapped = swap_word(mod.header.sample[i - 1].LOOP_LENGTH);

		if (sample_length_swapped > 0) {
			printf("Uploading sample %u (%u bytes) with default volume %u and loop start %u\r\n", i, sample_length_swapped * 2, mod.header.sample[i - 1].VOLUME, sample_loop_start_swapped * 2);

			temp_sample_buffer = (uint8_t*) malloc(sizeof(uint8_t) * sample_length_swapped * 2);
			
			if (temp_sample_buffer == NULL) return 0;

			fread(temp_sample_buffer, sizeof(uint8_t), sample_length_swapped * 2, file);

			clear_buffer(i);
			add_stream_to_buffer(i, temp_sample_buffer, sample_length_swapped * 2);
			free(temp_sample_buffer);
			tuneable_sample_from_buffer(i, 8363);

		}

		if (sample_loop_start_swapped > 0) {

			set_sample_loop_start(i, sample_loop_start_swapped * 2);
			set_sample_loop_length(i, sample_loop_length_swapped * 2);

		}

	}
	
	bool verbose = true;

	ticker = 0;
	//timer0_begin(23040, 16);
	timer0_begin(23500, 16); //Slightly faster time to offset other cycles swallowed.
	uint8_t order = 0, row = 0;
	uint24_t old_ticker = ticker;
	uint8_t enabled = 255;
	uint16_t old_key_count = sv->vkeycount;
	
	printf("\r\nOrder %u (Pattern %u)\r\n", order, mod.header.order[order]);
	process_note(mod.pattern_buffer, mod.header.order[order], row++, verbose, enabled);

	uint8_t mid_tick = 0, tick = 0;

	while (1) {

		if ((ticker - old_ticker) >= mod.current_speed) {

			old_ticker = ticker, tick = ticker;
			mid_tick = 0;

			if (sv->vkeycount != old_key_count) {

				if (sv->keyascii == 27) break;
				
				if (sv->keyascii == '1') enabled = enabled ^ 0x01;
				if (sv->keyascii == '2') enabled = enabled ^ 0x02;
				if (sv->keyascii == '3') enabled = enabled ^ 0x04;
				if (sv->keyascii == '4') enabled = enabled ^ 0x08;
				if (sv->keyascii == '0') enabled = 255;

				old_key_count = sv->vkeycount;

			}

			process_note(mod.pattern_buffer, mod.header.order[order], row++, verbose, enabled);

			if (row == 64) {
				order++;
				if (order > mod.header.num_orders - 1) break;
				printf("\r\nOrder %u (Pattern %u)\r\n", order, mod.header.order[order]);
				row = 0;
			}
			
		} else if (ticker - tick > 0) { //We're in between rows

			if (mid_tick++ < mod.current_speed) {

				tick = ticker;
				printf("\r\nTick %u", mid_tick);

			}

		}

	}

	for (uint8_t i = 0; i < mod.channels; i++) reset_channel(i);

	free(channels_data);
	fclose(file);
	timer0_end();

	set_channel_rate(-1, 16843);

	return 0;
}