#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#ifndef HEADLESS
#include <agon/vdp_vdu.h>
#include <agon/vdp_key.h>
#include <mos_api.h>
#else
// Placeholder definitions for local compilation
typedef uint32_t uint24_t;
typedef struct { 
    uint8_t scrMode;
    uint16_t vkeycount;
    uint8_t keyascii;
    void* ptr; 
} SYSVAR;

// Dummy function prototypes for local compilation
void mos_setintvector(int vector, void* handler);
void mos_puts(const char* str, uint16_t size, uint8_t flag);
int vdp_vdu_init(void);
int vdp_key_init(void);
void handle_exit(const char* message, bool cleanup);
void set_position(uint8_t channel, uint24_t position);
void set_sample_loop_start(uint16_t sample_id, uint24_t start);
void set_sample_loop_length(uint16_t sample_id, uint24_t length);
void logical_coords(bool on);
void cursor_set(bool state);

// Additional function prototypes for local compilation
void putch(uint8_t c);
void timer_begin(uint8_t timer_no, uint16_t reload_value, uint16_t clk_divider);
void timer_end(uint8_t timer_no);
void set_volume(uint8_t channel, uint8_t volume);
void set_frequency(uint8_t channel, uint16_t frequency);
void assign_sample_to_channel(uint16_t sample_id, uint8_t channel_id);
void dispatch_channel(uint8_t i);

// Dummy implementations for local compilation
void putch(uint8_t c) { (void)c; /* Dummy implementation */ }
void timer_begin(uint8_t timer_no, uint16_t reload_value, uint16_t clk_divider) { (void)timer_no; (void)reload_value; (void)clk_divider; /* Dummy implementation */ }
void timer_end(uint8_t timer_no) { (void)timer_no; /* Dummy implementation */ }
void set_volume(uint8_t channel, uint8_t volume) { (void)channel; (void)volume; /* Dummy implementation */ }
void set_frequency(uint8_t channel, uint16_t frequency) { (void)channel; (void)frequency; /* Dummy implementation */ }
void assign_sample_to_channel(uint16_t sample_id, uint8_t channel_id) { (void)sample_id; (void)channel_id; /* Dummy implementation */ }
void dispatch_channel(uint8_t i) { (void)i; /* Dummy implementation */ }
void mos_puts(const char* str, uint16_t size, uint8_t flag) { (void)str; (void)size; (void)flag; /* Dummy implementation */ }
int vdp_vdu_init(void) { return 0; /* Dummy implementation */ }
int vdp_key_init(void) { return 0; /* Dummy implementation */ }
#endif

#define VERSION 11

//Settings

//#define VARIABLE_RATE //Enables variable system sample rate setting based on channel count - does not work reliably on < C8 2.5.0
#define RATE_4_CHAN 32768
#define RATE_6_CHAN 24576
#define RATE_8_CHAN 8192

//#define HEADLESS //Will dump out into MOS after beginning to play - unstable!
#define VERBOSE //Enables normal visual tracker-style output
//#define VIZ //Crude visualation
//#define PRINT_DEBUG //Enables debug output to "printer" requiring C8 firmware 2.5.0

// VIZ and VERBOSE can both be defined for testing purposes

#define BUFFER_FLOOR 0x0000
#define START_DELAY 0
#define CHUNK_SIZE 256		//Sample upload chunk size in bytes
#define PD_HZ 225000		//Magic number used to convert amiga periods to Agon frequencies (original 187815)
#define TIMER_NO 5			//Timer block to use in ez80
#define MAX_CHANNELS 32  // Extended to support S3M's 32 channels

#define AMIGA_PERIOD_MAX 856		//Amiga period clamp maximum (finetune 0 = 856)
#define AMIGA_PERIOD_MIN 113		//Amiga period clamp minimum (finetune 0 = 113)

//MOD defines

#define EFFECT_ARPEGGIO		0x00
#define EFFECT_PORTA_UP		0x01
#define EFFECT_PORTA_DOWN	0x02
#define EFFECT_PORTA_NOTE	0x03
#define EFFECT_VIBRATO		0x04
#define EFFECT_VOL_TONE		0x05
#define EFFECT_VOL_VIBRATO	0x06
#define EFFECT_TREMULO		0x07
#define EFFECT_OFFSET		0x09
#define EFFECT_VOL_SLIDE	0x0A
#define EFFECT_ORDER_JUMP	0x0B
#define EFFECT_VOL_SET		0x0C
#define EFFECT_ROW_JUMP		0x0D
#define EFFECT_EXTENDED		0x0E
#define EFFECT_TEMP_SPEED	0x0F
#define EFFECT_NONE			0xFF

#define EXT_PORTA_UP		0x01
#define EXT_PORTA_DOWN		0x02
#define EXT_VIB_WAVE		0x04
#define EXT_FINETUNE		0x05
#define EXT_LOOP			0x06
#define EXT_TREM_WAVE		0x07
#define EXT_RETRIGGER		0x09
#define EXT_VOL_UP			0x0A
#define EXT_VOL_DOWN		0x0B
#define EXT_CUT_NOTE		0x0C
#define EXT_DELAY_NOTE		0x0D
#define EXT_REPEAT_NOTE		0x0E

//S3M defines

#define S3M_EFFECT_ARPEGGIO		0x00
#define S3M_EFFECT_SLIDE_UP		0x01
#define S3M_EFFECT_SLIDE_DOWN	0x02
#define S3M_EFFECT_TONE_PORTAMENTO	0x03
#define S3M_EFFECT_VIBRATO		0x04
#define S3M_EFFECT_TONE_PORTAMENTO_VOLUME_SLIDE	0x05
#define S3M_EFFECT_VIBRATO_VOLUME_SLIDE	0x06
#define S3M_EFFECT_TREMOLO		0x07
#define S3M_EFFECT_SET_PANNING	0x08
#define S3M_EFFECT_OFFSET		0x09
#define S3M_EFFECT_VOLUME_SLIDE	0x0A
#define S3M_EFFECT_POSITION_JUMP	0x0B
#define S3M_EFFECT_SET_VOLUME	0x0C
#define S3M_EFFECT_PATTERN_BREAK	0x0D
#define S3M_EFFECT_EXTENDED		0x0E
#define S3M_EFFECT_SET_SPEED	0x0F
#define S3M_EFFECT_SET_TEMPO	0x0F
#define S3M_EFFECT_GLOBAL_VOLUME	0x10
#define S3M_EFFECT_GLOBAL_VOLUME_SLIDE	0x11
#define S3M_EFFECT_KEY_OFF		0x14
#define S3M_EFFECT_SET_ENVELOPE_POSITION	0x15
#define S3M_EFFECT_PANNING_SLIDE	0x19
#define S3M_EFFECT_RETRIGGER	0x1B
#define S3M_EFFECT_FINE_VIBRATO	0x1C
#define S3M_EFFECT_FINE_SLIDE_UP	0x1D
#define S3M_EFFECT_FINE_SLIDE_DOWN	0x1E
#define S3M_EFFECT_SET_MODEL	0x1F
#define S3M_EFFECT_TREMOR		0x20
#define S3M_EFFECT_NONE			0xFF

//S3M extended effects
#define S3M_EXT_FILTER		0x00
#define S3M_EXT_FINEPORTA_UP	0x01
#define S3M_EXT_FINEPORTA_DOWN	0x02
#define S3M_EXT_GLISSANDO_CONTROL	0x03
#define S3M_EXT_VIBRATO_WAVE	0x04
#define S3M_EXT_FINETUNE		0x05
#define S3M_EXT_LOOP			0x06
#define S3M_EXT_TREMOLO_WAVE	0x07
#define S3M_EXT_RETRIGGER		0x09
#define S3M_EXT_FINE_VOLUME_UP	0x0A
#define S3M_EXT_FINE_VOLUME_DOWN	0x0B
#define S3M_EXT_CUT_NOTE		0x0C
#define S3M_EXT_DELAY_NOTE		0x0D
#define S3M_EXT_PATTERN_DELAY	0x0E
#define S3M_EXT_FUNK_REPEAT	0x0F

//ez80 defines

#define TMR0_CTL		0x80
#define TMR0_DR_L		0x81
#define TMR0_RR_L		0x81
#define TMR0_DR_H		0x82
#define TMR0_RR_H		0x82

#define PRT0_IVECT		0x0A

//File format types
typedef enum {
	FORMAT_MOD,
	FORMAT_S3M
} file_format_t;

volatile void *timer_prevhandler;
volatile uint24_t ticker = 0;
file_format_t current_format = FORMAT_MOD;
#ifndef HEADLESS
extern void timer_handler_0();
extern void timer_handler_1();
extern void timer_handler_2();
extern void timer_handler_3();
extern void timer_handler_4();
extern void timer_handler_5();

extern void uart0_fast_write(char *data, uint24_t length);
#endif



//Function prototypes
file_format_t detect_file_format(const char* filename);
void convert_16bit_to_8bit(uint8_t* dest, const int16_t* src, uint16_t length);

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

//S3M structures
typedef struct {
	char name[28];           // 0x00-0x1B: Song name
	uint8_t type;            // 0x1C: Type (0x10 = S3M)
	uint16_t reserved;       // 0x1D-0x1E: Reserved
	uint16_t num_orders;     // 0x1F-0x20: Number of orders
	uint16_t num_instruments; // 0x21-0x22: Number of instruments
	uint16_t num_patterns;   // 0x23-0x24: Number of patterns
	uint16_t flags;          // 0x25-0x26: Flags
	uint16_t tracker_version; // 0x27-0x28: Tracker version
	uint16_t file_version;   // 0x29-0x2A: File version
	uint8_t global_volume;   // 0x2B: Global volume
	char sig[4];             // 0x2C-0x2F: Signature "SCRM"
	uint8_t master_volume;   // 0x30: Master volume
	uint8_t ultra_click_removal; // 0x31: Ultra click removal
	uint8_t default_pan;     // 0x32: Default pan
	uint8_t reserved2[8];    // 0x33-0x3A: Reserved
	uint8_t special;         // 0x3B: Special
} s3m_file_header;

typedef struct {
	uint8_t type;
	uint8_t filename[12];
	uint8_t reserved;
	uint8_t volume;
	uint8_t c2spd;
	uint8_t reserved2[4];
	uint8_t pack;
	uint8_t flags;
	uint32_t c2spd_fine;
	uint8_t reserved3[12];
	uint8_t name[28];
	uint8_t magic[4];
} s3m_instrument_header;

typedef struct {
	uint8_t type;           // Sample type (0 = none, 1 = PCM, 2 = ADPCM)
	uint8_t reserved[3];    // Reserved
	uint32_t length;        // Sample length in words (16-bit)
	uint32_t loop_start;    // Loop start position
	uint32_t loop_end;      // Loop end position
	uint8_t volume;         // Default volume
	uint8_t reserved2[3];   // Reserved
	uint8_t pack;           // Packing type
	uint8_t flags;          // Flags
	uint32_t c2spd;         // C2 frequency
	uint8_t reserved3[12];  // Reserved
	char name[28];          // Sample name
	char magic[4];          // "SCRS" signature
} s3m_sample_header;

// S3M pattern structures
typedef struct {
	uint16_t length;           // Pattern length in words
	uint8_t data[];            // Pattern data (variable length)
} s3m_pattern_header;

