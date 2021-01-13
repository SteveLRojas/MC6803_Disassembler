#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static const char* LENGTH_TABLE_FILE = "MC6803_LEN.BIN";
static const char* NAME_TABLE_FILE = "MC6803_OP.BIN";

uint16_t* stack_mem = NULL;
uint16_t stack_index = 0;
void CreateStack(unsigned int size)
{
	stack_mem = (uint16_t*)malloc(size * sizeof(uint16_t));
	stack_index = 0;
}

void push(uint16_t data)
{
	stack_mem[stack_index] = data;
	stack_index = stack_index + 1;
}

uint16_t pop()
{
	stack_index = stack_index - 1;
	return stack_mem[stack_index];
}

uint16_t peek()
{
	return stack_mem[stack_index];
}

uint8_t isEmpty()
{
	return (stack_index == 0);
}

void print_and_free(char** program_strings, char* output_file)
{
	FILE* out_file = fopen(output_file, "w+");
	if(out_file == NULL)
	{
		printf("Failed to open/create output file, exiting...\n");
		exit(1);
	}
	for(int i = 0; i < 65536; i++)
	{
		if(program_strings[i])
		{
			fprintf(out_file, "%s\n", program_strings[i]);
			free(program_strings[i]);
		}
	}
	fclose(out_file);
}

void push_user(char* filename)
{
	FILE* entry_file = fopen(filename, "r");
	if(entry_file == NULL)
	{
		printf("Failed to open user input file, exiting...\n");
		exit(1);
	}

	char* buf = NULL;
	size_t len = 0;
	int read;

	while((read = getline(&buf, &len, entry_file)) != -1)
	{
		// Skip invalid lines - ex. 0x, \n, etc.
		if(read < 3)
			continue;
		uint16_t number = (uint16_t)strtol(buf, NULL, 0);
		push(number);
		printf("Pushing 0x%X onto stack!\n", number);
	}
	free(buf);
}


uint8_t get_opcode_length(uint8_t opcode, uint8_t* len_table)
{
	return len_table[opcode];
}


char* get_opcode_name(uint8_t opcode, uint8_t* name_table)
{
	uint16_t addr = name_table[2 * opcode] | (name_table[(2 * opcode) + 1] << 8);
	return (char*)(name_table + addr);
}


void push_next(uint8_t opcode, uint8_t inst_len, int8_t arg1, int8_t arg2, uint16_t curr)
{
	uint16_t next;
	uint16_t extended;

	switch (opcode)
	{
		// BRA REL
		case 0x20:
			next = curr + inst_len + arg1;
			push(next);
			break;

		// BHI, BLS, BNE, BCC, BCS, BEQ, BVC, BVS, BPL, BMI, BGE, BLT, BGT, BLE
		case 0x22:
		case 0x23:
		case 0x24:
		case 0x25:
		case 0x26:
		case 0x27:
		case 0x28:
		case 0x29:
		case 0x2A:
		case 0x2B:
		case 0x2C:
		case 0x2D:
		case 0x2E:
		case 0x2F:
			next = curr + inst_len + arg1;
			push(next);
			next = curr + inst_len;
			push(next);
			break;

		// RTS
		case 0x39:
			break;

		// JMP IDX
		case 0x6E:
			break;

		// JMP EXT
		case 0x7E:
		{
			extended = arg1 & 0xFF;
			extended = extended << 8;
			extended = extended | (arg2 & 0xFF);
			next = extended;
			push(next);
			break;
		}

		// BSR
		case 0x8D:
			next = curr + inst_len + arg1;
			push(next);
			next = curr + inst_len;
			push(next);
			break;

		// JSR DIR
		case 0x9D:
			next = ((int16_t)arg1) & 0xFF;
			push(next);
			next = curr + inst_len;
			push(next);
			break;

		// JSR EXT
		case 0xBD:
		{
			extended = arg1 & 0xFF;
			extended = extended << 8;
			extended = extended | (arg2 & 0xFF);
			next = extended;
			push(next);
			next = curr + inst_len;
			push(next);
			break;
		}

		// JSR IND
		// TODO: Implement
		case 0xAD:
		
		default:
			if(inst_len == 0) break;
			next = curr + inst_len;
			push(next);
			break;
	}
}


