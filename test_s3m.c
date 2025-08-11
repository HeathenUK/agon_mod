#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// File format enum
typedef enum {
	FORMAT_MOD,
	FORMAT_S3M
} file_format_t;

// S3M file header structure
typedef struct __attribute__((packed)) {
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

// S3M sample header structure
typedef struct __attribute__((packed)) {
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

// Function to swap bytes for 16-bit values (little-endian to big-endian)
uint16_t swap16(uint16_t value) {
	return (value >> 8) | ((value & 0xFF) << 8);
}

// Function to detect file format
file_format_t detect_file_format(const char* filename) {
	FILE* file = fopen(filename, "rb");
	if (!file) return FORMAT_MOD;
	
	// Check for S3M signature at offset 0x2C
	fseek(file, 0x2C, SEEK_SET);
	char sig[5];
	fread(sig, 1, 4, file);
	sig[4] = '\0';
	fclose(file);
	
	if (strcmp(sig, "SCRM") == 0) {
		return FORMAT_S3M;
	}
	
	return FORMAT_MOD;
}

// Function to convert 16-bit to 8-bit
void convert_16bit_to_8bit(uint8_t* dest, const int16_t* src, uint16_t length) {
	for (uint16_t i = 0; i < length; i++) {
		// Convert 16-bit signed (-32768 to 32767) to 8-bit unsigned (0 to 255)
		int32_t sample = src[i];
		sample = (sample + 32768) >> 8; // Shift and offset to 8-bit range
		dest[i] = (uint8_t)(sample > 255 ? 255 : (sample < 0 ? 0 : sample));
	}
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		printf("Usage: %s <s3m_file>\n", argv[0]);
		return 1;
	}
	
	// Detect file format
	file_format_t format = detect_file_format(argv[1]);
	printf("Detected format: %s\n", format == FORMAT_S3M ? "S3M" : "MOD");
	
	if (format != FORMAT_S3M) {
		printf("Not an S3M file!\n");
		return 1;
	}
	
	// Open and read S3M file
	FILE* file = fopen(argv[1], "rb");
	if (!file) {
		printf("Could not open file!\n");
		return 1;
	}
	
	// Read S3M header
	s3m_file_header header;
	fread(&header, sizeof(s3m_file_header), 1, file);
	
	// Validate signature
	if (strncmp(header.sig, "SCRM", 4) != 0) {
		printf("Invalid S3M signature: %.4s\n", header.sig);
		fclose(file);
		return 1;
	}
	
	printf("Song name: %.28s\n", header.name);
	printf("Type: 0x%02X\n", header.type);
	printf("Orders: %d\n", swap16(header.num_orders));
	printf("Instruments: %d\n", swap16(header.num_instruments));
	printf("Patterns: %d\n", swap16(header.num_patterns));
	printf("Flags: 0x%04X\n", swap16(header.flags));
	printf("Tracker version: %d\n", swap16(header.tracker_version));
	printf("File version: %d\n", swap16(header.file_version));
	printf("Global volume: %d\n", header.global_volume);
	printf("Master volume: %d\n", header.master_volume);
	
	// Read order list
	uint8_t order_list[256];
	fseek(file, 0x3C, SEEK_SET);
	uint16_t num_orders = swap16(header.num_orders);
	fread(order_list, 1, num_orders, file);
	
	printf("Order list: ");
	for (int i = 0; i < num_orders; i++) {
		printf("%d ", order_list[i]);
	}
	printf("\n");
	
	// Read pattern pointer table
	uint16_t pattern_pointers[256];
	fseek(file, 0x7C, SEEK_SET);
	uint16_t num_patterns = swap16(header.num_patterns);
	for (int i = 0; i < num_patterns; i++) {
		uint16_t offset;
		fread(&offset, sizeof(uint16_t), 1, file);
		pattern_pointers[i] = (offset >> 8) | ((offset & 0xFF) << 8); // Swap bytes
		printf("Pattern %d offset: 0x%04X\n", i, pattern_pointers[i] * 16);
	}
	
	// Read sample headers
	printf("\nSample headers:\n");
	uint16_t num_instruments = swap16(header.num_instruments);
	for (int i = 0; i < num_instruments; i++) {
		s3m_sample_header sample_header;
		fread(&sample_header, sizeof(s3m_sample_header), 1, file);
		
		if (strncmp(sample_header.magic, "SCRS", 4) == 0) {
			printf("Sample %d: %s, type: %d, length: %d, volume: %d\n", 
				   i, sample_header.name, sample_header.type, 
				   sample_header.length, sample_header.volume);
		}
	}
	
	fclose(file);
	printf("\nS3M file loaded successfully!\n");
	
	return 0;
}