// S3M note structure (4 bytes per note)
typedef struct {
	uint8_t note;              // Note number (0-96, 255 = no note)
	uint8_t instrument;        // Instrument number (1-255, 0 = no instrument)
	uint8_t volume;            // Volume (0-64, 255 = no volume)
	uint8_t effect;            // Effect command
	uint8_t effect_data;       // Effect data
} s3m_note;

// S3M pattern loading function prototypes
uint16_t* load_s3m_patterns(FILE* file, const s3m_file_header* header);
s3m_pattern_header* load_s3m_pattern(FILE* file, uint16_t pattern_offset);
void parse_s3m_pattern_row(const uint8_t* data, uint16_t* offset, s3m_note* notes, uint8_t num_channels);

// S3M pattern row structure
typedef struct {
	uint8_t channel_mask;      // Channel mask for this row
	uint8_t row_data[];        // Row data (variable length)
} s3m_pattern_row;

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
	uint8_t sample_volume[32];	
	uint8_t sample_channel[32];
	bool bad_samples;
	uint24_t pd_hz;
	uint8_t tick_no;
	
	uint8_t row_repeat;
	bool row_repeat_live;

	bool channel_disabled[MAX_CHANNELS];

} mod_header;

//Extended header structure to support both MOD and S3M
typedef struct {
	file_format_t format;
	union {
		mod_file_header mod_header;
		s3m_file_header s3m_header;
	} header_data;
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
	uint8_t sample_volume[32];	
	uint8_t sample_channel[32];
	bool bad_samples;
	uint24_t pd_hz;
	uint8_t tick_no;
	
	uint8_t row_repeat;
	bool	row_repeat_live;

	bool channel_disabled[MAX_CHANNELS];
	
	// S3M specific data
	uint8_t s3m_order_list[256];

} extended_header;

typedef struct {

	uint16_t latched_sample;
	//int16_t latched_volume;
	int16_t current_volume;
	uint8_t current_effect;
	uint8_t current_effect_param;
	uint16_t base_period;
	uint16_t tuned_period;
	uint8_t finetune;
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

	uint8_t loop_row;
	bool	loop_live;
	uint8_t loop_count;	

} channel_data;

uint8_t sine_table[] = {
	0, 24, 49, 74, 97,120,141,161,
	180,197,212,224,235,244,250,253,
	255,253,250,244,235,224,212,197,
	180,161,141,120, 97, 74, 49, 24};

#pragma pack(pop)

static extended_header mod;
//static channel_data *channels_data = NULL;
static channel_data channels_data[MAX_CHANNELS];
static FILE *file;
static uint8_t old_mode;
static int16_t global_volume = 100;
static uint24_t old_ticker = 0;
static uint24_t tick = 0;
static uint24_t mid_tick = 0;
static bool mod_ready = false;

// Parameters:
// - argc: Argument count
// - argv: Pointer to the argument string - zero terminated, parameters separated by spaces
//

static volatile SYSVAR *sv;
volatile uint8_t global_timer_ctl_no = 0x80; //default Timer 0

#ifndef HEADLESS
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
#else
unsigned char get_port(uint8_t port) { (void)port; return 0; /* Dummy implementation */ }
void set_port(uint8_t port, uint8_t value) { (void)port; (void)value; /* Dummy implementation */ }
#endif

void print_to_debug(const char* format, ...) {

	va_list args;
	putch(2);
	putch(21);
	
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

	putch(6);
	putch(3);

}

#ifndef HEADLESS
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
#endif

#ifndef HEADLESS
void timer_end(uint8_t timer_no) {
	
	set_port(TMR0_CTL + (timer_no * 3), 0x00);
	set_port(TMR0_RR_L + (timer_no * 3), 0x00);
	set_port(TMR0_RR_H + (timer_no * 3), 0x00);
	mos_setintvector(PRT0_IVECT + (timer_no * 2), timer_prevhandler);
	ticker = 0;

}
#endif

bool test_bit(int num, int pos) {
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

// static inline void clear_assets() {
//     char buffer[3] = {23, 27, 16};
//     mos_puts(buffer, 3, 0);
// }

void add_stream_to_buffer(uint16_t buffer_id, char* buffer_content, uint16_t buffer_size) {	

	putch(23);
	putch(0);
	putch(0xA0);
	write16bit(BUFFER_FLOOR + buffer_id);
	putch(0);
	write16bit(buffer_size);
	
    mos_puts(buffer_content, buffer_size, 0);

}

// static inline void add_stream_to_buffer(uint16_t buffer_id, char* buffer_content, uint16_t buffer_size) {
//     char buffer[8] = {23, 0, 0xA0, buffer_id & 0xFF, buffer_id >> 8, 0, buffer_size & 0xFF, buffer_size >> 8};
//     mos_puts(buffer, 8, 0);
//     mos_puts(buffer_content, buffer_size, 0);
// }

void sample_from_buffer(uint16_t buffer_id, uint8_t format) {

	putch(23);
	putch(0);
	putch(0x85);	
	putch(0);
	putch(5);
	putch(2);
	write16bit(BUFFER_FLOOR + buffer_id);
	putch(format);

}

// static inline void sample_from_buffer(uint16_t buffer_id, uint8_t format) {
//     char buffer[9] = {23, 0, 0x85, 0, 5, 2, buffer_id & 0xFF, buffer_id >> 8, format};
//     mos_puts(buffer, 9, 0);
// }

void tuneable_sample_from_buffer(uint16_t buffer_id, uint16_t frequency) {

	putch(23);
	putch(0);
	putch(0x85);	
	putch(0); //Ignored
	putch(5);
	putch(2);
	write16bit(BUFFER_FLOOR + buffer_id);
	putch(24);
	write16bit(frequency);

}

// static inline void tuneable_sample_from_buffer(uint16_t buffer_id, uint16_t frequency) {
//     char buffer[11] = {23, 0, 0x85, 0, 5, 2, buffer_id & 0xFF, buffer_id >> 8, 24, frequency & 0xFF, frequency >> 8};
//     mos_puts(buffer, 11, 0);
// }

void disable_channel(uint8_t channel) {

	//VDU 23, 0, &85, channel, 9
	putch(23);
	putch(0);
	putch(0x85);
	putch(channel);
	putch(9);

}

void enable_channel(uint8_t channel) {

	//VDU 23, 0, &85, channel, 8
	putch(23);
	putch(0);
	putch(0x85);
	putch(channel);
	putch(8);

}

// static inline void enable_channel(uint8_t channel) {
//     char buffer[5] = {23, 0, 0x85, channel, 8};
//     mos_puts(buffer, 5, 0);
// }

#ifndef HEADLESS
void assign_sample_to_channel(uint16_t sample_id, uint8_t channel_id) {
	
	putch(23);
	putch(0);
	putch(0x85);
	putch(channel_id);
	putch(4);
	putch(8);
	write16bit(BUFFER_FLOOR + sample_id);
	
}
#endif

// static inline void assign_sample_to_channel(uint16_t sample_id, uint8_t channel_id) {
//     char buffer[8] = {23, 0, 0x85, channel_id, 4, 8, sample_id & 0xFF, sample_id >> 8};
//     mos_puts(buffer, 8, 0);
// }

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

// static inline void play_sample(uint16_t sample_id, uint8_t channel, uint8_t volume, uint16_t duration, uint16_t frequency) {
//     assign_sample_to_channel(sample_id, channel);
//     char buffer[10] = {23, 0, 0x85, channel, 0, volume, frequency & 0xFF, frequency >> 8, duration & 0xFF, duration >> 8};
//     mos_puts(buffer, 10, 0);
// }


#ifndef HEADLESS
void set_volume(uint8_t channel, uint8_t volume) {

	//VDU 23, 0, &85, channel, 2, volume

	putch(23);
	putch(0);
	putch(0x85);
	putch(channel);
	putch(2);
	putch(volume);

}
#endif

// static inline void set_volume(uint8_t channel, uint8_t volume) {
//     char buffer[6] = {23, 0, 0x85, channel, 2, volume};
//     mos_puts(buffer, 6, 0);
// }

#ifndef HEADLESS
void set_frequency(uint8_t channel, uint16_t frequency) {

	//VDU 23, 0, &85, channel, 3, frequency;

	//Shouldn't be necessary given period clamp, but belt and braces.

	if (frequency > 3000) frequency = 3000;

	putch(23);
	putch(0);
	putch(0x85);
	putch(channel);
	putch(3);
	write16bit(frequency);

}
#endif

// static inline void set_frequency(uint8_t channel, uint16_t frequency) {
//     if (frequency > 2100) frequency = 2100;
//     else if (frequency < 50) frequency = 50;

//     char buffer[7] = {23, 0, 0x85, channel, 3, frequency & 0xFF, frequency >> 8};
//     mos_puts(buffer, 7, 0);
// }


#ifndef HEADLESS
void set_position(uint8_t channel, uint24_t position) {

	//VDU 23, 0, &85, channel, 11, position; positionHighByte

	putch(23);
	putch(0);
	putch(0x85);
	putch(channel);
	putch(11);
	write24bit(position);

}
#endif

// static inline void set_position(uint8_t channel, uint24_t position) {
//     char buffer[8] = {23, 0, 0x85, channel, 11, position & 0xFF, (position >> 8) & 0xFF, position >> 16};
//     mos_puts(buffer, 8, 0);
// }

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

// static inline void play_channel(uint8_t channel, uint8_t volume, uint24_t duration, uint16_t frequency) {
//     char buffer[11] = {23, 0, 0x85, channel, 0, volume, frequency & 0xFF, frequency >> 8, duration & 0xFF, (duration >> 8) & 0xFF, duration >> 16};
//     mos_puts(buffer, 11, 0);
// }

void vdu_move(uint16_t x, uint16_t y) {

	putch(25);
	putch(4);
	write16bit(x);
	write16bit(y);

}

// static inline void vdu_move(uint16_t x, uint16_t y) {
//     char buffer[6] = {25, 4, x & 0xFF, x >> 8, y & 0xFF, y >> 8};
//     mos_puts(buffer, 6, 0);
// }

void set_graphics_foreground(uint8_t colour) {

	putch(18);
	putch(0);
	putch(colour);

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

// static inline void draw_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
//     char buffer[12] = {25, 4, x1 & 0xFF, x1 >> 8, y1 & 0xFF, y1 >> 8, 25, 101, x2 & 0xFF, x2 >> 8, y2 & 0xFF, y2 >> 8};
//     mos_puts(buffer, 12, 0);
// }


void rect_drop_shadow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t gap_bottom, uint8_t gap_right, uint8_t fg, uint8_t bg) {

	set_graphics_foreground(bg);
	draw_rect(x1 + gap_right,y1 + gap_bottom,x2 + gap_right, y2 + gap_bottom);
	set_graphics_foreground(fg);
	draw_rect(x1, y1, x2, y2);

}

void draw_progress_bar(int total, int progress, int maxBarLength, int upperX, int upperY, int barHeight, uint8_t bg, uint8_t fg) {
    if (total <= 0) {
        return;
    }

    if (progress < 0 || progress > total) {
        return;
    }

    int segmentWidth = 10; // Fixed width of each segment
    int gap = 4; // Gap between segments

    // Calculate the maximum number of segments that can fit within the maxBarLength
    int maxSegments = (maxBarLength + gap) / (segmentWidth + gap);

    // Calculate the actual number of segments based on total
    int actualSegments = (total < maxSegments) ? total : maxSegments;

    // Recalculate the actual bar length considering the number of segments and gaps
    int actualBarLength = actualSegments * (segmentWidth + gap) - gap;

    // Calculate the number of filled segments based on progress
    int filledSegments = (progress * actualSegments) / total;

	set_graphics_foreground(bg);

	draw_rect(upperX - gap, upperY - (gap / 2), upperX + actualBarLength + gap, upperY + barHeight + (gap / 2));

	set_graphics_foreground(fg);

    for (int i = 0; i < filledSegments; ++i) {
        int x1 = upperX + i * (segmentWidth + gap); // Starting x-coordinate of each segment
        int y1 = upperY; // Top y-coordinate (upperY)
        int x2 = x1 + segmentWidth; // Ending x-coordinate of each segment
        int y2 = upperY + barHeight; // Bottom y-coordinate

        draw_rect(x1, y1, x2, y2);
    }
}

void plot_point(uint16_t x1, uint16_t y1) {

	//PLOT 69,x,y

	putch(25);
	putch(69);
	write16bit(x1);
	write16bit(y1);

}

// static inline void plot_point(uint16_t x1, uint16_t y1) {
//     char buffer[6] = {25, 69, x1 & 0xFF, x1 >> 8, y1 & 0xFF, y1 >> 8};
//     mos_puts(buffer, 6, 0);
// }

void clear_buffer(uint16_t buffer_id) {
	
	putch(23);
	putch(0);
	putch(0xA0);
	write16bit(BUFFER_FLOOR + buffer_id);
	putch(2);
	
}

// static inline void clear_buffer(uint16_t buffer_id) {
//     char buffer[6] = {23, 0, 0xA0, buffer_id & 0xFF, buffer_id >> 8, 2};
//     mos_puts(buffer, 6, 0);
// }


void set_sample_frequency(uint16_t buffer_id, uint16_t frequency) {
	
	putch(23);
	putch(0);
	putch(0x85);
	putch(0); //Ignored channel
	putch(5);
	putch(4);
	write16bit(BUFFER_FLOOR + buffer_id);
	write16bit(frequency);
	
}

// static inline void set_sample_frequency(uint16_t buffer_id, uint16_t frequency) {
//     char buffer[10] = {23, 0, 0x85, 0, 5, 4, buffer_id & 0xFF, buffer_id >> 8, frequency & 0xFF, frequency >> 8};
//     mos_puts(buffer, 10, 0);
// }

void set_channel_rate(uint8_t channel, uint16_t sample_rate) {
	
	putch(23);
	putch(0);
	putch(0x85);
	putch(channel);
	putch(13);
	write16bit(sample_rate);
	
}

// static inline void set_channel_rate(uint8_t channel, uint16_t sample_rate) {
//     char buffer[7] = {23, 0, 0x85, channel, 13, sample_rate & 0xFF, sample_rate >> 8};
//     mos_puts(buffer, 7, 0);
// }

void reset_channel(uint8_t channel) {
	
	putch(23);
	putch(0);
	putch(0x85);
	putch(channel);
	putch(10);
	
}

// static inline void reset_channel(uint8_t channel) {
//     char buffer[5] = {23, 0, 0x85, channel, 10};
//     mos_puts(buffer, 5, 0);
// }

void set_sample_duration(uint8_t channel, uint24_t duration) {
	
	putch(23);
	putch(0);
	putch(0x85);
	putch(channel);
	putch(12);
	write24bit(duration);
	
}

// static inline void set_sample_duration_and_play(uint8_t channel, uint24_t duration) {
//     char buffer[8] = {23, 0, 0x85, channel, 12, duration & 0xFF, (duration >> 8) & 0xFF, duration >> 16};
//     mos_puts(buffer, 8, 0);
// }

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
	write16bit(BUFFER_FLOOR + sample_id);
	write24bit(start);

}

