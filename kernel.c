#include "versatilepb.h"
#include "asm.h"

#include <stddef.h>

#define STACK_SIZE 256 	/* Size of task stacks in words */
#define TASK_LIMIT 2 	/* Max number of tasks we can handle */

void *memcpy(void *dest, const void *src, size_t n)
{
	char *d = dest;
	const char *s = src;
	size_t i;

	for(i = 0; i < n; i++)
	{
		d[i] = s[i];
	}

	return d;
}

unsigned int *init_task (unsigned int *stack, void (*start)())
{
	stack += STACK_SIZE - 16; /*End stack minus what we're about to push*/
	stack[0] = 0x10; /* User mode, interrupts on */
	stack[1] = (unsigned int) start;
	return stack;
}

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
	
	if(!fork())
	{
		task();
	}

	bwputs("In user mode time 2\n");
	
	while(1)
	{
		syscall();
	}
}

int main()
{
	unsigned int stacks[TASK_LIMIT][STACK_SIZE];
	unsigned int *tasks[TASK_LIMIT];
	size_t task_count = 0;
	size_t current_task = 0;

	tasks[task_count] = init_task(stacks[task_count], &first);
	task_count++;

	while(1)
	{
		tasks[current_task] = activate(tasks[current_task]);

		switch(tasks[current_task][2+7])
		{
			case 0x1:
				if(task_count == TASK_LIMIT)
				{
					/* Unsuccessful at creating new task */
					tasks[current_task][2+0] = -1;
				}
				else
				{
					/* Check how much of stack is used */
					size_t used = stacks[current_task] + STACK_SIZE 
						- tasks[current_task];
					tasks[task_count] = stacks[task_count] + STACK_SIZE 
						- used;

					memcpy(tasks[task_count], tasks[current_task],
							used*sizeof(*tasks[current_task]));

					tasks[current_task][2+0] = task_count;
					tasks[task_count][2+0] = 0;

					task_count++;
				}
				break;
		}

		current_task++;

		if(current_task >= task_count)
		{
			current_task = 0;
		}
	}

	bwputs("Starting....\n");
	activate(tasks[0]);
	bwputs("Heading back to user mode\n");
	activate(tasks[1]);
	bwputs("Done.\n");

	while(1); /*because we can't exit the program*/

	return 0;
}
