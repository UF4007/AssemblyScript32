#include "AssemblyScript32.h"

void asc_testmain()
{
	asc::elf_t elf;
	elf.text.push_back({});
	asc::func_t& entry = elf.text[0];
	entry.EIP_Begin = 26;
	entry.assembly = { 
		0x48,0x65,0x6c,0x6c,0x6f,0x20,0x77,0x6f,0x72,0x6c,0x64,0x0a,0x00,0x00,	// Hello world!\n\0		{0}
		0x00,0x00,0x00,0x00,													// char* a14 = 0;		{14}
		0x00,0x00,0x00,0x00,													// int a18 = 0;			{18}
		0x00,0x00,0x00,0x00,													// bool a22 = 0;		{22}
		0xB8,0x00,0x00,0x00,18,0x00,0x00,0x00,									// MOV ESP, 18			{26}
		0xFF,0x00,0x00,0x00,1,0x00,0x00,0x00,									// CALLEXT 1
		0x40,0x00,0x03,0x00,0x00,0x00,0x00,0x00,								// INC [ESP]
		0xB8,0x00,0x00,0x00,14,0x00,0x00,0x00,									// MOV ESP, 14
		0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,								// CALLEXT 0
		0xB8,0x00,0x00,0x00,22,0x00,0x00,0x00,									// MOV ESP, 22
		0x39,0x00,0x03,0x00,0xfc,0xff,0xff,0xff,0xff,0x00,0x00,0x00,			// CMP [ESP - 4], 255
		0x74,0x00,0x00,0x00,26,0x00,0x00,0x00,									// JZ 26
		0x00,0x00,0x00,0x00,													// padding
	};
	elf();
	std::cout << "this" << std::endl;
}