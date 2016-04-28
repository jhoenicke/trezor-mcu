APPVER = 2.0.0

NAME  = trezor
OBJS += trezor.o
include ../Makefile.include

CFLAGS += -Wno-sequence-point
CFLAGS += -Iprotob -DPB_FIELD_16BIT=1
CFLAGS += -DQR_MAX_VERSION=0
CFLAGS += -DDEBUG_LINK=0
CFLAGS += -DDEBUG_LOG=0
CFLAGS += -DSCM_REVISION='"$(shell git rev-parse HEAD | sed 's:\(..\):\\x\1:g')"'
CFLAGS += -DED25519_CUSTOMRANDOM=1
CFLAGS += -DED25519_CUSTOMHASH=1

blob.h: ../bootloader/bootloader.bin
	perl mkblob.pl

trezor.o: blob.h
