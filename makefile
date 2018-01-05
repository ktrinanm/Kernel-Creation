CC=arm-linux-gnueabi-gcc
CFLAGS=-ansi -pedantic -Wall -Wextra -march=armv6 -msoft-float -fPIC -mapcs-frame -marm -fno-stack-protector

LD=arm-linux-gnueabi-ld
LDFLAGS=-N -Ttext=0x10000

.SUFFIXES: .o .elf
.o.elf:
	$(LD) $(LDFLAGS) -o $@ $^

.SUFFIXES: .s.o
.s.o:
	$(CC) $(CFLAGS) -o $@ -c $^

kernel.elf: context_switch.o bootstrap.o kernel.o