// static inline void set_sample_loop_start(uint16_t sample_id, uint24_t start) {
//     char buffer[11] = {23, 0, 0x85, 0, 5, 6, sample_id & 0xFF, sample_id >> 8, start & 0xFF, (start >> 8) & 0xFF, start >> 16};
//     mos_puts(buffer, 11, 0);
// }


void switch_buffers() {

	//VDU 23, 0, &C3

	putch(23);
	putch(0);
	putch(0xC3);

}

// static inline void switch_buffers() {
//     char buffer[3] = {23, 0, 0xC3};
//     mos_puts(buffer, 3, 0);
// }


void set_sample_loop_length(uint16_t sample_id, uint24_t length) {

	//VDU 23, 0, &85, channel, 5, 8, bufferId; repeatLength; repeatLengthHighByte

	putch(23);
	putch(0);
	putch(0x85);
	putch(0);
	putch(5);
	putch(8);
	write16bit(BUFFER_FLOOR + sample_id);
	write24bit(length);

}

// static inline void set_sample_loop_length(uint16_t sample_id, uint24_t length) {
//     char buffer[11] = {23, 0, 0x85, 0, 5, 8, sample_id & 0xFF, sample_id >> 8, length & 0xFF, (length >> 8) & 0xFF, length >> 16};
//     mos_puts(buffer, 11, 0);
// }

void set_text_window(uint8_t left, uint8_t bottom, uint8_t right, uint8_t top) {

	putch(0x1C);

	putch(left);
	putch(bottom);
	putch(right);
	putch(top);

}

// static inline void set_text_window(uint8_t left, uint8_t bottom, uint8_t right, uint8_t top) {
//     char buffer[5] = {0x1C, left, bottom, right, top};
//     mos_puts(buffer, 5, 0);
// }

void set_graphics_window(uint16_t left, uint16_t bottom, uint16_t right, uint16_t top) {

	putch(24);

	write16bit(left);
	write16bit(bottom);
	write16bit(right);
	write16bit(top);

}

// static inline void set_graphics_window(uint16_t left, uint16_t bottom, uint16_t right, uint16_t top) {
//     char buffer[9] = {24, left & 0xFF, left >> 8, bottom & 0xFF, bottom >> 8, right & 0xFF, right >> 8, top & 0xFF, top >> 8};
//     mos_puts(buffer, 9, 0);
// }

void cursor_tab(uint8_t x, uint8_t y) {

	putch(0x1F);
	putch(x);
	putch(y);

}

void cursor_set(bool state) {

	//VDU 23, 1, n

	putch(23);
	putch(1);
	putch(state);

}

// static inline void cursor_tab(uint8_t x, uint8_t y) {
//     char buffer[3] = {0x1F, x, y};
//     mos_puts(buffer, 3, 0);
// }

uint8_t index_period(uint16_t period) {
    switch (period) {
        case 113: return 35;
        case 120: return 34;
        case 127: return 33;
        case 135: return 32;
        case 143: return 31;
        case 151: return 30;
        case 160: return 29;
        case 170: return 28;
        case 180: return 27;
        case 190: return 26;
        case 202: return 25;
        case 214: return 24;
        case 226: return 23;
        case 240: return 22;
        case 254: return 21;
        case 269: return 20;
        case 285: return 19;
        case 302: return 18;
        case 320: return 17;
        case 339: return 16;
        case 360: return 15;
        case 381: return 14;
        case 404: return 13;
        case 428: return 12;
        case 453: return 11;
        case 480: return 10;
        case 508: return 9;
        case 538: return 8;
        case 570: return 7;
        case 604: return 6;
        case 640: return 5;
        case 678: return 4;
        case 720: return 3;
        case 762: return 2;
        case 808: return 1;
        case 856: return 0;
        default: {
			#ifdef PRINT_DEBUG
			print_to_debug("\r\nNon-standard period %u.\r\n", period);
			#endif
			return 24; //C-3, to avoid horrible screeching as much as possible.
			}

    }
}

const uint16_t tunings[][36] = {
	{856,808,762,720,678,640,604,570,538,508,480,453,428,404,381,360,339,320,302,285,269,254,240,226,214,202,190,180,170,160,151,143,135,127,120,113}, //0
	{850,802,757,715,674,637,601,567,535,505,477,450,425,401,379,357,337,318,300,284,268,253,239,225,213,201,189,179,169,159,150,142,134,126,119,113}, //1
	{844,796,752,709,670,632,597,563,532,502,474,447,422,398,376,355,335,316,298,282,266,251,237,224,211,199,188,177,167,158,149,141,133,125,118,112}, //2
	{838,791,746,704,665,628,592,559,528,498,470,444,419,395,373,352,332,314,296,280,264,249,235,222,209,198,187,176,166,157,148,140,132,125,118,111}, //3
	{832,785,741,699,660,623,588,555,524,495,467,441,416,392,370,350,330,312,294,278,262,247,233,220,208,196,185,175,165,156,147,139,131,124,117,110}, //4
	{826,779,736,694,655,619,584,551,520,491,463,437,413,390,368,347,328,309,292,276,260,245,232,219,206,195,184,174,164,155,146,138,130,123,116,109}, //5
	{820,774,730,689,651,614,580,547,516,487,460,434,410,387,365,345,325,307,290,274,258,244,230,217,205,193,183,172,163,154,145,137,129,122,115,109}, //6
	{814,768,725,684,646,610,575,543,513,484,457,431,407,384,363,342,323,305,288,272,256,242,228,216,204,192,181,171,161,152,144,136,128,121,114,108}, //7
	{907,856,808,762,720,678,640,604,570,538,508,480,453,428,404,381,360,339,320,302,285,269,254,240,226,214,202,190,180,170,160,151,143,135,127,120}, //8 -8
	{900,850,802,757,715,675,636,601,567,535,505,477,450,425,401,379,357,337,318,300,284,268,253,238,225,212,200,189,179,169,159,150,142,134,126,119}, //9 -7
	{894,844,796,752,709,670,632,597,563,532,502,474,447,422,398,376,355,335,316,298,282,266,251,237,223,211,199,188,177,167,158,149,141,133,125,118}, //A -6
	{887,838,791,746,704,665,628,592,559,528,498,470,444,419,395,373,352,332,314,296,280,264,249,235,222,209,198,187,176,166,157,148,140,132,125,118}, //B -5
	{881,832,785,741,699,660,623,588,555,524,494,467,441,416,392,370,350,330,312,294,278,262,247,233,220,208,196,185,175,165,156,147,139,131,123,117}, //C -4
	{875,826,779,736,694,655,619,584,551,520,491,463,437,413,390,368,347,328,309,292,276,260,245,232,219,206,195,184,174,164,155,146,138,130,123,116}, //D -3
	{868,820,774,730,689,651,614,580,547,516,487,460,434,410,387,365,345,325,307,290,274,258,244,230,217,205,193,183,172,163,154,145,137,129,122,115}, //E -2
	{862,814,768,725,684,646,610,575,543,513,484,457,431,407,384,363,342,323,305,288,272,256,242,228,216,203,192,181,171,161,152,144,136,128,121,114}, //F -1
};

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

	for (uint8_t i = 0; i < 8; i++) reset_channel(i);

	timer_end(TIMER_NO);

	//if (channels_data != NULL) free(channels_data);

	if (file != NULL) fclose(file);

	#ifdef VARIABLE_RATE
	set_channel_rate(-1, 16384);
	#endif

	#if defined(VIZ) || defined(VERBOSE)

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

// static inline uint16_t max(uint16_t a, uint16_t b) {
//     return (a > b) ? a : b;
// }

// static inline uint16_t min(uint16_t a, uint16_t b) {
//     return (a < b) ? a : b;
// }

