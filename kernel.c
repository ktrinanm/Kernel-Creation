#include "versatilepb.h"
#include "asm.h"

void bwputs(char *s)
{
	while(*s)
	{
		while(*(UART0 + UARTFR) & UARTFR_TXFF);
		*UART0 = *s;
		s++;
	}
}

void task()
{
	bwputs("In another task\n");
	
	while(1) 
	{
		syscall();
	}
}

void first(void)
{
	bwputs("In user mode\n");
	syscall();
	bwputs("In user mode time 2\n");
	
	while(1)
	{
		syscall();
	}
}

int main()
{
	unsigned int stacks[2][256];
	unsigned int *tasks[2];

	tasks[0] = stacks[0] + 256 - 16;
	tasks[0][0] = 0x10;
	tasks[0][1] = (unsigned int)&first;

	tasks[1] = stacks[1] + 256 - 16;
	tasks[1][0] = 0x10;
	tasks[1][1] = (unsigned int)&task;

	bwputs("Starting....\n");
	activate(tasks[0]);
	bwputs("Heading back to user mode\n");
	activate(tasks[1]);
	bwputs("Done.\n");

	while(1); /*because we can't exit the program*/

	return 0;
}
