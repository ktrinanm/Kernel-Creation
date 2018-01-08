#include "versatilepb.h"
#include "asm.h"

#include <stddef.h>

#define STACK_SIZE 1024	/* Size of task stacks in words */
#define TASK_LIMIT 2 	/* Max number of tasks we can handle */

#define PIPE_BUF	512  /* Size of largest atomic pipe message */
#define PIPE_LIMIT	(TASK_LIMIT*5)

#define TASK_READY		0
#define TASK_WAIT_READ	1
#define TASK_WAIT_WRITE	2

struct pipe_ringbuffer
{
	int start;
	int end;
	char data[PIPE_BUF];
};

#define RB_PUSH(rb, size, v) do \
{ \
	(rb).data[(rb).end] = (v); \
	(rb).end++; \
	if((rb).end > size) \
	{ \
		(rb).end = 0; \
	} \
}while (0)

#define RB_POP(rb, size, v) do \
{ \
	(v) = (rb).data[(rb).start]; \
	(rb).start++; \
	if((rb).start > size) \
	{ \
		(rb).start = 0; \
	} \
}while(0)

#define RB_LEN(rb, size) \
	(((rb).end - (rb).start) + (((rb).end < (rb).start) ? size : 0))

#define PIPE_PUSH(pipe, v) 	RB_PUSH((pipe), PIPE_BUF, (v))
#define PIPE_POP(pipe, v) 	RB_POP((pipe), PIPE_BUF, (v))
#define PIPE_LEN(pipe)		(RB_LEN((pipe), PIPE_BUF))

void _read(unsigned int *task, unsigned int **tasks, size_t task_count, 
		struct pipe_ringbuffer *pipes);
void _write(unsigned int *task, unsigned int **tasks, size_t task_count,
		struct pipe_ringbuffer *pipes);

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
	while(1);
}

void first(void)
{
	bwputs("In user mode\n");
	
	if(!fork())
	{
		task();
	}

	bwputs("In user mode time 2\n");
	while(1);
}

int main()
{
	*(PIC + VIC_INTENABLE) = PIC_TIMER01;

	*TIMER0 = 1000000;
	*(TIMER0 + TIMER_CONTROL) = TIMER_EN | TIMER_PERIODIC | TIMER_32BIT 
		| TIMER_INTEN;

	unsigned int stacks[TASK_LIMIT][STACK_SIZE];
	unsigned int *tasks[TASK_LIMIT];
	size_t task_count = 0;
	size_t current_task = 0;
	struct pipe_ringbuffer pipes[PIPE_LIMIT];
	size_t i;

	for(i = 0; i < PIPE_LIMIT; i++)
	{
		pipes[i].start = pipes[i].end = 0;
	}

	tasks[task_count] = init_task(stacks[task_count], &first);
	task_count++;

	while(1)
	{
		tasks[current_task] = activate(tasks[current_task]);
		tasks[current_task][-1] = TASK_READY;

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
			case 0x2:  /*getpid */
				tasks[current_task][2+0] = current_task;
				break;
			case 0x3: /* write */
				_write(tasks[current_task], tasks, task_count, pipes);
				break;
			case 0x4: /* read */
				_read(tasks[current_task], tasks, task_count, pipes);
				break;
			case -4:  /* Timer 0 or 1 went off */
				if(*(TIMER0 + TIMER_MIS)) /* Timer 0 went off */
				{
					*(TIMER0 + TIMER_INTCLR) = 1; /* Clear Interrupt */
					bwputs("tick\n");
				}
				break;
		}

		/* Select next TASK_READY task */
		while(TASK_READY != tasks[current_task = 
				(current_task+1 >= task_count ? 0 : current_task+1)][-1]);
	}

	return 0;
}

void _read(unsigned int *task, unsigned int **tasks, size_t task_count, 
		struct pipe_ringbuffer *pipes)
{
	task[-1] = TASK_READY;

	/* If the fd is invalid, or trying to read too much */
	if(task[2+0] > PIPE_LIMIT || task[2+2] > PIPE_BUF)
	{
		task[2+0] = -1;
	}
	else
	{
		struct pipe_ringbuffer *pipe = &pipes[task[2+0]];
		if((size_t)PIPE_LEN(*pipe) < task[2+2])
		{
			/*Trying to read more than there is: block */
			task[-1] = TASK_WAIT_READ;
		}
		else
		{
			size_t i;
			char *buf = (char*)task[2+1];
			/* Copy data into buf */

			for(i = 0; i < task[2+2]; i++)
			{
				PIPE_POP(*pipe, buf[i]);
			}

			/* Unblock any waiting writes
			 * 	XXX: nondeterministic unblock order
			 */
			for(i = 0; i < task_count; i++)
			{
				if(tasks[i][-1] == TASK_WAIT_WRITE)
				{
					_write(tasks[i], tasks, task_count, pipes);
				}
			}
		}
	}
}
}

void _write(unsigned int *task, unsigned int **tasks, size_t task_count,
		struct pipe_ringbuffer *pipes)
{
	/*If the fd is invalid or the write would be non-atomic */
	if(task[2+0] > PIPE_LIMIT || task[2+2] > PIPE_BUF)
	{
		task[2+0] = -1
	}
	else
	{
		struct pipe_ringbuffer *pipe - &pipes[task[2+0]];

		if((size_t)PIPE_BUF - PIPE_LEN(*pipe) < task[2+2])
		{
			/* Trying to write more than we have space for : block */
			task[-1] = TASK_WAIT_WRITE;
		}
		else
		{
			size_t i;
			const char *buf = (const char*)task[2+1];
			
			/* Copy data into pipe */
			for(i = 0; i < task[2+2]; i++)
			{
				PIPE_PUSH(*pipe, buf[i]);
			}

			/* Unblock any waiting reads 
			 * 		XXX: nondeterministic unblock order
			 **/
			for( i = 0; i < task_count; i++)
			{
				if(tasks[i][-1] == TASK_WAIT_READ)
				{
					_read(tasks[i], tasks, task_count, pipes);
				}
			}
		}
	}
}
}