uint16_t clamp_period(uint16_t period) {
    if (period <= AMIGA_PERIOD_MIN) return AMIGA_PERIOD_MIN;
    else if (period >= AMIGA_PERIOD_MAX) return AMIGA_PERIOD_MAX;
    else return period;
}

uint8_t clamp_volume(int8_t volume) {
    if (volume <= 0) return 0;
    else if (volume >= 64) return 64;
    else return volume;
}

uint8_t scale_volume(int original_volume) {
    int scaled_volume = (original_volume * 127) + 63;
    return scaled_volume / 64;
}

void fill_empty(uint8_t rows) {
			
	putch(0x1E); //Return to top left

	for (uint8_t i = 0; i < rows; i++) {
		if (mod.channels == 4) {
			uint8_t c = 9;
			putch(17);
			putch(7); //White
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

#ifndef HEADLESS
void dispatch_channel(uint8_t i) {
		
		if ((channels_data[i].current_effect == EFFECT_EXTENDED) && ((channels_data[i].current_effect_param >> 4) == EXT_DELAY_NOTE) && mod.tick_no != (channels_data[i].current_effect_param & 0x0F)) {
			//set_volume(i, 0);
			//printf("\r\nOn tick %u, delaying until tick %u\r\n", mod.tick_no, channels_data[i].current_effect_param & 0x0F);
			return;
		}
		
		channels_data[i].current_hz = channels_data[i].tuned_period > 0 ? mod.pd_hz / channels_data[i].tuned_period : 0;

		if (swap_word(mod.header_data.mod_header.sample[channels_data[i].latched_sample - 1].LOOP_LENGTH) > 1) play_sample(channels_data[i].latched_sample, i, scale_volume(channels_data[i].current_volume), -1, channels_data[i].current_hz);
		else play_sample(channels_data[i].latched_sample, i, scale_volume(channels_data[i].current_volume), 0, channels_data[i].current_hz);
		if (channels_data[i].current_effect == EFFECT_OFFSET) set_position(i, channels_data[i].latched_offset);
		mod.sample_volume[channels_data[i].latched_sample] = channels_data[i].current_volume;

}
#endif

void volume_slide(uint8_t i) {

	if (channels_data[i].current_effect == EFFECT_VOL_TONE || channels_data[i].current_effect == EFFECT_VOL_VIBRATO || channels_data[i].current_effect == EFFECT_VOL_SLIDE) {
	
		uint8_t slide_x = channels_data[i].current_effect_param >> 4;
		uint8_t slide_y = channels_data[i].current_effect_param & 0x0F;

		if (slide_x) {

			uint8_t slide_adjusted = (slide_x);
			channels_data[i].current_volume = clamp_volume(channels_data[i].current_volume + slide_adjusted);
			set_volume(i, scale_volume(channels_data[i].current_volume));
			mod.sample_volume[channels_data[i].latched_sample] = channels_data[i].current_volume;

		} else if (slide_y) {

			uint8_t slide_adjusted = (slide_y);
			channels_data[i].current_volume = clamp_volume(channels_data[i].current_volume - slide_adjusted);
			set_volume(i, scale_volume(channels_data[i].current_volume));
			mod.sample_volume[channels_data[i].latched_sample] = channels_data[i].current_volume;
		}

	} else {

		if ((channels_data[i].current_effect_param >> 4) == EXT_VOL_UP) {

			uint8_t slide_adjusted = ((channels_data[i].current_effect_param & 0x0F));
			channels_data[i].current_volume = clamp_volume(channels_data[i].current_volume + slide_adjusted);
			set_volume(i, scale_volume(channels_data[i].current_volume));
			mod.sample_volume[channels_data[i].latched_sample] = channels_data[i].current_volume;	

		} else if ((channels_data[i].current_effect_param >> 4) == EXT_VOL_DOWN) {

			uint8_t slide_adjusted = ((channels_data[i].current_effect_param & 0x0F));
			channels_data[i].current_volume = clamp_volume(channels_data[i].current_volume - slide_adjusted);
			set_volume(i, scale_volume(channels_data[i].current_volume));
			mod.sample_volume[channels_data[i].latched_sample] = channels_data[i].current_volume;

		}

	}

}

void pitch_slide(uint8_t i) {

	if (channels_data[i].current_effect == EFFECT_PORTA_NOTE || channels_data[i].current_effect == EFFECT_VOL_TONE) {

		if (channels_data[i].target_period) {

			if (channels_data[i].target_period > channels_data[i].tuned_period) {

				if (channels_data[i].tuned_period + channels_data[i].slide_rate >= channels_data[i].target_period) {
					channels_data[i].tuned_period = channels_data[i].target_period;
					channels_data[i].current_hz = mod.pd_hz / channels_data[i].tuned_period;
					set_frequency(i, channels_data[i].current_hz);
					channels_data[i].target_period = 0;
				} else {
					channels_data[i].tuned_period = clamp_period(channels_data[i].tuned_period + channels_data[i].slide_rate);
					channels_data[i].current_hz = mod.pd_hz / channels_data[i].tuned_period;
					set_frequency(i, channels_data[i].current_hz);
				}

			} else if (channels_data[i].target_period < channels_data[i].tuned_period) {

				if (channels_data[i].tuned_period - channels_data[i].slide_rate <= channels_data[i].target_period) {
					channels_data[i].tuned_period = channels_data[i].target_period;
					channels_data[i].current_hz = mod.pd_hz / channels_data[i].tuned_period;
					set_frequency(i, channels_data[i].current_hz);
					channels_data[i].target_period = 0;
				} else {
					channels_data[i].tuned_period = clamp_period(channels_data[i].tuned_period - channels_data[i].slide_rate);
					channels_data[i].current_hz = mod.pd_hz / channels_data[i].tuned_period;
					set_frequency(i, channels_data[i].current_hz);
				}

			} 
			
			if (channels_data[i].target_period == channels_data[i].tuned_period) {

				channels_data[i].target_period = 0;
				channels_data[i].current_hz = mod.pd_hz / channels_data[i].tuned_period;
				set_frequency(i, channels_data[i].current_hz);				

			}

		}

	} else {

		int16_t magnitude = 0;

		if (channels_data[i].current_effect == EFFECT_PORTA_UP) magnitude = -1 * channels_data[i].current_effect_param;
		else if ((channels_data[i].current_effect == EFFECT_EXTENDED) && channels_data[i].current_effect_param >> 4 == EXT_PORTA_UP) magnitude = -1 * (channels_data[i].current_effect_param & 0x0F);

		else if (channels_data[i].current_effect == EFFECT_PORTA_DOWN) magnitude = channels_data[i].current_effect_param;
		else if ((channels_data[i].current_effect == EFFECT_EXTENDED) && channels_data[i].current_effect_param >> 4 == EXT_PORTA_DOWN) magnitude = channels_data[i].current_effect_param & 0x0F;	

		channels_data[i].tuned_period = clamp_period(channels_data[i].tuned_period + magnitude);
		channels_data[i].current_hz = mod.pd_hz / channels_data[i].tuned_period;
		set_frequency(i, channels_data[i].current_hz);

	}

}

void do_vibrato(uint8_t i) {

	uint16_t delta = sine_table[channels_data[i].vibrato_position & 31];
	delta *= channels_data[i].vibrato_depth;
	delta >>= 7; //Divide by 128

	if (channels_data[i].vibrato_position < 0) {
		channels_data[i].current_hz = mod.pd_hz / clamp_period(channels_data[i].tuned_period + delta);
		set_frequency(i, channels_data[i].current_hz);
	}
	else if (channels_data[i].vibrato_position >= 0) {
		channels_data[i].current_hz = mod.pd_hz / clamp_period(channels_data[i].tuned_period + delta);
		set_frequency(i, channels_data[i].current_hz);
	}

	//printf("\r\nVibrato on %u with speed %u and depth %u, sine pos %i meaning delta %u.", i, channels_data[i].vibrato_speed, channels_data[i].vibrato_depth, channels_data[i].vibrato_position, delta);

	channels_data[i].vibrato_position += channels_data[i].vibrato_speed;
	if (channels_data[i].vibrato_position > 31) channels_data[i].vibrato_position -= 64;

}

void do_tremulo(uint8_t i) {

	uint16_t delta = sine_table[channels_data[i].tremolo_position & 31];
	delta *= channels_data[i].tremolo_depth;
	delta >>= 6; //Divide by 64

	if (channels_data[i].tremolo_position < 0) {
		set_volume(i, scale_volume(clamp_volume(channels_data[i].current_volume - (delta))));
		mod.sample_volume[channels_data[i].latched_sample] = clamp_volume(channels_data[i].current_volume - (delta));
	}
	else if (channels_data[i].tremolo_position >= 0) {
		set_volume(i, scale_volume(clamp_volume(channels_data[i].current_volume + (delta))));
		mod.sample_volume[channels_data[i].latched_sample] = clamp_volume(channels_data[i].current_volume + (delta));
	}

	//printf("\r\nTremolo on %u with speed %u and depth %u, sine pos %i meaning delta %u.", i, channels_data[i].tremolo_speed, channels_data[i].tremolo_depth, channels_data[i].tremolo_position, delta);

	channels_data[i].tremolo_position += channels_data[i].tremolo_speed;
	if (channels_data[i].tremolo_position > 31) channels_data[i].tremolo_position -= 64;

}

void handle_breaks() {

	if (mod.order_break_pending && mod.order_break_pending) { //Combined order and pattern breaks (0x0B and 0x0D on the same row)

		mod.current_order = mod.new_order;
		mod.current_row = mod.new_row;
		mod.order_break_pending = false;
		mod.pattern_break_pending = false;
		
	} else if (mod.order_break_pending) { //Just an order break (0x0B)

		mod.current_order = mod.new_order;
		mod.current_row = 0;
		mod.order_break_pending = false;

	} else if (mod.pattern_break_pending) { //Just a pattern break (0x0D)

		if (mod.current_order > mod.header_data.mod_header.num_orders - 1) mod.current_order = 0; //Handle 0x0D at end of a song.
		mod.current_row = mod.new_row;
		if (mod.current_row < 63) mod.current_order++;
		mod.pattern_break_pending = false;

	}

}

void process_note(size_t pattern_no, size_t row)  {
	
	if (mod.format == FORMAT_S3M) {
		// S3M format - load pattern data from pattern buffer and parse the row
		
		// Reset tick counter
		mod.tick_no = 0;
		
		// Get the pattern number from the order list
		uint8_t pattern_num = mod.s3m_order_list[mod.current_order];
		if (pattern_num >= swap_word(mod.header_data.s3m_header.num_patterns)) {
			// Invalid pattern, skip to next order
			return;
		}
		
		// Get the pattern data from the pre-loaded patterns
		s3m_pattern_header** patterns = (s3m_pattern_header**)mod.pattern_buffer;
		s3m_pattern_header* pattern = patterns[pattern_num];
		if (!pattern) {
			return; // Failed to load pattern (should not happen if pre-loaded correctly)
		}
		
		// Parse the row data
		uint16_t data_offset = 0;
		s3m_note notes[MAX_CHANNELS];
		uint8_t num_channels = 32; // S3M supports up to 32 channels
		
		// Calculate the offset for the current row within the pattern data
		// This part needs to be refined to correctly seek to the start of the row data
		// For now, parse_s3m_pattern_row will iterate through the pattern data
		// and we need to ensure it starts at the correct row.
		// The current parse_s3m_pattern_row function parses from the beginning of the pattern.
		// We need to advance the data_offset to the correct row.
		// This is a placeholder for now, assuming parse_s3m_pattern_row handles row iteration.
		// A better approach would be to pass the row number to parse_s3m_pattern_row
		// or pre-calculate the offset to the start of the row.
		
		// For now, let's assume parse_s3m_pattern_row can handle finding the correct row
		// or that we only process the first row of the pattern for simplicity.
		// This needs to be fixed to correctly parse the 'row' parameter.
		
		// To correctly parse a specific row, we need to iterate through the pattern data
		// until we reach the desired row.
		uint16_t current_data_offset = 0;
		for (size_t r = 0; r < row; r++) {
			uint8_t byte;
			do {
				byte = pattern->data[current_data_offset++];
				if (byte & 0x20) current_data_offset++; // Note
				if (byte & 0x40) current_data_offset++; // Instrument
				if (byte & 0x80) current_data_offset++; // Volume
				if (byte & 0x0F) current_data_offset += 2; // Effect
			} while (byte != 0 && current_data_offset < pattern->length);
			if (byte == 0) { // Reached end of pattern before desired row
				// This means the pattern is shorter than expected, or row is out of bounds
				// For now, just break and process what we have (likely empty notes)
				break;
			}
		}
		data_offset = current_data_offset;
		
		parse_s3m_pattern_row(pattern->data, &data_offset, notes, num_channels);
		
		// Process notes for each channel
		for (uint8_t i = 0; i < num_channels; i++) {
			if (mod.channel_disabled[i]) {
				continue;
			}
			
			s3m_note* note = &notes[i];
			
			// Handle note (0-96, 255 = no note)
			if (note->note != 255 && note->note <= 96) {
				// Convert S3M note to frequency
				// S3M uses note 0-96 where 48 = C-4 (middle C)
				// We need to convert this to our tuning system
				uint8_t note_index = note->note + 12; // Adjust for our tuning array
				if (note_index < 36) {
					uint16_t period = tunings[0][note_index]; // Use tuning 0 for now
					channels_data[i].base_period = period;
					channels_data[i].tuned_period = period;
					set_frequency(i, PD_HZ / period); // Set frequency
				}
			}
			
			// Handle instrument (1-255, 0 = no instrument)
			if (note->instrument > 0 && note->instrument <= swap_word(mod.header_data.s3m_header.num_instruments)) {
				uint8_t sample_id = note->instrument;
				if (mod.sample_live[sample_id]) {
					channels_data[i].latched_sample = sample_id;
					mod.sample_channel[sample_id] = i;
					
					// Set volume from sample header
					uint8_t volume = mod.sample_volume[sample_id];
					channels_data[i].current_volume = clamp_volume(volume);
					set_volume(i, scale_volume(channels_data[i].current_volume));
					
					// Assign sample to channel
					assign_sample_to_channel(sample_id, i);
				}
			}
			
			// Handle volume (0-64, 255 = no volume)
			if (note->volume != 255) {
				channels_data[i].current_volume = clamp_volume(note->volume);
				set_volume(i, scale_volume(channels_data[i].current_volume));
			}
			
			// Handle effects
			if (note->effect != 0) {
				channels_data[i].current_effect = note->effect;
				channels_data[i].current_effect_param = note->effect_data;
				
				// Process immediate effects
				uint8_t param_x = note->effect_data >> 4;
				uint8_t param_y = note->effect_data & 0x0F;
				
				switch (note->effect) {
					case S3M_EFFECT_SET_VOLUME:
						channels_data[i].current_volume = clamp_volume(note->effect_data);
						set_volume(i, scale_volume(channels_data[i].current_volume));
						break;
						
					case S3M_EFFECT_POSITION_JUMP:
						if (!mod.order_break_pending) {
							mod.new_order = note->effect_data;
							mod.order_break_pending = true;
							if (mod.pattern_break_pending) {
								mod.pattern_break_pending = false;
								mod.new_row = 0;
							}
						}
						break;
						
					case S3M_EFFECT_PATTERN_BREAK:
						if (!mod.pattern_break_pending) {
							mod.new_row = (param_x * 10) + param_y;
							mod.pattern_break_pending = true;
						}
						break;
						
					case S3M_EFFECT_SET_SPEED: // Also handles S3M_EFFECT_SET_TEMPO
						if (note->effect_data < 0x20) {
							mod.current_speed = note->effect_data;
						} else {
							// Effect data >= 0x20 means SET_TEMPO
							mod.current_bpm = note->effect_data;
							timer_end(TIMER_NO);
							timer_begin(TIMER_NO, (rr_array[mod.current_bpm - 0x20]), div_array[mod.current_bpm - 0x20]);
						}
						break;
						
					case S3M_EFFECT_EXTENDED:
						// Handle extended S3M effects
						switch (param_x) {
							case S3M_EXT_FINETUNE:
								channels_data[i].finetune = param_y;
								break;
							case S3M_EXT_RETRIGGER:
								// Retrigger the note
								if (channels_data[i].latched_sample > 0) {
									dispatch_channel(i);
								}
								break;
						}
						break;
				}
			}
		}
	} else {
		// MOD format - existing code
		size_t offset = (mod.channels * 4 * 64) * pattern_no + (row * 4 * mod.channels);

		fseek(file, 1084 + offset, SEEK_SET);

		//uint8_t *noteData = buffer + offset;

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
												printf("%02zu ", row);
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

			if (mod.channel_disabled[i]) {
				
				#ifdef VERBOSE

					putch(17);
					uint8_t new_colour = 9+i;
					if (new_colour == 16) new_colour++;
					putch(new_colour);			

					if (mod.channels == 4) {
					
						   //## FFF SS VV EEE FFF SS VV EEE FFF SS VV EEE FFF SS VV EEE
						printf("### ## ## ###");
						if (i != mod.channels - 1) printf(" ");

					} else if (mod.channels == 6) {

						   //## FFF SS FFF SS FFF SS FFF SS FFF SS FFF SS
						printf("### ##");
						if (i != mod.channels - 1) printf(" ");			


					} else if (mod.channels == 8) {
						
						   //## FFF SS FFF SS FFF SS FFF SS FFF SS FFF SS FFF SS FFF SS
						printf("### ##");
						if (i != mod.channels - 1) printf(" ");
						
					}

				#endif

				//Move on to next channel/note in memory

				//noteData += 4;
				fseek(file, 4, SEEK_CUR);

				continue;
			}

			#ifdef VERBOSE

				putch(17);
				uint8_t new_colour = 9+i;
				if (new_colour == 16) new_colour++;
				putch(new_colour);
				
			#endif

			uint8_t noteData[4];
			fread(&noteData, sizeof(uint8_t), 4, file);

			sample_number = (noteData[0] & 0xF0) + (noteData[2] >> 4);
			effect_number = (noteData[2] & 0xF);
			effect_param = noteData[3];
			period = ((uint16_t)(noteData[0] & 0xF) << 8) | (uint16_t)noteData[1];

			if (effect_number == EFFECT_PORTA_NOTE || effect_number == EFFECT_VOL_TONE) {
				if (period && effect_number == EFFECT_PORTA_NOTE) channels_data[i].target_period = tunings[channels_data[i].finetune][index_period(period)];		//Log the note as effect 3/5's target, but don't use it now.
				if (effect_param > 0 && effect_number == EFFECT_PORTA_NOTE) channels_data[i].slide_rate = effect_param;	//If effect 3 has a parameter, use it as slide rate.
			} else if (period > 0) {
				//channels_data[i].tuned_period = period;
				channels_data[i].base_period = period;
				channels_data[i].tuned_period = tunings[channels_data[i].finetune][index_period(period)];
			}

			if (effect_param || effect_number) {
				channels_data[i].current_effect = effect_number;
				channels_data[i].current_effect_param = effect_param;
			} else channels_data[i].current_effect = EFFECT_NONE;

			// Output the decoded note information
			// Ref: void play_sample(uint16_t sample_id, uint8_t channel, uint8_t volume, uint16_t duration, uint16_t frequency)
			
			if (sample_number > 0) {
				
				if (channels_data[i].latched_sample != sample_number) {
					mod.sample_volume[channels_data[i].latched_sample] = 0;
					mod.sample_channel[channels_data[i].latched_sample] = i;
				}
				channels_data[i].latched_sample = sample_number;
				channels_data[i].finetune = mod.header_data.mod_header.sample[channels_data[i].latched_sample - 1].FINE_TUNE;
				mod.sample_channel[channels_data[i].latched_sample] = i;
						//channels_data[i].latched_volume = clamp_volume((mod.header_data.mod_header.sample[channels_data[i].latched_sample - 1].VOLUME * 2) - 1);	
			channels_data[i].current_volume = clamp_volume((mod.header_data.mod_header.sample[channels_data[i].latched_sample - 1].VOLUME));
				set_volume(i, scale_volume(channels_data[i].current_volume));

				if (period > 0 && (effect_number != EFFECT_PORTA_NOTE) && (effect_number != EFFECT_VOL_TONE)) {

					if (effect_number == 0x09) channels_data[i].latched_offset = effect_param << 8;
					
					dispatch_channel(i);

				} else if (period > 0 && ((effect_number == EFFECT_PORTA_NOTE) || (effect_number == EFFECT_VOL_TONE)) && swap_word(mod.header_data.mod_header.sample[channels_data[i].latched_sample - 1].LOOP_LENGTH) > 1) {

					channels_data[i].latched_offset = swap_word(mod.header_data.mod_header.sample[channels_data[i].latched_sample - 1].LOOP_LENGTH) * 2;
					dispatch_channel(i);

				}

			} else if ((period > 0) && (effect_number != EFFECT_PORTA_NOTE) && (effect_number != EFFECT_VOL_TONE)) {

				if (channels_data[i].latched_sample > 0) {

					if (effect_number == 0x09) channels_data[i].latched_offset = effect_param << 8;
					
					dispatch_channel(i);

				}

			}

			//Process effects that should happen immediately

			if (channels_data[i].current_effect != 0xFF) {

				uint8_t param_x = channels_data[i].current_effect_param >> 4;
				uint8_t param_y = channels_data[i].current_effect_param & 0x0F;

				switch (channels_data[i].current_effect) {

					case EFFECT_VIBRATO: {//Vibrato
						
						if (channels_data[i].vibrato_retrigger == true) channels_data[i].vibrato_position = 0;
						if (param_x > 0) channels_data[i].vibrato_speed = param_x;
						if (param_y > 0) channels_data[i].vibrato_depth = param_y;

					} break;	

					case EFFECT_TREMULO: {//Tremolo
						
						if (channels_data[i].tremolo_retrigger == true) channels_data[i].tremolo_position = 0;
						if (param_x) channels_data[i].tremolo_speed = param_x;
						if (param_y) channels_data[i].tremolo_depth = param_y;

					} break;					

					case EFFECT_ORDER_JUMP: {//Skip to order xx
				
						if (mod.order_break_pending == false) {
							mod.new_order = channels_data[i].current_effect_param;
							mod.order_break_pending = true;
							if (mod.pattern_break_pending) {
								mod.pattern_break_pending = false;
								mod.new_row = 0;
							}
						}

					} break;				

					case EFFECT_VOL_SET: {//Set channel volume to xx

						channels_data[i].current_volume = clamp_volume((channels_data[i].current_effect_param));
						set_volume(i, scale_volume(channels_data[i].current_volume));
						mod.sample_volume[channels_data[i].latched_sample] = channels_data[i].current_volume;

					} break;				

					case EFFECT_ROW_JUMP: {//Pattern break - Skip to next pattern, row xx

						if (mod.pattern_break_pending == false) {
							mod.new_row = (param_x * 10) + param_y;
							mod.pattern_break_pending = true;
						}

					} break;


					case EFFECT_EXTENDED: {//Extended functions
						
						switch (param_x) {

							case EXT_PORTA_UP: {

								pitch_slide(i);							
							} break;

							case EXT_PORTA_DOWN: {

								pitch_slide(i);

							} break;
							
							case EXT_VIB_WAVE: {//Set waveform (vibrato)

								if (param_y == 0) channels_data[i].vibrato_retrigger = true;
								else if (param_y == 4) channels_data[i].vibrato_retrigger = false;

							} break;		

							case EXT_LOOP: {//Set or begin loop at row X

								if (param_y == 0) {
									channels_data[i].loop_row = mod.current_row - 1;
									//printf("\r\nLoop set at row %u\r\n", channels_data[i].loop_row);
									//printf("\r\nLogged row %u as the start of a future loop\r\n", mod.loop_row);
								}

								else {

									if (!channels_data[i].loop_live) {

										channels_data[i].loop_count = param_y;
										channels_data[i].loop_live = true;
										mod.current_row = channels_data[i].loop_row;
										//printf("\r\nLoop now starting, returning to row %u %u times\r\n", channels_data[i].loop_row, channels_data[i].loop_count);

									} else if (channels_data[i].loop_live) {

										channels_data[i].loop_count--;
										//printf("\r\nLooped, %u to go\r\n", channels_data[i].loop_count);

										if (channels_data[i].loop_count == 0) {
											
											//printf("\r\nReached 0, ending loop\r\n");

											channels_data[i].loop_live = false;

										} else mod.current_row = channels_data[i].loop_row;

									}

								}

							} break;

							case EXT_TREM_WAVE: {//Set waveform (tremolo)

								if (param_y == 0) channels_data[i].tremolo_retrigger = true;
								else if (param_y == 4) channels_data[i].tremolo_retrigger = false;

							} break;

							case EXT_VOL_UP: {

								volume_slide(i);

							} break;

							case EXT_VOL_DOWN: {

								volume_slide(i);

							} break;										
							
							case EXT_REPEAT_NOTE: {//Repeat row

								if (mod.row_repeat_live == true) {
									if (mod.row_repeat == 0) mod.row_repeat_live = false;
									else {
										mod.row_repeat--;
										mod.current_row--;
									}
								} else {
									mod.current_row--;
									mod.row_repeat = param_y - 1;
									mod.row_repeat_live = true;
								}

							} break;														

						}


					} break;				

					case EFFECT_TEMP_SPEED: {//Set speed or tempo

						if (channels_data[i].current_effect_param < 0x20) { //<0x20 means speed (i.e. ticks per row)

							mod.current_speed = channels_data[i].current_effect_param;

						}

						else if (channels_data[i].current_effect_param >= 0x20) { //<0x20 means bpm/tempo (i.e. ticker period)

							mod.current_bpm = channels_data[i].current_effect_param;
							timer_end(TIMER_NO);
							timer_begin(TIMER_NO, (rr_array[mod.current_bpm - 0x20]), div_array[mod.current_bpm - 0x20]);

						}

					} break;				

					default:
						break;

				}		

			}

			#ifdef VERBOSE

				if (mod.channels == 4) {
				
					//## FFF SS VV EEE FFF SS VV EEE FFF SS VV EEE FFF SS VV EEE
					printf("%s %02u %02u %X%02X", period_to_note(period), sample_number, (channels_data[i].current_volume), effect_number, effect_param);
					if (i != mod.channels - 1) printf(" ");

				} else if (mod.channels == 6) {

					//## FFF SS FFF SS FFF SS FFF SS FFF SS FFF SS
					printf("%s %02u %02u %X%02X", period_to_note(period), sample_number, (channels_data[i].current_volume), effect_number, effect_param);
					if (i != mod.channels - 1) printf(" ");			


				} else if (mod.channels == 8) {
					
					   //## FFF SS FFF SS FFF SS FFF SS FFF SS FFF SS FFF SS FFF SS
					printf("%s %02u", period_to_note(period), sample_number);
					if (i != mod.channels - 1) printf(" ");
					
				}

			#endif

			//Move on to next channel/note in memory

			//noteData += 4;

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

}

void process_tick() {

	for (uint8_t i = 0; i < mod.channels; i++) {

		if (channels_data[i].current_effect != 0xFF) {

			switch (channels_data[i].current_effect) {

				case EFFECT_ARPEGGIO: { //Arpeggio

					uint8_t x = channels_data[i].current_effect_param >> 4;
					uint8_t y = channels_data[i].current_effect_param & 0x0F;

					uint8_t r = mod.tick_no % 3;
					if (r == 0) {
						channels_data[i].current_hz = mod.pd_hz / clamp_period(channels_data[i].tuned_period);
						set_frequency(i, channels_data[i].current_hz);
					}
					else if (r == 1) {
						channels_data[i].current_hz = mod.pd_hz / clamp_period(channels_data[i].tuned_period) + (x * 8);
						set_frequency(i, channels_data[i].current_hz);
					}
					else if (r == 2) {
						channels_data[i].current_hz = mod.pd_hz / clamp_period(channels_data[i].tuned_period) + (y * 8);
						set_frequency(i, channels_data[i].current_hz);
					}

				} break;

				case EFFECT_PORTA_UP: { //Pitch slide (porta) up

					//pitch_slide_up(i, channels_data[i].current_effect_param);
					pitch_slide(i);

				} break;

				case EFFECT_PORTA_DOWN: { //Pitch slide (porta) down	
					
					//pitch_slide_down(i, channels_data[i].current_effect_param);
					pitch_slide(i);

				} break;

				case EFFECT_PORTA_NOTE: { //Pitch slide toward target note (tone portamento)

					//pitch_slide_directional(i);
					pitch_slide(i);

				} break;				

				case EFFECT_VIBRATO: {//Vibrato
					
					do_vibrato(i);

				} break;

				case EFFECT_VOL_TONE: {//Volume Slide + Tone Portamento
					
					//volume_slide(i, channels_data[i].current_effect_param);
					volume_slide(i);

					//pitch_slide_directional(i);
					pitch_slide(i);

				} break;	

				case EFFECT_VOL_VIBRATO: {//Volume Slide + Vibrato

					//volume_slide(i, channels_data[i].current_effect_param);					
					volume_slide(i);

					do_vibrato(i);

				} break;	

				case EFFECT_TREMULO: {//Tremolo
					
					do_tremulo(i);

				} break;				

				case EFFECT_VOL_SLIDE: { //Volume slide

					//volume_slide(i, channels_data[i].current_effect_param);
					volume_slide(i);

				} break;

				case EFFECT_EXTENDED: { //Extended effects

					uint8_t param_x = channels_data[i].current_effect_param >> 4;
					uint8_t param_y = channels_data[i].current_effect_param & 0x0F;

					switch (param_x) {

						case 0x09: { //Retrigger

							if (mod.tick_no % param_y == 0) dispatch_channel(i);
			
						} break;
						
						case 0x0C: { //Cut volume on tick y

							if (mod.tick_no == param_y) {
								
								channels_data[i].current_volume = 0;
								set_volume(i, channels_data[i].current_volume);

							}
			
						} break;

						case EXT_DELAY_NOTE: {//Delay sample change

							dispatch_channel(i);

						} break;	

					}

				} break;				

				default:
					break;

			}		

		}

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

void print_arb(uint16_t x, uint16_t y, uint8_t colour, const char* format, ...) {
    va_list args;

    putch(5);
    vdu_move(x, y);
	set_graphics_foreground(colour);

    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    putch(4);
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

	//void draw_progress_bar(int total, int progress, int maxBarLength, int upperX, int upperY, int barHeight, uint8_t bg, uint8_t fg) {
	//draw_progress_bar(127, global_volume, 128, 30, 49, 3, 7, 15);

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

	set_graphics_window(8,228,152,214); //Left, bottom, right, top, remember

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
	draw_rect(148,228,152,228 - mean_vol); //Draw a mean bar (cyan)
	
	set_graphics_window(0,239,639,0); //Ensure graphics window is full

	//Order update
	
	//left, bottom, right, top
	set_text_window(0,26,20,25);
	putch(17);
	putch(15); //White row text
			printf(" Page %03u of %03u", mod.current_order + 1, mod.header_data.mod_header.num_orders);

	//BPM/Speed update

	//left, bottom, right, top
	set_text_window(0,30,20,29);
	printf(" BPM: %03u Speed: %02u", mod.current_bpm, mod.current_speed);
	set_text_window(20,29,80,2);
	cursor_tab(0,27);

}

void update_viz() {
    putch(0x0C); // Clear Screen (CLS)

    // Calculate the width of each line
    uint8_t line_width = 2;
    uint16_t available_width = 320;
    uint16_t total_line_width = mod.channels * line_width;
    
    // Calculate the spacing between lines so they are centered
    uint16_t spacing_between_lines = (available_width - total_line_width) / (mod.channels + 1);

    for (uint8_t i = 0; i < mod.channels; i++) {
        
		uint8_t vol = channels_data[i].current_volume;
		if (vol < 2) vol = 2;
		if (vol > 64) vol = 64;
		vol *= 2;

		uint16_t line_start_x = spacing_between_lines + i * (line_width + spacing_between_lines);
        uint16_t line_end_x = line_start_x + line_width;
        
        // Ensure the values are within screen dimensions
        if (line_end_x <= available_width) {
            set_graphics_foreground(15);
			draw_rect(line_start_x, 40, line_end_x, 180);
			
			set_graphics_foreground(15 + channels_data[i].latched_sample);
			draw_rect(line_start_x - 8, 171 - vol, line_end_x + 8, 177 - vol);

        }
    }

	switch_buffers();

}

//S3M support functions
file_format_t detect_file_format(const char* filename) {
	FILE* test_file = fopen(filename, "rb");
	if (!test_file) return FORMAT_MOD; // Default to MOD if can't open
	
	// Check for S3M signature at offset 0x2C
	fseek(test_file, 0x2C, SEEK_SET);
	char sig[4];
	fread(sig, 1, 4, test_file);
	
	if (strncmp(sig, "SCRM", 4) == 0) {
		fclose(test_file);
		return FORMAT_S3M;
	}
	
	// Check for MOD signatures at beginning of file
	fseek(test_file, 0, SEEK_SET);
	fread(sig, 1, 4, test_file);
	fclose(test_file);
	
	if (strncmp(sig, "M.K.", 4) == 0 || 
		strncmp(sig, "FLT4", 4) == 0 ||
		strncmp(sig, "6CHN", 4) == 0 ||
		strncmp(sig, "8CHN", 4) == 0) {
		return FORMAT_MOD;
	}
	
	// Default to MOD for unknown formats
	return FORMAT_MOD;
}

void convert_16bit_to_8bit(uint8_t* dest, const int16_t* src, uint16_t length) {
	for (uint16_t i = 0; i < length; i++) {
		// Convert 16-bit signed (-32768 to 32767) to 8-bit unsigned (0 to 255)
		int32_t sample = src[i];
		sample = (sample + 32768) >> 8; // Shift and offset to 8-bit range
		dest[i] = (uint8_t)clamp_volume(sample);
	}
}

// S3M pattern loading functions
uint16_t* load_s3m_patterns(FILE* file, const s3m_file_header* header) {
	// Allocate memory for pattern pointer table
	uint16_t num_patterns = swap_word(header->num_patterns);
	uint16_t* pattern_pointers = malloc(num_patterns * sizeof(uint16_t));
	if (!pattern_pointers) return NULL;
	
	// Read pattern pointer table (starts at offset 0x7C)
	fseek(file, 0x7C, SEEK_SET);
	for (uint16_t i = 0; i < num_patterns; i++) {
		uint16_t offset;
		fread(&offset, sizeof(uint16_t), 1, file);
		pattern_pointers[i] = swap_word(offset) * 16; // Convert to byte offset
	}
	
	return pattern_pointers;
}

s3m_pattern_header* load_s3m_pattern(FILE* file, uint16_t pattern_offset) {
	// Seek to pattern data
	fseek(file, pattern_offset, SEEK_SET);
	
	// Read pattern header
	s3m_pattern_header* pattern = malloc(sizeof(s3m_pattern_header));
	if (!pattern) return NULL;
	
	fread(&pattern->length, sizeof(uint16_t), 1, file);
	pattern->length = swap_word(pattern->length);
	
	// Allocate memory for pattern data
	pattern = realloc(pattern, sizeof(s3m_pattern_header) + pattern->length);
	if (!pattern) return NULL;
	
	// Read pattern data
	fread(pattern->data, 1, pattern->length, file);
	
	return pattern;
}

void parse_s3m_pattern_row(const uint8_t* data, uint16_t* offset, s3m_note* notes, uint8_t num_channels) {
	uint16_t pos = *offset;
	uint8_t channel = 0;
	
	// Initialize all notes to "no note"
	for (uint8_t i = 0; i < num_channels; i++) {
		notes[i].note = 255;
		notes[i].instrument = 0;
		notes[i].volume = 255;
		notes[i].effect = 0;
		notes[i].effect_data = 0;
	}
	
	// Parse row data
	while (pos < 64 && channel < num_channels) {
		uint8_t byte = data[pos++];
		
		if (byte == 0) break; // End of row
		
		// Channel mask
		uint8_t channel_mask = byte & 0x1F;
		channel = channel_mask;
		
		// Note data
		if (byte & 0x20) {
			notes[channel].note = data[pos++];
		}
		
		// Instrument data
		if (byte & 0x40) {
			notes[channel].instrument = data[pos++];
		}
		
		// Volume data
		if (byte & 0x80) {
			notes[channel].volume = data[pos++];
		}
		
		// Effect data
		if (pos < 64) {
			notes[channel].effect = data[pos++];
			if (pos < 64) {
				notes[channel].effect_data = data[pos++];
			}
		}
	}
	
	*offset = pos;
}

void on_tick()
{
	ticker++;

	if (mod_ready && ticker > 0) {

		if ((ticker - old_ticker) >= mod.current_speed) {

			old_ticker = ticker, tick = ticker;
			mid_tick = 0;

			handle_breaks();
			
			// Handle different file formats
			if (mod.format == FORMAT_S3M) {
				// S3M format - get pattern from order list
				uint8_t pattern_num = mod.s3m_order_list[mod.current_order];
				if (pattern_num < swap_word(mod.header_data.s3m_header.num_patterns)) {
					process_note(pattern_num, mod.current_row++);
				} else {
					// Invalid pattern, skip to next order
					mod.current_row = 64;
				}
			} else {
				// MOD format
				process_note(mod.header_data.mod_header.order[mod.current_order], mod.current_row++);
			}
			
			#if !defined(VERBOSE) && !defined(VIZ) && !defined(HEADLESS)
			if (mod.format == FORMAT_S3M) {
				printf("\rPlaying S3M song page %03u/%03u row %02u, press ESCAPE to exit.", mod.current_order + 1, swap_word(mod.header_data.s3m_header.num_orders), mod.current_row);
			} else {
				printf("\rPlaying song page %03u/%03u row %02u, press ESCAPE to exit.", mod.current_order + 1, mod.header_data.mod_header.num_orders, mod.current_row);
			}
			#endif
			
			#ifdef VERBOSE
			draw_sample_bars();
			#endif
			#ifdef VIZ
			update_viz();
			#endif			

			if (mod.current_row == 64 && !(mod.order_break_pending)) {

				mod.current_order++;
				if (mod.format == FORMAT_S3M) {
					if ((mod.current_order) > swap_word(mod.header_data.s3m_header.num_orders) - 1) mod.current_order = 0;
				} else {
					if ((mod.current_order) > mod.header_data.mod_header.num_orders - 1) mod.current_order = 0;
				}
				//printf("\r\nOrder %u (Pattern %u)\r\n", mod.current_order, mod.header_data.mod_header.order[mod.current_order]);
				//header_line();
				mod.current_row = 0;
			}

		} else if (ticker - tick > 0) { //We're in between rows

			if (mid_tick++ < mod.current_speed) {

				tick = ticker;
				mod.tick_no++;
				process_tick();
				#ifdef VERBOSE
				draw_sample_bars();
				#endif				

			}

		}	

	}

}

int main(int argc, char * argv[])       
{

	#ifndef HEADLESS
	sv = vdp_vdu_init();
	#endif
	if ( vdp_key_init() == -1 ) return 1;

	#ifdef HEADLESS
	if (strncmp(argv[1], "STOP", 4) == 0 || strncmp(argv[1], "stop", 4) == 0) {
		handle_exit(NULL, false);
		return 0;
	}
	#endif

	#ifdef PRINT_DEBUG
	print_to_debug("\r\nDebug starting.\r\n");
	#endif

	if (argc < 2) {
		handle_exit("Usage is playmod <file> [alternative magic number]", false);
		return 0;
	}

	if (argc >= 3) mod.pd_hz = atoi(argv[2]);
	else mod.pd_hz = PD_HZ;

	if (argc >= 4) {

		uint8_t param = atoi(argv[3]);

		for (uint8_t i = 0; i < MAX_CHANNELS - 1; i++) {

			if (test_bit(param, i)) mod.channel_disabled[i] = true;

		}
		 
	}

	file = fopen(argv[1], "rb");
    if (file == NULL) {
        handle_exit("Could not open file.", false);
		return 0;
    }
	
	// Detect file format
	current_format = detect_file_format(argv[1]);
	
	#ifndef HEADLESS
	printf("Agon_MOD (v%03u)\r\n", VERSION);	
	if (current_format == FORMAT_S3M) {
		printf("Reading .S3M header\r\n");
	} else {
		printf("Reading .MOD header\r\n");
	}
	#endif

	if (current_format == FORMAT_S3M) {
		// Handle S3M file
		s3m_file_header s3m_header;
		fread(&s3m_header, sizeof(s3m_file_header), 1, file);
		
		// Validate S3M signature
		if (strncmp(s3m_header.sig, "SCRM", 4) != 0) {
			handle_exit("Invalid S3M file signature.", false);
			return 0;
		}
		
		// Set basic parameters
		mod.channels = 32; // S3M supports up to 32 channels
		mod.current_speed = 6; // Default speed (will be updated when patterns are loaded)
		mod.current_bpm = 125; // Default BPM (will be updated when patterns are loaded)
		
		#ifndef HEADLESS
		printf("S3M: %s, %d channels, %d orders, %d patterns\r\n", 
			s3m_header.name, mod.channels, swap_word(s3m_header.num_orders), swap_word(s3m_header.num_patterns));
		#endif
		
		// Load S3M samples
		#ifndef HEADLESS
		printf("Loading S3M samples...\r\n");
		#endif
		
		// Skip order list (already read)
		// Skip pattern list (we'll implement pattern loading later)
		
		// Read sample headers
		uint32_t sample_lengths[256]; // Sample lengths
		uint32_t sample_loop_starts[256]; // Loop start positions
		uint32_t sample_loop_ends[256]; // Loop end positions
		uint8_t sample_volumes[256]; // Sample volumes
		
		// Read sample headers
		for (uint8_t i = 0; i < swap_word(s3m_header.num_instruments); i++) {
			s3m_sample_header sample_header;
			fread(&sample_header, sizeof(s3m_sample_header), 1, file);
			
			// Validate sample header signature
			if (strncmp(sample_header.magic, "SCRS", 4) != 0) {
				#ifndef HEADLESS
				printf("Warning: Invalid sample header signature for sample %d\r\n", i);
				#endif
				continue;
			}
			
			// Store sample info
			sample_lengths[i] = sample_header.length;
			sample_loop_starts[i] = sample_header.loop_start;
			sample_loop_ends[i] = sample_header.loop_end;
			sample_volumes[i] = sample_header.volume;
			
			#ifndef HEADLESS
			printf("Sample %d: %s, length: %d, loop: %d-%d, volume: %d\r\n", 
				i, sample_header.name, sample_header.length, 
				sample_header.loop_start, sample_header.loop_end, sample_header.volume);
			#endif
		}
		
		// Calculate sample data start position
		// After all sample headers, sample data starts
		uint32_t sample_data_start = ftell(file);
		
		// Now read and upload samples
		mod.sample_total = 0;
		for (uint8_t i = 0; i < swap_word(s3m_header.num_instruments); i++) {
			if (sample_lengths[i] > 0) {
				// Calculate sample data offset
				uint32_t sample_offset = sample_data_start;
				for (uint8_t j = 0; j < i; j++) {
					sample_offset += sample_lengths[j] * 2; // 16-bit samples
				}
				
				// Seek to sample data
				fseek(file, sample_offset, SEEK_SET);
				
				// Read 16-bit sample data
				uint32_t sample_size = sample_lengths[i] * 2; // 16-bit samples
				uint8_t temp_sample_buffer[512]; // Increased buffer size
				
				if (sample_size <= sizeof(temp_sample_buffer)) {
					// Sample fits in buffer
					clear_buffer(i);
					
					// Read 16-bit data
					int16_t* sample_16bit = (int16_t*)temp_sample_buffer;
					fread(sample_16bit, sizeof(int16_t), sample_lengths[i], file);
					
					// Convert to 8-bit
					uint8_t* sample_8bit = (uint8_t*)temp_sample_buffer;
					convert_16bit_to_8bit(sample_8bit, sample_16bit, sample_lengths[i]);
					
					// Upload to Agon
					add_stream_to_buffer(i, (char*)sample_8bit, sample_lengths[i]);
					
				} else {
					// Sample needs multiple chunks
					clear_buffer(i);
					uint32_t remaining_data = sample_size;
					uint32_t bytes_read = 0;
					
					while (remaining_data > 0) {
						uint32_t chunk_size = (remaining_data > sizeof(temp_sample_buffer)) ? 
							sizeof(temp_sample_buffer) : remaining_data;
						
						// Read 16-bit chunk
						int16_t* sample_16bit = (int16_t*)temp_sample_buffer;
						uint32_t words_to_read = chunk_size / 2;
						fread(sample_16bit, sizeof(int16_t), words_to_read, file);
						
						// Convert to 8-bit
						uint8_t* sample_8bit = (uint8_t*)temp_sample_buffer;
						convert_16bit_to_8bit(sample_8bit, sample_16bit, words_to_read);
						
						// Upload chunk to Agon
						add_stream_to_buffer(i, (char*)sample_8bit, words_to_read);
						
						remaining_data -= chunk_size;
						bytes_read += chunk_size;
					}
				}
				
				// Set sample properties
				mod.sample_live[i] = true;
				mod.sample_volume[i] = sample_volumes[i];
				
				// Set loop points if they exist
				if (sample_loop_ends[i] > sample_loop_starts[i] && sample_loop_ends[i] > 1) {
					set_sample_loop_start(i, sample_loop_starts[i] * 2);
					set_sample_loop_length(i, (sample_loop_ends[i] - sample_loop_starts[i]) * 2);
				}
				
				// Set sample frequency (C2 note = 8363 Hz)
				tuneable_sample_from_buffer(i, 8363);
				
				mod.sample_total++;
				
				#ifndef HEADLESS
				printf("\rUploading sample: %02u", i);
				#endif
			} else {
				mod.sample_live[i] = false;
			}
		}
		
		#ifndef HEADLESS
		printf("\r\n");
		#endif
		
		// Set pattern info for now
		mod.pattern_max = swap_word(s3m_header.num_patterns) - 1;
		
		// Load S3M patterns
		#ifndef HEADLESS
		printf("Loading S3M patterns...\r\n");
		#endif
		
		// Store the S3M header in our global mod structure
		mod.format = FORMAT_S3M;
		mod.header_data.s3m_header = s3m_header;
		
		// Load order list (song sequence)
		fseek(file, 0x3C, SEEK_SET); // Order list starts at 0x3C
		fread(mod.s3m_order_list, 1, swap_word(s3m_header.num_orders), file);
		
		// Load pattern pointer table
		uint16_t* pattern_pointers = load_s3m_patterns(file, &s3m_header);
		if (!pattern_pointers) {
			handle_exit("Failed to load S3M pattern pointers.", false);
			return 0;
		}
		
		// Load all patterns into memory
		uint16_t num_patterns = swap_word(s3m_header.num_patterns);
		s3m_pattern_header** patterns = malloc(num_patterns * sizeof(s3m_pattern_header*));
		if (!patterns) {
			handle_exit("Failed to allocate pattern memory.", false);
			free(pattern_pointers);
			return 0;
		}
		
		// Load each pattern
		for (uint16_t i = 0; i < num_patterns; i++) {
			patterns[i] = load_s3m_pattern(file, pattern_pointers[i]);
			if (!patterns[i]) {
				handle_exit("Failed to load pattern.", false);
				// Clean up already loaded patterns
				for (uint16_t j = 0; j < i; j++) {
					free(patterns[j]);
				}
				free(patterns);
				free(pattern_pointers);
				return 0;
			}
		}
		
		// Store patterns for later use
		mod.pattern_buffer = (uint8_t*)patterns;
		free(pattern_pointers); // No longer needed
		
		#ifndef HEADLESS
		printf("Loaded %d patterns\r\n", swap_word(s3m_header.num_patterns));
		#endif
		
	} else {
		// Handle MOD file (existing code)
		mod.format = FORMAT_MOD;
		fread(&mod.header_data.mod_header, sizeof(mod_file_header), 1, file);

	if (strncmp(mod.header_data.mod_header.sig, "M.K.", 4) == 0) {
		mod.channels = 4; //Classic 4 channels
		#ifdef VARIABLE_RATE
		set_channel_rate(-1, RATE_4_CHAN);
		#endif
	}
	else if (strncmp(mod.header_data.mod_header.sig, "FLT4", 4) == 0) {
		mod.channels = 4; //Startrekker 4 channels
		#ifdef VARIABLE_RATE
		set_channel_rate(-1, RATE_4_CHAN);
		#endif		
	}
	else if (strncmp(mod.header_data.mod_header.sig, "6CHN", 4) == 0) {
		mod.channels = 6; //6 channels
		#ifdef VARIABLE_RATE
		set_channel_rate(-1, RATE_6_CHAN);
		#endif		
	}
	else if (strncmp(mod.header_data.mod_header.sig, "8CHN", 4) == 0) {
		mod.channels = 8; //8 channels
		#ifdef VARIABLE_RATE
		set_channel_rate(-1, RATE_8_CHAN);
		#endif				
	}
	else {

		handle_exit("Unknown .mod format, only 4 or 8 channel .MODs are supported.", false);
		return 0;

	}

	//channels_data = (channel_data*) malloc(sizeof(channel_data) * mod.channels);
	//channel_data channels_data[8];

	if (argc >= 5) mod.current_order = atoi(argv[4]) - 1;
	else mod.current_order = 0;

	ticker = 0;
	timer_begin(TIMER_NO, rr_array[mod.current_bpm - 0x20], div_array[mod.current_bpm - 0x20]);

	mod.pattern_break_pending = false;
	mod.order_break_pending = false;

	for (uint8_t i = 1; i < 31; i++) mod.sample_volume[i] = 0;

	for (uint8_t i = 0; i < 127; i++) if (mod.header_data.mod_header.order[i] > mod.pattern_max) mod.pattern_max = mod.header_data.mod_header.order[i];

	#ifndef HEADLESS
	printf("Reading pattern data...\r\n");
	#endif

	//Number of patterns * number of channels * number of bytes per note per channel * number of notes per pattern (i.e. 1024 bytes per 4 channel pattern)
	//mod.pattern_buffer = (uint8_t*) malloc(sizeof(uint8_t) * (mod.pattern_max + 1) * mod.channels * 4 * 64);
	//fread(mod.pattern_buffer, sizeof(uint8_t), (mod.pattern_max + 1) * mod.channels * 4 * 64, file);
	fseek(file, (mod.pattern_max + 1) * mod.channels * 4 * 64, SEEK_CUR);

	//uint8_t *temp_sample_buffer;
	//mod.sample_total = 0;

	for (uint8_t i = 1; i < 31; i++) {

		uint16_t sample_length_swapped = swap_word(mod.header_data.mod_header.sample[i - 1].SAMPLE_LENGTH);
		uint16_t sample_loop_start_swapped = swap_word(mod.header_data.mod_header.sample[i - 1].LOOP_START);
		uint16_t sample_loop_length_swapped = swap_word(mod.header_data.mod_header.sample[i - 1].LOOP_LENGTH);

		if (sample_length_swapped > 0) {

			mod.sample_total++;
			mod.sample_live[i] = true;

			// if (1) {
			// 	printf("Uploading sample %u", i);
			// 	printf(" %02u bytes, def. vol %02X", (sample_length_swapped * 2), mod.header_data.mod_header.sample[i - 1].VOLUME);			
			// 	printf(", loop start %05u", sample_loop_start_swapped * 2);
			// 	printf(", loop length %05u", sample_loop_length_swapped * 2);
			// 	printf(", finetune byte %u", mod.header_data.mod_header.sample[i - 1].FINE_TUNE);
			// 	printf("\r\n");
			// 	return 0;
			// }

			//if ((sample_length_swapped * 2) < 300) mod.bad_samples = true;

			uint8_t temp_sample_buffer[256];

			if ((sample_length_swapped * 2) <= CHUNK_SIZE) {

				clear_buffer(i);
				// temp_sample_buffer = (uint8_t*) malloc(sizeof(uint8_t) * (sample_length_swapped * 2));
				// if (temp_sample_buffer == NULL) {
				// 	handle_exit("Local sample memory allocation failed", false);
				// 	return 0;	
				// }
				fread(temp_sample_buffer, sizeof(uint8_t), (sample_length_swapped * 2), file);
				add_stream_to_buffer(i, (char *)temp_sample_buffer, (sample_length_swapped * 2));		

			} else {

				uint24_t remaining_data = sample_length_swapped * 2;
				uint16_t chunk;

				clear_buffer(i);
				// temp_sample_buffer = (uint8_t*) malloc(sizeof(uint8_t) * CHUNK_SIZE);
				// if (temp_sample_buffer == NULL) {
				// 	handle_exit("Local sample memory allocation failed", false);
				// 	return 0;	
				// }

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

			}

			#ifndef HEADLESS
			printf("\rUploading sample: %02u", i);
			#endif

		} else mod.sample_live[i] = false;

	}

	#ifndef HEADLESS
	printf("\r\n");
	#endif
	} // End of MOD handling else block

	// Initialize channels for both formats
	//channels_data = (channel_data*) malloc(sizeof(channel_data) * mod.channels);
	//channel_data channels_data[8];
	
	for (uint8_t i = 0; i < mod.channels; i++) {
		enable_channel(i);
		reset_channel(i);
		set_volume(i, 0);
		set_frequency(i, 0);
		channels_data[i].current_volume = 0;
		channels_data[i].current_effect = 0xFF;
		channels_data[i].loop_live = false;
		channels_data[i].vibrato_retrigger = true;
		channels_data[i].tremolo_retrigger = true;		
		
	}

	// Set default values if not already set by S3M
	if (current_format == FORMAT_MOD) {
		mod.current_speed = 6;
		mod.current_bpm = 125;
	}

	//free(temp_sample_buffer);
	
	old_ticker = ticker;
	uint16_t old_key_count = sv->vkeycount;

	set_volume(255, global_volume);

	#ifdef VERBOSE

		old_mode = sv->scrMode;

		if (sv->scrMode != 4) {
			putch(22);
			putch(4);
		}

		logical_coords(false);

		putch(0x0C); //CLS
		cursor_set(false);

		//Set up the UI
		//left, bottom, right, top
		set_text_window(0,29,22,1);

		printf("Agon_MOD (v%03u)", VERSION);
		if (mod.pd_hz != PD_HZ) printf(" [%06u]", mod.pd_hz);
		printf("\r\n\r\n");
		printf("Mod title:\r\n%.20s\r\n\r\nVol \r\n\r\n", mod.header_data.mod_header.name);
	
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

		//set_graphics_foreground(7);
		//draw_rect(8,198,152,230); //Volume background
		rect_drop_shadow(8,214,152,228,2,4,7,8);
		
		set_text_window(20,2,80,1);

		header_line();

		set_text_window(20,29,80,2);

		fill_empty(27);

	#endif

	#ifdef VIZ
	
		old_mode = sv->scrMode;

		if (sv->scrMode != 12) {
			putch(22);
			putch(12);
		}

		logical_coords(false);

		putch(0x0C); //CLS
	
	#endif

			//process_note(mod.header_data.mod_header.order[mod.current_order], mod.current_row++);
	// #if !defined(VERBOSE) && !defined(VIZ) && !defined(HEADLESS)
	// printf("\rPlaying song page %03u/%03u row %02u, press ESCAPE to exit.", mod.current_order + 1, mod.header_data.mod_header.num_orders, mod.current_row);
	// #endif

	#ifdef VERBOSE
	draw_sample_bars();
	#endif
	#ifdef VIZ
	update_viz();
	#endif

	tick = 0;

	mod_ready = true;

	#ifdef HEADLESS
	return 0;
	#endif

	while (1) {

		if (sv->vkeycount != old_key_count) {

			if (sv->keyascii == 27 || sv->keyascii == 'q') {
				timer_end(TIMER_NO);
				break;
			}

			else if (sv->keyascii == '+') {
				global_volume += 5;
				if (global_volume > 125) global_volume = 125;
				set_volume(255, global_volume);
			}
			else if (sv->keyascii == '-') {
				global_volume -= 5;
				if (global_volume < 0) global_volume = 0;
				set_volume(255, global_volume);
			}

			old_key_count = sv->vkeycount;

			}	
	
	}
	#if !defined(VERBOSE) && !defined(VIZ) && !defined(HEADLESS)
	printf("\r\n");
	#endif

	#if defined(VERBOSE) || defined(VIZ)
	handle_exit(NULL, true);
	#endif

	return 0;

}