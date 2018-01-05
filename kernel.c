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

void first(void)
{
	bwputs("In user mode\n");
	syscall();
	bwputs("In user mode time 2\n");
	syscall();
}

int main()
{
	unsigned int first_stack[256];
	unsigned int *first_stack_start = first_stack + 256 - 16;
	first_stack_start[0] = 0x10;
	first_stack_start[1] = (unsigned int)&first;

	bwputs("Starting....\n");
	first_stack_start = activate(first_stack_start);
	bwputs("Heading back to user mode\n");
	first_stack_start = activate(first_stack_start);
	bwputs("Done.\n");

	while(1); /*because we can't exit the program*/

	return 0;
}
