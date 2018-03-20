FUZZER := 1

OBJS += fuzzer.o
OBJS += ../emulator/strl.o
OBJS += ../emulator/flash.o
OBJS += ../emulator/rng.o
OBJS += ../emulator/timer.o
OBJS += ../emulator/oled.o

libfuzzer.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

include ../Makefile.include
