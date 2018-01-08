#include <stddef.h>

unsigned int *activate(unsigned int *stack);

int fork();

int getpid();

int write(int fd, const void *buf, size_t count);
int read(int fd, void *buf, size_t count);
