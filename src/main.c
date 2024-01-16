#include <agon/vdp_vdu.h>
#include <agon/vdp_key.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <mos_api.h>

//#define VARIABLE_RATE
#define VERBOSE

#define CHUNK_SIZE 1024		//Sample upload chunk size in bytes
#define PD_HZ 225000		//Magic number used to convert amiga periods to Agon frequencies (original 187815)
#define TIMER_NO 5			//Timer block to use in ez80

//ez80 defines

#define TMR0_CTL		0x80
#define TMR0_DR_L		0x81
#define TMR0_RR_L		0x81
#define TMR0_DR_H		0x82
#define TMR0_RR_H		0x82

#define PRT0_IVECT		0x0A

volatile void *timer_prevhandler;
volatile uint24_t ticker = 0;
extern void timer_handler_0();
extern void timer_handler_1();
extern void timer_handler_2();
extern void timer_handler_3();
extern void timer_handler_4();
extern void timer_handler_5();

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

static const uint16_t rr_array[] = {22500, 21818, 21176, 20571, 20000, 19459, 18947, 18461, 18000, 17560, 17142, 16744, 65454, 64000, 62608, 61276, 60000, 58775, 57600, 56470, 55384, 54339, 53333, 52363, 51428, 50526, 49655, 48813, 48000, 47213, 46451, 45714, 45000, 44307, 43636, 42985, 42352, 41739, 41142, 40563, 40000, 39452, 38918, 38400, 37894, 37402, 36923, 36455, 36000, 35555, 35121, 34698, 34285, 33882, 33488, 33103, 32727, 32359, 32000, 31648, 31304, 30967, 30638, 30315, 30000, 29690, 29387, 29090, 28800, 28514, 28235, 27961, 27692, 27428, 27169, 26915, 26666, 26422, 26181, 25945, 25714, 25486, 25263, 25043, 24827, 24615, 24406, 24201, 24000, 23801, 23606, 23414, 23225, 23040, 22857, 22677, 22500, 22325, 22153, 21984, 21818, 21654, 21492, 21333, 21176, 21021, 20869, 20719, 20571, 20425, 20281, 20139, 20000, 19862, 19726, 19591, 19459, 19328, 19200, 19072, 18947, 18823, 18701, 18580, 18461, 18343, 18227, 18113, 18000, 17888, 17777, 17668, 17560, 17454, 17349, 17245, 17142, 17041, 16941, 16842, 16744, 16647, 16551, 16457, 65454, 65084, 64719, 64357, 64000, 63646, 63296, 62950, 62608, 62270, 61935, 61604, 61276, 60952, 60631, 60314, 60000, 59689, 59381, 59076, 58775, 58477, 58181, 57889, 57600, 57313, 57029, 56748, 56470, 56195, 55922, 55652, 55384, 55119, 54857, 54597, 54339, 54084, 53831, 53581, 53333, 53087, 52844, 52602, 52363, 52126, 51891, 51659, 51428, 51200, 50973, 50748, 50526, 50305, 50086, 49870, 49655, 49442, 49230, 49021, 48813, 48607, 48403, 48200, 48000, 47800, 47603, 47407, 47213, 47020, 46829, 46639, 46451, 46265, 46080, 45896, 45714, 45533, 45354, 45176};
static const uint8_t div_array[] = {64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};

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
	bool sample_live[32];
	uint8_t sample_finetune[32];
	uint8_t sample_volume[32];	
	uint8_t sample_channel[32];
	bool bad_samples;
	uint24_t pd_hz;
	uint8_t tick_no;
	
} mod_header;