int main(int argc, char** argv)
{
	//Assert proper number of program arguments
	if(argc < 3)
	{
		printf("Usage: <input file> <output file> <additional entry points file (optional)>\n");
		exit(1);
	}
	if(argc < 4)
	{
		printf("You may also provide a file with additional entry points to examine.\n");
	}
	
	//Load length table
	FILE* fp_length = fopen(LENGTH_TABLE_FILE, "r");
	if(fp_length == NULL)
	{
		printf("Failed to open length table file, exiting...\n");
		exit(1);
	}

	uint8_t* len_table = malloc(256 * sizeof(uint8_t));
	size_t num_read_LT = fread(len_table, sizeof(uint8_t), 256, fp_length);
	if(num_read_LT != 256)
	{
		printf("Failed to read length table file properly, exiting...\n");
		exit(1);
	}
	fclose(fp_length);

	//Load name table
	FILE* fp_name = fopen(NAME_TABLE_FILE, "r");
	if(fp_name == NULL)
	{
		printf("Failed to open name table file, exiting...\n");
		exit(1);
	}

	fseek(fp_name, 0L, SEEK_END);
	int name_len = ftell(fp_name);
	fseek(fp_name, 0L, SEEK_SET);

	uint8_t* name_table = malloc(name_len * sizeof(uint8_t));
	size_t num_read_NT = fread(name_table, sizeof(uint8_t), name_len, fp_name);
	if(num_read_NT != name_len)
	{
		printf("Failed to read name table file properly, exiting...\n");
		exit(1);
	}
	fclose(fp_name);

	//Load ROM
	FILE* fp_in = fopen(argv[1], "r");
	if(fp_in == NULL)
	{
		printf("Failed to open input file, exiting...\n");
		exit(1);
	}

	//Get size of ROM
	fseek(fp_in, 0L, SEEK_END);
	int rom_len = ftell(fp_in);
	// printf("Loading file of size %d bytes...\n", rom_len);
	fseek(fp_in, 0L, SEEK_SET);

	//Copy ROM into rom buffer, padded with 0s before
	uint8_t* rom = malloc(65536 * sizeof(uint8_t));	//the ROM is loaded at the end of the address space
	for(unsigned int d = 0; d < (65536 - rom_len); ++d)	//unused space is filed with 0x00, which is not a valid opcode.
	{
		rom[d] = 0x00;
	}
	size_t num_read = fread(rom + (65536 - rom_len), sizeof(uint8_t), rom_len, fp_in);
	if(num_read != rom_len)
	{
		printf("Failed to load input file properly, exiting...\n");
		exit(1);
	}
	fclose(fp_in);

	/////////
	//BEGIN//
	/////////
	char** program_strings = malloc(65536 * sizeof(char*));	//array of output strings. there can be no more than 65536 lines of code.
	for(unsigned int d = 0; d < 65536; ++d)
	{
		program_strings[d] = NULL;
	}
	uint16_t entry = rom[0xFFFF] | (rom[0xFFFE] << 8);	//program entry point

	CreateStack(32768);
	push(entry);

	if(argc > 3)
	{
		push_user(argv[3]);
	}

	while(!isEmpty())
	{
		uint16_t curr = pop();
		if(program_strings[curr])
		{
			continue;	//this line has already been disassembled
		}

		uint8_t opcode = rom[curr];
		char* opcode_name = get_opcode_name(opcode, name_table);

		char* inst_string = malloc(32 * sizeof(char));
		int inst_off = sprintf(inst_string, "0x%X %s", curr, opcode_name);

		uint8_t inst_length = get_opcode_length(opcode, len_table);
		int8_t arg1 = rom[curr + 1];
		int8_t arg2 = rom[curr + 2];

		if(inst_length > 2)
		{
			//Treat as unsigned to produce 16-bit value
			uint16_t extended = arg1 & 0xFF;
			extended = extended << 8;
			extended = extended | (arg2 & 0xFF);
			inst_off += sprintf(inst_string + inst_off, " %X", extended);
		}
		else if(inst_length > 1)
		{
			inst_off += sprintf(inst_string + inst_off, " %X", (arg1 & 0xFF));
		}

		program_strings[curr] = inst_string;
		push_next(opcode, inst_length, arg1, arg2, curr);
	}

	print_and_free(program_strings, argv[2]);
	free(program_strings);
	free(len_table);
	free(name_table);
	free(rom);
}