typedef struct {

	uint16_t latched_sample;
	int16_t latched_volume;
	int16_t current_volume;
	uint8_t current_effect;
	uint8_t current_effect_param;
	uint16_t current_period;
	uint16_t current_hz;
	uint16_t target_period;
	uint8_t slide_rate;
	uint24_t latched_offset;
	int8_t vibrato_position;
	uint8_t vibrato_speed;
	uint8_t vibrato_depth;
	bool vibrato_retrigger;
	bool tremolo_retrigger;
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

static mod_header mod;
static channel_data *channels_data = NULL;
static FILE *file;
static uint8_t old_mode;
static int16_t global_volume = 100;

// Parameters:
// - argc: Argument count
// - argv: Pointer to the argument string - zero terminated, parameters separated by spaces
//

static volatile SYSVAR *sv;
volatile uint8_t global_timer_ctl_no = 0x80; //default Timer 0

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

void timer_begin(uint8_t timer_no, uint16_t reload_value, uint16_t clk_divider) {

	//timer period (in SECONDS) = (reload_value * clk_divider) / system_clock_frequency (which is 18432000 Hz)
	//clk_divider can be 4, 16, 64 or 256
    
	unsigned char clkbits = 0;
    unsigned char ctl;

    
	if (timer_no == 0) timer_prevhandler = mos_setintvector(PRT0_IVECT + (timer_no * 2), timer_handler_0);
	else if (timer_no == 1) timer_prevhandler = mos_setintvector(PRT0_IVECT + (timer_no * 2), timer_handler_1);
	else if (timer_no == 2) timer_prevhandler = mos_setintvector(PRT0_IVECT + (timer_no * 2), timer_handler_2);
	else if (timer_no == 3) timer_prevhandler = mos_setintvector(PRT0_IVECT + (timer_no * 2), timer_handler_3);
	else if (timer_no == 4) timer_prevhandler = mos_setintvector(PRT0_IVECT + (timer_no * 2), timer_handler_4);
	else if (timer_no == 5) timer_prevhandler = mos_setintvector(PRT0_IVECT + (timer_no * 2), timer_handler_5);

    switch (clk_divider) {
        case 4:   clkbits = 0x00; break;
        case 16:  clkbits = 0x04; break;
        case 64:  clkbits = 0x08; break;
        case 256: clkbits = 0x0C; break;
    }
    ctl = 0x53 | clkbits; // Continuous mode, reload and restart enabled, and enable the timer    

    set_port(TMR0_CTL + (timer_no * 3), 0x00); // Disable the timer and clear all settings
	ticker = 0;
    set_port(TMR0_RR_L + (timer_no * 3), (unsigned char)(reload_value));
    set_port(TMR0_RR_H + (timer_no * 3), (unsigned char)(reload_value >> 8));
    set_port(TMR0_CTL + (timer_no * 3), ctl);

}

void timer_end(uint8_t timer_no) {
	
	set_port(TMR0_CTL + (timer_no * 3), 0x00);
	set_port(TMR0_RR_L + (timer_no * 3), 0x00);
	set_port(TMR0_RR_H + (timer_no * 3), 0x00);
	mos_setintvector(PRT0_IVECT + (timer_no * 2), timer_prevhandler);
	ticker = 0;

}

int test_bit(int num, int pos) {
    return (num & (1 << pos)) != 0;
}

void set_bit(int *num, int pos) {
    *num |= (1 << pos);
}

void toggle_bit(int *num, int pos) {
    *num ^= (1 << pos);
}

void clea_bit(int *num, int pos) {
    *num &= ~(1 << pos);
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

void clear_assets() {

	//VDU 23, 27, 16

	putch(23);
	putch(27);
	putch(16);

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

void draw_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {

	//MOVE x1,y1:MOVE x1+w,y1+h:PLOT 189,x2,y2
	//PLOT = VDU 25,k,x;y;
	//MOVE = VDU 25 4 x; y;

	putch(25);
	putch(4);
	write16bit(x1);
	write16bit(y1);

	putch(25);
	putch(101);
	write16bit(x2);
	write16bit(y2);	

}

void plot_point(uint16_t x1, uint16_t y1) {

	//PLOT 69,x,y

	putch(25);
	putch(69);
	write16bit(x1);
	write16bit(y1);

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

void switch_buffers() {

	//VDU 23, 0, &C3

	putch(23);
	putch(0);
	putch(0xC3);

}

void set_graphics_foreground(uint8_t colour) {

	putch(18);
	putch(0);
	putch(colour);

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

void set_text_window(uint8_t left, uint8_t bottom, uint8_t right, uint8_t top) {

	putch(0x1C);

	putch(left);
	putch(bottom);
	putch(right);
	putch(top);

}

void set_graphics_window(uint16_t left, uint16_t bottom, uint16_t right, uint16_t top) {

	putch(24);

	write16bit(left);
	write16bit(bottom);
	write16bit(right);
	write16bit(top);

}

bool verbose = true;
bool extra_verbose = false;

//Value:    0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
//Finetune: 0  +1  +2  +3  +4  +5  +6  +7  -8  -7  -6  -5  -4  -3  -2  -1

typedef struct {
    uint16_t tuned_period;
    const char* name;
} note_data;

const uint16_t periods[] = {856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113};

const int8_t finetune_offsets[][36] = {
	//{000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000},
	{-06,-06,-05,-05,-04,-03,-03,-03,-03,-03,-03,-03,-03,-03,-02,-03,-02,-02,-02,-01,-01,-01,-01,-01,-01,-01,-01,-01,-01,-01,-01,-01,-01,-01,-01,000},
	{-12,-12,-10,-11,-08,-08,-07,-07,-06,-06,-06,-06,-06,-06,-05,-05,-04,-04,-04,-03,-03,-03,-03,-02,-03,-03,-02,-03,-03,-02,-02,-02,-02,-02,-02,-01},
	{-18,-17,-16,-16,-13,-12,-12,-11,-10,-10,-10,-09,-09,-09,-08,-08,-07,-06,-06,-05,-05,-05,-05,-04,-05,-04,-03,-04,-04,-03,-03,-03,-03,-02,-02,-02},
	{-24,-23,-21,-21,-18,-17,-16,-15,-14,-13,-13,-12,-12,-12,-11,-10,-09,-08,-08,-07,-07,-07,-07,-06,-06,-06,-05,-05,-05,-04,-04,-04,-04,-03,-03,-03},
	{-30,-29,-26,-26,-23,-21,-20,-19,-18,-17,-17,-16,-15,-14,-13,-13,-11,-11,-10,-09,-09,-09,-08,-07,-08,-07,-06,-06,-06,-05,-05,-05,-05,-04,-04,-04},
	{-36,-34,-32,-31,-27,-26,-24,-23,-22,-21,-20,-19,-18,-17,-16,-15,-14,-13,-12,-11,-11,-10,-10,-09,-09,-09,-07,-08,-07,-06,-06,-06,-06,-05,-05,-04},
	{-42,-40,-37,-36,-32,-30,-29,-27,-25,-24,-23,-22,-21,-20,-18,-18,-16,-15,-14,-13,-13,-12,-12,-10,-10,-10,-09,-09,-09,-08,-07,-07,-07,-06,-06,-05},
	{051,048,046,042,042,038,036,034,032,030,028,027,025,024,023,021,021,019,018,017,016,015,014,014,012,012,012,010,010,010,009,008,008,008,007,007},
	{404,042,040,037,037,035,032,031,029,027,025,024,022,021,020,019,018,017,016,015,015,014,013,012,011,010,010,009,009,009,008,007,007,007,006,006},
	{038,036,034,032,031,030,028,027,025,024,022,021,019,018,017,016,016,015,014,013,013,012,011,011,009,009,009,008,007,007,007,006,006,006,005,005},
	{031,030,029,026,026,025,024,022,021,020,018,017,016,015,014,013,013,012,012,011,011,010,009,009,008,007,008,007,006,006,006,005,005,005,005,005},
	{025,024,023,021,021,020,019,018,017,016,014,014,013,012,011,010,011,010,010,009,009,008,007,007,006,006,006,005,005,005,005,004,004,004,003,004},
	{019,018,017,016,016,015,015,014,013,012,011,010,009,009,009,008,008,008,007,007,007,006,005,006,005,004,005,004,004,004,004,003,003,003,003,003},
	{012,012,012,010,011,011,010,010,009,008,007,007,006,006,006,005,006,005,005,005,005,004,004,004,003,003,003,003,002,003,003,002,002,002,002,002},
	{006,006,006,005,006,006,006,005,005,005,004,004,003,003,003,003,003,003,003,003,003,002,002,002,002,001,002,001,001,001,001,001,001,001,001,001},
};

uint16_t clamp_period(uint16_t period) {
    if (period <= 113) return 113;
    else if (period >= 856) return 856;
    else return period;
}

int16_t clamp_volume(int8_t volume) {
    if (volume <= 0) return 0;
    else if (volume >= 127) return 127;
    else return volume;
}

uint16_t finetune(uint16_t period, int8_t finetune) {
    size_t periods_count = sizeof(periods) / sizeof(periods[0]);
    size_t finetune_count = sizeof(finetune_values) / sizeof(finetune_values[0]);

    if (finetune < 0 || finetune >= finetune_count) {
        return 0;
    }

    for (size_t i = 0; i < periods_count; ++i) {
        if (periods[i] == period) {
            if (finetune == 0) return period;
			else return clamp_period(period + finetune_values[finetune][i]);
        }
    }

    return 0;
}

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

void handle_exit(const char* exit_message, bool tidy) {

	if (mod.channels) for (uint8_t i = 0; i <= mod.channels; i++) reset_channel(i);
	//for (uint8_t i = 1; i < mod.sample_total; i++) if (mod.sample_live[i]) clear_buffer(i);

	if (channels_data != NULL) free(channels_data);

	if (file != NULL) fclose(file);
	
	timer_end(TIMER_NO);

	#ifdef VARIABLE_RATE
	set_channel_rate(-1, 16384);
	#endif

	#ifdef VERBOSE

		clear_assets();

		putch(17);
		putch(15);
		printf("\r\n"); 

		if (tidy == true) {
			
			if (sv->scrMode != old_mode) {
				putch(22);
				putch(old_mode);
			}
		
			putch(26); //Reset viewports
			putch(0x0C); //CLS

		}

	#endif

	if (exit_message != NULL) printf("%s\r\n", exit_message);

}

void fill_empty(uint8_t rows) {
			
	putch(0x1E);

	for (uint8_t i = 0; i < rows; i++) {
		if (mod.channels == 4) {
			uint8_t c = 9;
			putch(17);
			putch(7);
			printf("--");
			putch(17);
			putch(c++);
			printf(" --- -- -- ---");
			putch(17);
			putch(c++);
			printf(" --- -- -- ---");
			putch(17);
			putch(c++);
			printf(" --- -- -- ---");
			putch(17);
			putch(c++);
			printf(" --- -- -- ---");
			printf("\r\n");
		} else if (mod.channels == 8) {
			uint8_t c = 9;
			putch(17);
			putch(c++);
			printf("-- --- --");
			putch(17);
			putch(c++);
			printf(" --- --");
			putch(17);
			putch(c++);
			printf(" --- --");
			putch(17);
			putch(c++);
			printf(" --- --");
			putch(17);
			putch(c++);
			printf(" --- --");
			putch(17);
			putch(c++);
			printf(" --- --");
			putch(17);
			putch(17);
			printf(" --- --");
			putch(17);
			putch(18); //Manual fix for colour black
			printf(" --- --");
			printf("\r\n");
		}
	}

}

void dispatch_channel(uint8_t i) {

		if (channels_data[i].current_effect == 0x0E) {

			if (channels_data[i].current_effect_param >> 4 == 0x05) {

				channels_data[i].current_period = finetune(channels_data[i].current_period, channels_data[i].current_effect_param & 0x0F);
				channels_data[i].current_hz = mod.pd_hz / channels_data[i].current_period;

			}

		}

		if (swap_word(mod.header.sample[channels_data[i].latched_sample - 1].LOOP_LENGTH) > 1) play_sample(channels_data[i].latched_sample, i, channels_data[i].current_volume, -1, channels_data[i].current_hz);
		else play_sample(channels_data[i].latched_sample, i, channels_data[i].current_volume, 0, channels_data[i].current_hz);
		if (channels_data[i].current_effect == 0x09) set_position(i, channels_data[i].latched_offset);
		mod.sample_volume[channels_data[i].latched_sample] = channels_data[i].current_volume;

}

void volume_slide(uint8_t i, uint8_t effect_param) {

	uint8_t slide_x = effect_param >> 4;
	uint8_t slide_y = effect_param & 0x0F;

	if (slide_x) {

		uint8_t slide_adjusted = (slide_x * 2) - 1;
		channels_data[i].current_volume += slide_adjusted;
		if (channels_data[i].current_volume > 127) channels_data[i].current_volume = 127;
		if (extra_verbose) printf("\r\nSlide tick on %u, increase by %u (%u) to %u.", i, slide_x, slide_adjusted, channels_data[i].current_volume);
		set_volume(i, channels_data[i].current_volume);
		mod.sample_volume[channels_data[i].latched_sample] = channels_data[i].current_volume;

	} else if (slide_y) {

		uint8_t slide_adjusted = (slide_y * 2) - 1;
		channels_data[i].current_volume -= slide_adjusted;
		if (channels_data[i].current_volume < 0) channels_data[i].current_volume = 0;
		if (extra_verbose) printf("\r\nSlide tick on %u, decrease by %u (%u) to %u.", i, slide_y, slide_adjusted, channels_data[i].current_volume);
		set_volume(i, channels_data[i].current_volume);
		mod.sample_volume[channels_data[i].latched_sample] = channels_data[i].current_volume;
	}		

}

void fine_volume_slide_up(uint8_t i, uint8_t vol) {

		uint8_t slide_adjusted = (vol * 2) - 1;
		channels_data[i].current_volume += slide_adjusted;
		if (channels_data[i].current_volume > 127) channels_data[i].current_volume = 127;
		if (extra_verbose) printf("\r\nSlide tick on %u, increase by %u (%u) to %u.", i, vol, slide_adjusted, channels_data[i].current_volume);
		set_volume(i, channels_data[i].current_volume);
		mod.sample_volume[channels_data[i].latched_sample] = channels_data[i].current_volume;	

}

void fine_volume_slide_down(uint8_t i, uint8_t vol) {

	uint8_t slide_adjusted = (vol * 2) - 1;
	channels_data[i].current_volume -= slide_adjusted;
	if (channels_data[i].current_volume < 0) channels_data[i].current_volume = 0;
	if (extra_verbose) printf("\r\nSlide tick on %u, decrease by %u (%u) to %u.", i, vol, slide_adjusted, channels_data[i].current_volume);
	set_volume(i, channels_data[i].current_volume);
	mod.sample_volume[channels_data[i].latched_sample] = channels_data[i].current_volume;

}

void pitch_slide_down(uint8_t i, uint8_t period) {

	channels_data[i].current_period = clamp_period(channels_data[i].current_period + period);
	set_frequency(i, mod.pd_hz / channels_data[i].current_period);
	if (extra_verbose) printf("\r\nSlide period down %u to %u (%uHz)", period, channels_data[i].current_period, mod.pd_hz / channels_data[i].current_period);

}

void pitch_slide_up(uint8_t i, uint8_t period) {

	channels_data[i].current_period = clamp_period(channels_data[i].current_period - period);
	set_frequency(i, mod.pd_hz / channels_data[i].current_period);
	if (extra_verbose) printf("\r\nSlide period up %u to %u (%uHz)", period, channels_data[i].current_period, mod.pd_hz / channels_data[i].current_period);

}

void pitch_slide_directional(uint8_t i) {

	if (channels_data[i].target_period) {

		if (channels_data[i].target_period > channels_data[i].current_period) {

			if (extra_verbose) printf("Sliding period %u up %u toward %u on channel %u\r\n", channels_data[i].current_period, channels_data[i].slide_rate, channels_data[i].target_period, i);
			channels_data[i].current_period = channels_data[i].current_period + channels_data[i].slide_rate;
			if (channels_data[i].current_period > channels_data[i].target_period) channels_data[i].current_period = channels_data[i].target_period;
			if (channels_data[i].current_period > 856) channels_data[i].current_period = 856;
			set_frequency(i, mod.pd_hz / channels_data[i].current_period);
			if (channels_data[i].current_period >= channels_data[i].target_period) channels_data[i].target_period = 0;

		} else if (channels_data[i].target_period < channels_data[i].current_period) {

			if (extra_verbose) printf("Sliding period %u down %u toward %u on channel %u\r\n", channels_data[i].current_period, channels_data[i].slide_rate, channels_data[i].target_period, i);
			channels_data[i].current_period = channels_data[i].current_period - channels_data[i].slide_rate;
			if (channels_data[i].current_period < channels_data[i].target_period) channels_data[i].current_period = channels_data[i].target_period;
			if (channels_data[i].current_period < 113) channels_data[i].current_period = 113;
			set_frequency(i, mod.pd_hz / channels_data[i].current_period);
			if (channels_data[i].current_period <= channels_data[i].target_period) channels_data[i].target_period = 0;

		} else if (channels_data[i].target_period == channels_data[i].current_period) {

			channels_data[i].target_period = 0;

		}

	}

}

void do_vibrato(uint8_t i) {

	uint16_t delta = sine_table[channels_data[i].vibrato_position & 31];
	delta *= channels_data[i].vibrato_depth;
	delta >>= 7; //Divide by 128

	if (channels_data[i].vibrato_position < 0) set_frequency(i, mod.pd_hz / clamp_period(channels_data[i].current_period - delta));
	else if (channels_data[i].vibrato_position >= 0) set_frequency(i, mod.pd_hz / clamp_period(channels_data[i].current_period + delta));					

	if (extra_verbose) printf("\r\nVibrato on %u with speed %u and depth %u, sine pos %i meaning delta %u.", i, channels_data[i].vibrato_speed, channels_data[i].vibrato_depth, channels_data[i].vibrato_position, delta);

	channels_data[i].vibrato_position += channels_data[i].vibrato_speed;
	if (channels_data[i].vibrato_position > 31) channels_data[i].vibrato_position -= 64;

}

void do_tremulo(uint8_t i) {

	uint16_t delta = sine_table[channels_data[i].tremolo_position & 31];
	delta *= channels_data[i].tremolo_depth;
	delta >>= 6; //Divide by 64

	if (channels_data[i].tremolo_position < 0) set_volume(i, channels_data[i].current_volume - (delta * 2) - 1);
	else if (channels_data[i].tremolo_position >= 0) set_volume(i, channels_data[i].current_volume + (delta * 2) - 1);
	if (channels_data[i].current_volume > 127) channels_data[i].current_volume = 127;
	if (channels_data[i].current_volume < 0) channels_data[i].current_volume = 0;
	
	mod.sample_volume[channels_data[i].latched_sample] = channels_data[i].current_volume;

	//if (extra_verbose) printf("\r\nTremolo on %u with speed %u and depth %u, sine pos %i meaning delta %u.", i, channels_data[i].tremolo_speed, channels_data[i].tremolo_depth, channels_data[i].tremolo_position, delta);

	channels_data[i].tremolo_position += channels_data[i].tremolo_speed;
	if (channels_data[i].tremolo_position > 31) channels_data[i].tremolo_position -= 64;

}

void process_note(uint8_t *buffer, size_t pattern_no, size_t row)  {

	size_t offset = (mod.channels * 4 * 64) * pattern_no + (row * 4 * mod.channels);

	uint8_t *noteData = buffer + offset;

	mod.tick_no = 0;

	//mod.pattern_break_pending = false;

	#ifdef VERBOSE
	
		//putch(26);
		//set_text_window(20,29,80,2);

		if (row != 0) {
			putch(17);
			putch(15); // White row text
			putch(17);
			putch(128); //Black row background
			printf("%02u ", row);
		} else {
			putch(17);
			putch(0); //Black row text
			putch(17);
			putch(7 + 128); //Grey row background
			printf("%02X ", mod.current_order);
		}

	#endif

	uint8_t sample_number;
	uint8_t effect_number;
	uint8_t effect_param;
	uint16_t period;

	for (uint8_t i = 0; i < mod.channels; i++) {

		#ifdef VERBOSE

			putch(17);
			uint8_t new_colour = 9+i;
			if (new_colour == 16) new_colour++;
			putch(new_colour);
			
		#endif

		sample_number = (noteData[0] & 0xF0) + (noteData[2] >> 4);
		effect_number = (noteData[2] & 0xF);
		effect_param = noteData[3];
		period = ((uint16_t)(noteData[0] & 0xF) << 8) | (uint16_t)noteData[1];

		if (effect_number == 0x03 || effect_number == 0x05) {
			if (period && effect_number == 0x03) channels_data[i].target_period = finetune(period, mod.header.sample[sample_number - 1].FINE_TUNE);				//Log the note as effect 3/5's target, but don't use it now.
			if (effect_param > 0 && effect_number == 0x03) channels_data[i].slide_rate = effect_param;	//If effect 3 has a parameter, use it as slide rate.
		} else if (period > 0) {
			channels_data[i].current_period = period;
		}

		channels_data[i].current_hz = channels_data[i].current_period > 0 ? mod.pd_hz / clamp_period(channels_data[i].current_period) : 0;

		if (effect_param || effect_number) {
			channels_data[i].current_effect = effect_number;
			channels_data[i].current_effect_param = effect_param;
		} else channels_data[i].current_effect = 0xFF;

		if (sample_number > 0) {

			if (channels_data[i].latched_sample != sample_number) {
				mod.sample_volume[channels_data[i].latched_sample] = 0;
				mod.sample_channel[channels_data[i].latched_sample] = i;
			}
			channels_data[i].latched_sample = sample_number;
			mod.sample_channel[channels_data[i].latched_sample] = i;
			channels_data[i].latched_volume = clamp_volume((mod.header.sample[sample_number - 1].VOLUME * 2) - 1);	
			channels_data[i].current_volume = channels_data[i].latched_volume;
			set_volume(i, channels_data[i].current_volume);

			if (period > 0 && (effect_number != 0x03) && (effect_number != 0x05)) {

				if (effect_number == 0x09) channels_data[i].latched_offset = effect_param << 8;
				
				dispatch_channel(i);

			} else if (period > 0 && ((effect_number == 0x03) || (effect_number == 0x05)) && swap_word(mod.header.sample[channels_data[i].latched_sample - 1].LOOP_LENGTH) > 1) {

				channels_data[i].latched_offset = swap_word(mod.header.sample[channels_data[i].latched_sample - 1].LOOP_LENGTH) * 2;
				
				dispatch_channel(i);

			}

		} else if ((period > 0) && (effect_number != 0x03) && (effect_number != 0x05)) {

			if (channels_data[i].latched_sample > 0) {

				if (effect_number == 0x09) channels_data[i].latched_offset = effect_param << 8;
				
				dispatch_channel(i);

			}

		}

		noteData += 4;

		//Process effects that should happen immediately

		if (channels_data[i].current_effect != 0xFF) {

			switch (channels_data[i].current_effect) {

				case 0x04: {//Vibrato

					uint8_t param_x = channels_data[i].current_effect_param >> 4;
					uint8_t param_y = channels_data[i].current_effect_param & 0x0F;
					
					if (channels_data[i].vibrato_retrigger == true) channels_data[i].vibrato_position = 0;
					if (param_x > 0) channels_data[i].vibrato_speed = param_x;
					if (param_y > 0) channels_data[i].vibrato_depth = param_y;

				} break;	

				case 0x07: {//Tremolo

					uint8_t param_x = channels_data[i].current_effect_param >> 4;
					uint8_t param_y = channels_data[i].current_effect_param & 0x0F;
					
					if (channels_data[i].tremolo_retrigger == true) channels_data[i].tremolo_position = 0;
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
					mod.sample_volume[channels_data[i].latched_sample] = channels_data[i].current_volume;
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


				case 0x0E: {//Extended functions

					uint8_t param_x = channels_data[i].current_effect_param >> 4;
					uint8_t param_y = channels_data[i].current_effect_param & 0x0F;
					
					switch (param_x) {

						case 0x01: {

							pitch_slide_up(i, param_y);

						} break;

						case 0x02: {

							pitch_slide_down(i, param_y);

						} break;						
						
						case 0x04: {//Set waveform (vibrato)

							if (param_y == 0) channels_data[i].vibrato_retrigger = true;
							else if (param_y == 4) channels_data[i].vibrato_retrigger = false;

						} break;

						case 0x05: {//Override finetune

							if (period) {

								

							}

						} break;						

						case 0x07: {//Set waveform (tremolo)

							if (param_y == 0) channels_data[i].tremolo_retrigger = true;
							else if (param_y == 4) channels_data[i].tremolo_retrigger = false;

						} break;

						case 0x0A: {

							fine_volume_slide_up(i, param_y);

						} break;

						case 0x0B: {

							fine_volume_slide_down(i, param_y);

						} break;										
						
						case 0x0D: {//Delay sample change

							//TODO

						} break;										

					}


				} break;				

				case 0x0F: {//Set speed or tempo

					if (channels_data[i].current_effect_param < 0x20) { //<0x20 means speed (i.e. ticks per row)

						mod.current_speed = channels_data[i].current_effect_param;
						if (extra_verbose) printf("\r\nTempo set to %u.", channels_data[i].current_effect_param);

					}

					else if (channels_data[i].current_effect_param >= 0x20) { //<0x20 means bpm/tempo (i.e. ticker period)

						mod.current_bpm = channels_data[i].current_effect_param;
						timer_end(TIMER_NO);
						timer_begin(TIMER_NO, rr_array[mod.current_bpm - 0x20], div_array[mod.current_bpm - 0x20]);

					}

				} break;				

				default:
					break;

			}		

		}

		#ifdef VERBOSE

			if (mod.channels == 4) {
			
				//## FFF SS VV EEE FFF SS VV EEE FFF SS VV EEE FFF SS VV EEE
				printf("%s %02u %02u %X%02X", period_to_note(period), sample_number, (channels_data[i].current_volume / 2) + 1, effect_number, effect_param);
				if (i != mod.channels - 1) printf(" ");

			} else if (mod.channels == 6) {

				//## FFF SS FFF SS FFF SS FFF SS FFF SS FFF SS
				printf("%s %02u %02u %X%02X", period_to_note(period), sample_number, (channels_data[i].current_volume / 2) + 1, effect_number, effect_param);
				if (i != mod.channels - 1) printf(" ");			


			} else if (mod.channels == 8) {
				
				//## FFF SS FFF SS FFF SS FFF SS FFF SS FFF SS FFF SS FFF SS
				printf("%s %02u", period_to_note(period), sample_number);
				if (i != mod.channels - 1) printf(" ");
				
			}

		#endif

	}

	#ifdef VERBOSE
	putch(17);
	putch(128); //Black row background	
	printf(" \r\n");

	set_graphics_foreground(7);

	draw_rect(178,      18,180,      230);

	if (mod.channels == 4) {
		draw_rect(290,      18,292,      230);
		draw_rect(282 + 120,18,284 + 120,230);
		draw_rect(274 + 240,18,276 + 240,230);
	} else if (mod.channels == 8) {
		for (uint8_t i = 0; i < 7; i++) {
			uint16_t offset = 56 * i;
			draw_rect(234 + offset, 18, 234 + offset + 2, 230);
		}													
	}

	#endif

}

void process_tick() {

	for (uint8_t i = 0; i < mod.channels; i++) {

		if (channels_data[i].current_effect != 0xFF) {

			switch (channels_data[i].current_effect) {

				case 0x00: { //Arpeggio

					uint8_t x = channels_data[i].current_effect_param >> 4;
					uint8_t y = channels_data[i].current_effect_param & 0x0F;

					uint8_t r = mod.tick_no % 3;
					if (r == 0) {
						set_frequency(i, mod.pd_hz / clamp_period(channels_data[i].current_period));
					}
					else if (r == 1) {
						set_frequency(i, mod.pd_hz / clamp_period(channels_data[i].current_period) + (x * 8));
					}
					else if (r == 2) {
						set_frequency(i, mod.pd_hz / clamp_period(channels_data[i].current_period) + (y * 8));
					}

				} break;

				case 0x01: { //Pitch slide (porta) up

					pitch_slide_up(i, channels_data[i].current_effect_param);

				} break;

				case 0x02: { //Pitch slide (porta) down	
					
					pitch_slide_down(i, channels_data[i].current_effect_param);

				} break;

				case 0x03: { //Pitch slide toward target note (tone portamento)

					pitch_slide_directional(i);

				} break;				

				case 0x04: {//Vibrato
					
					do_vibrato(i);

				} break;

				case 0x05: {//Volume Slide + Tone Portamento
					
					volume_slide(i, channels_data[i].current_effect_param);

					pitch_slide_directional(i);

				} break;	

				case 0x06: {//Volume Slide + Vibrato

					volume_slide(i, channels_data[i].current_effect_param);					

					do_vibrato(i);

				} break;	

				case 0x07: {//Tremolo
					
					do_tremulo(i);

				} break;				

				case 0x0A: { //Volume slide

					volume_slide(i, channels_data[i].current_effect_param);

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

	putch(17);
	putch(0); //Black foreground
	putch(17);
	putch(128 + 7); //Grey background
	if (mod.channels == 4) printf("## Frq Sa Vo Eff Frq Sa Vo Eff Frq Sa Vo Eff Frq Sa Vo Eff"); //58 Chars long (22 from left)
	else if (mod.channels == 6) printf("## Frq Sa Frq Sa Frq Sa Frq Sa Frq Sa Frq Sa");	
	else if (mod.channels == 8) printf("## Frq Sa Frq Sa Frq Sa Frq Sa Frq Sa Frq Sa Frq Sa Frq Sa"); //58 Chars long (22 from left)
	putch(17);
	putch(128 + 0); //Reset to black background
	//printf("          \r\n");
	
}

void logical_coords(bool on) {

	//VDU 23, 0, &C0, n
	putch(23);
	putch(0);
	putch(0xC0);
	putch(on);

}

void scroll_graphics_left(uint8_t scroll_x) {

	//VDU 23, 7
	putch(23);
	putch(7);
	putch(2); //Scroll graphics viewport
	putch(1); //Scroll left
	putch(scroll_x); //Scroll by scroll_x

}

void draw_sample_bars() {

	switch_buffers();
	set_graphics_window(0,239,639,0); //Ensure graphics window is full

	uint8_t top_offset = 52;

	set_graphics_foreground(0);
	
	draw_rect(19,top_offset + 8,55,top_offset + 8  + ((1 + mod.sample_total) * 4)); //Clear first column
	draw_rect(89,top_offset + 8,160,top_offset + 8 + ((1 + mod.sample_total) * 4)); //Clear second column
	
	set_graphics_foreground(7);

	draw_rect(25,32 + 16,154,38 + 16); //Master volume back bar
	
	set_graphics_foreground(15);

	draw_rect(26,33 + 16,26 + global_volume,37 + 16); //Master volume front bar

	for (uint8_t i = 1, j = 0; i < 32;) {

		while (i < 32 && !mod.sample_live[i]) {
			i++;
		}

		if (i < 32) {
			j++;

			if (mod.sample_volume[i] > 0) {
				set_graphics_foreground(9 + mod.sample_channel[i]);
				draw_rect(20, top_offset + (j * 8) + 6, 20 + (mod.sample_volume[i] / 4), top_offset + (j * 8) + 8);
			}

			i++;

			while (i < 32 && !mod.sample_live[i]) {
				i++;
			}

			if (i < 32 && mod.sample_volume[i] > 0) {
				set_graphics_foreground(9 + mod.sample_channel[i]);
				draw_rect(92, top_offset + (j * 8) + 6, 92 + (mod.sample_volume[i] / 4), top_offset + (j * 8) + 8);
			}

			i++;
		} else break;

	}
	
	//Volume area to scroll is (x1,y1,x2,y2) 

	set_graphics_window(12,230,150,198); //Left, bottom, right, top, remember

	//Workaround for scrolling bug
	set_graphics_foreground(7);
	draw_rect(0,0,1,1);

	scroll_graphics_left(2);

	uint16_t mean_vol = 0;
	for (uint8_t i = 0; i < mod.channels; i++) {
		mean_vol += channels_data[i].current_volume;
	}
	mean_vol /= mod.channels;
	mean_vol /= 4;

	// set_graphics_foreground(15);
	// draw_rect(154,228, 150,232 - max_vol); //Draw a max bar (white)

	set_graphics_foreground(14);
	draw_rect(154,228, 148,230 - mean_vol); //Draw a mean bar (cyan)
	
	set_graphics_window(0,239,639,0); //Ensure graphics window is full

}

int main(int argc, char * argv[])
{

	verbose = true;
	extra_verbose = false;
	
	sv = vdp_vdu_init();
	if ( vdp_key_init() == -1 ) return 1;

	if (argc < 2) {
		handle_exit("Usage is playmod <file>", false);
		return 0;
	}

	if (argc == 3) mod.pd_hz = atoi(argv[2]);
	else mod.pd_hz = PD_HZ;

	file = fopen(argv[1], "rb");
    if (file == NULL) {
        handle_exit("Could not open file.", false);
		return 0;
    }

	#ifdef VERBOSE

		old_mode = sv->scrMode;

		if (sv->scrMode != 4) {
			putch(22);
			putch(4);
		}

		logical_coords(false);

	#endif

	set_volume(255, global_volume);

	fread(&mod.header, sizeof(mod_file_header), 1, file);

	if (strncmp(mod.header.sig, "M.K.", 4) == 0) {
		mod.channels = 4; //Classic 4 channels
		#ifdef VARIABLE_RATE
		set_channel_rate(-1, 32768);
		#endif
	}
	else if (strncmp(mod.header.sig, "FLT4", 4) == 0) {
		mod.channels = 4; //Startrekker 4 channels
		#ifdef VARIABLE_RATE
		set_channel_rate(-1, 32768);
		#endif		
	}
	else if (strncmp(mod.header.sig, "6CHN", 4) == 0) {
		mod.channels = 6; //6 channels
		#ifdef VARIABLE_RATE
		set_channel_rate(-1, 24576);
		#endif		
	}
	else if (strncmp(mod.header.sig, "8CHN", 4) == 0) {
		mod.channels = 8; //8 channels
	}
	else if (strncmp(mod.header.sig, "28CH", 4) == 0) {
		mod.channels = 28;//28 channels
		#ifdef VARIABLE_RATE
		set_channel_rate(-1, 8192);
		#endif				
		 verbose = false;
	}
	else {

		handle_exit("Unknown .mod format, only 4 or 8 channel .MODs are supported.", false);
		return 0;

	}

	channels_data = (channel_data*) malloc(sizeof(channel_data) * mod.channels);
	for (uint8_t i = 0; i < mod.channels; i++) {
		enable_channel(i);
		channels_data[i].latched_sample = 0;
		channels_data[i].latched_volume = 0;
		channels_data[i].current_effect = 0xFF;
		channels_data[i].current_effect_param = 0;
		channels_data[i].vibrato_retrigger = true;
		channels_data[i].tremolo_retrigger = true;		
		
	}
	mod.pattern_max = 0;
	mod.current_speed = 6;
	mod.current_bpm = 125;

	ticker = 0;
	timer_begin(TIMER_NO, rr_array[mod.current_bpm - 0x20], div_array[mod.current_bpm - 0x20]);

	mod.pattern_break_pending = false;
	mod.order_break_pending = false;

	for (uint8_t i = 1; i < 31; i++) mod.sample_volume[i] = 0;

	for (uint8_t i = 0; i < 127; i++) if (mod.header.order[i] > mod.pattern_max) mod.pattern_max = mod.header.order[i];

	//Number of patterns * number of channels * number of bytes per note per channel * number of notes per pattern (i.e. 1024 bytes per 4 channel pattern)
	mod.pattern_buffer = (uint8_t*) malloc(sizeof(uint8_t) * (mod.pattern_max + 1) * mod.channels * 4 * 64);
	fread(mod.pattern_buffer, sizeof(uint8_t), (mod.pattern_max + 1) * mod.channels * 4 * 64, file);

	// if (extra_verbose) printf("Module name: %s\r\n", mod.header.name);
	// if (extra_verbose) printf("Song length: %u\r\n", mod.header.num_orders);
	// if (extra_verbose) printf("Mod patterns: %u\r\n", mod.pattern_max);
	// if (extra_verbose) printf("Module signature: %c%c%c%c (%u channels)\r\n", mod.header.sig[0], mod.header.sig[1], mod.header.sig[2], mod.header.sig[3], mod.channels);
	// if (extra_verbose) printf("Pattern buffer size: %u Bytes\r\n", (mod.pattern_max + 1) * mod.channels * 4 * 64);	

	uint8_t *temp_sample_buffer;
	mod.sample_total = 0;
	
	for (uint8_t i = 1; i < 31; i++) {

		uint16_t sample_length_swapped = swap_word(mod.header.sample[i - 1].SAMPLE_LENGTH);
		uint16_t sample_loop_start_swapped = swap_word(mod.header.sample[i - 1].LOOP_START);
		uint16_t sample_loop_length_swapped = swap_word(mod.header.sample[i - 1].LOOP_LENGTH);

		if (sample_length_swapped > 0) {

			mod.sample_total++;
			mod.sample_live[i] = true;

			// if (1) {
			// 	printf("Uploading sample %u", i);
			// 	printf(" %02u bytes, def. vol %02X", (sample_length_swapped * 2), mod.header.sample[i - 1].VOLUME);			
			// 	printf(", loop start %05u", sample_loop_start_swapped * 2);
			// 	printf(", loop length %05u", sample_loop_length_swapped * 2);
			// 	printf("\r\n");
			// 	untidy_handle_exit(NULL);
			//	return 0;
			// }

			if ((sample_length_swapped * 2) < 300) mod.bad_samples = true;

			if ((sample_length_swapped * 2) <= CHUNK_SIZE) {

				clear_buffer(i);
				temp_sample_buffer = (uint8_t*) malloc(sizeof(uint8_t) * (sample_length_swapped * 2));
				if (temp_sample_buffer == NULL) {
					handle_exit("Local sample memory allocation failed", false);
					return 0;	
				}
				fread(temp_sample_buffer, sizeof(uint8_t), (sample_length_swapped * 2), file);
				add_stream_to_buffer(i, (char *)temp_sample_buffer, (sample_length_swapped * 2));		

			} else {

				uint24_t remaining_data = sample_length_swapped * 2;
				uint16_t chunk;

				clear_buffer(i);
				temp_sample_buffer = (uint8_t*) malloc(sizeof(uint8_t) * CHUNK_SIZE);
				if (temp_sample_buffer == NULL) {
					handle_exit("Local sample memory allocation failed", false);
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
		}

			tuneable_sample_from_buffer(i, 8363);

			if (sample_loop_length_swapped > 1) {

				set_sample_loop_start(i, sample_loop_start_swapped * 2);
				set_sample_loop_length(i, sample_loop_length_swapped * 2);

			} //else set_sample_loop_length(i, sample_loop_length_swapped * 2);

		} else mod.sample_live[i] = false;

	}

	mod.sample_live[0] = false; //No sample #0

	free(temp_sample_buffer);
	
	//timer_begin(TIMER_NO, 23040, 16); //0.02 seconds
	mod.current_order = 0, mod.current_row = 0;
	uint24_t old_ticker = ticker;
	uint16_t old_key_count = sv->vkeycount;
	
	//printf("\r\nOrder %u (Pattern %u)\r\n", mod.current_order, mod.header.order[mod.current_order]);
	
	#ifdef VERBOSE

		putch(0x0C); //CLS

		//Set up the UI
		//left, bottom, right, top
		set_text_window(0,29,22,1);

		printf("Agon_MOD");
		if (mod.pd_hz != PD_HZ) printf(" [%06u]", mod.pd_hz);
		printf("\r\n\r\n");
		printf("Mod title:\r\n%.20s\r\n\r\nVol \r\n\r\n", mod.header.name);
	
		for (uint8_t i = 1; i < 32;) {

			while (i < 32 && !mod.sample_live[i]) i++;

			if (i < 32) {

				if (mod.sample_live[i]) printf("%02u       ", i);
				i++;

				while (i < 32 && !mod.sample_live[i]) i++;

				if (mod.sample_live[i]) printf("%02u\r\n", i);
				i++;

			} else break;

		}

		if (mod.bad_samples) {
			printf("\r\n\r\nNOTE: Tiny samples\r\ndetected. These may\r\nnot play well (yet)");
		}

		set_graphics_foreground(7);
		//draw_rect(8,180,152,230); //Volume background
		draw_rect(8,198,152,230); //Volume background
		
		set_text_window(20,2,80,1);

		header_line();

		set_text_window(20,29,80,2);

		fill_empty(27);

	#endif

	process_note(mod.pattern_buffer, mod.header.order[mod.current_order], mod.current_row++);

	#ifdef VERBOSE
	draw_sample_bars();
	#endif

	uint24_t tick = 0;
	uint8_t mid_tick;

	bool do_ticks = true;

	while (1) {

		if ((ticker - old_ticker) >= mod.current_speed) {

			old_ticker = ticker, tick = ticker;
			mid_tick = 0;

			if (sv->vkeycount != old_key_count) {

				if (sv->keyascii == 27) {
					break;
				}

				else if (sv->keyascii == 'q') {
					break;
				}

				else if (sv->keyascii == '`') {
					return 0; //To stop dead and examine the rows.
				}				

				else if (sv->keyascii == '+') {
					global_volume += 5;
					if (global_volume > 127) global_volume = 127;
					set_volume(255, global_volume);
				}
				else if (sv->keyascii == '-') {
					global_volume -= 5;
					if (global_volume < 0) global_volume = 0;
					set_volume(255, global_volume);
				}

				old_key_count = sv->vkeycount;

			}

			if (mod.order_break_pending && mod.order_break_pending) { //Combined order and pattern breaks (0x0B and 0x0D on the same row)

				mod.current_order = mod.new_order;
				mod.current_row = mod.new_row;
				mod.order_break_pending = false;
				mod.pattern_break_pending = false;
				
				//printf("\r\nOrder %u (Pattern %u)\r\n", mod.current_order, mod.header.order[mod.current_order]);
				//header_line();

			} else if (mod.order_break_pending) { //Just an order break (0x0B)

				mod.current_order = mod.new_order;
				mod.current_row = 0;
				mod.order_break_pending = false;

				//printf("\r\nOrder %u (Pattern %u)\r\n", mod.current_order, mod.header.order[mod.current_order]);
				//header_line();

			} else if (mod.pattern_break_pending) { //Just a pattern break (0x0D)

				mod.current_order++;
				mod.current_row = mod.new_row;
				mod.pattern_break_pending = false;

				//printf("\r\nOrder %u (Pattern %u)\r\n", mod.current_order, mod.header.order[mod.current_order]);
				//header_line();

			}			

			process_note(mod.pattern_buffer, mod.header.order[mod.current_order], mod.current_row++);
			
			#ifdef VERBOSE
			draw_sample_bars();
			#endif

			if (mod.current_row == 64) {

				mod.current_order++;
				if (mod.current_order >= mod.header.num_orders - 1) mod.current_order = 0;
				//printf("\r\nOrder %u (Pattern %u)\r\n", mod.current_order, mod.header.order[mod.current_order]);
				//header_line();
				mod.current_row = 0;

			}

		} else if (ticker - tick > 0) { //We're in between rows

			if (mid_tick++ < mod.current_speed) {

				tick = ticker;
				//printf("\r\nTick %u", mid_tick);
				if (do_ticks) {
					mod.tick_no++;
					process_tick();
					#ifdef VERBOSE
					draw_sample_bars();
					#endif
				}

			}

		}
	
	}

	handle_exit(NULL, true);
	return 0;

}