/*
 * This file is part of the TREZOR project, https://trezor.io/
 *
 * Copyright (C) 2018 Jochen Hoenicke <hoenicke@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <libopencm3/stm32/flash.h>

#include "memory.h"
#include "oled.h"
#include "rng.h"
#include "setup.h"
#include "timer.h"
#include "buttons.h"

const uint8_t *fuzzer_ptr;
size_t fuzzer_length;

uint8_t flash[FLASH_TOTAL_SIZE];
uint8_t *emulator_flash_base = flash;

uint32_t __stack_chk_guard;

int fuzzer_checkend(void) {
	return fuzzer_length == 0;
}

const uint8_t *fuzzer_input(size_t len) {
	if (fuzzer_length < len) {
		fuzzer_length = 0;
		return NULL;
	}
	const uint8_t *result = fuzzer_ptr;
	fuzzer_length -= len;
	fuzzer_ptr += len;
	return result;
}

extern char usbTiny(char set);
extern char read_state;
extern uint16_t read_msg_id;
extern uint32_t read_msg_size;
extern uint32_t read_msg_pos;
enum {
        READSTATE_IDLE,
        READSTATE_READING,
};
extern void recovery_abort(void);
extern void signing_abort(void);
extern void ethereum_signing_abort(void);
void setup(void) {
	const uint8_t *input = fuzzer_input(32);
	memset(emulator_flash_base, -1, FLASH_TOTAL_SIZE);
	memset(emulator_flash_base+0x8100, 0, 0x3f00);
	if (input) {
		memcpy(emulator_flash_base + 0x8100, input, 32);
		//HACK: force NUL termination of strings in storage
		emulator_flash_base[0x9000] = 0;
	}
	read_state = READSTATE_IDLE;
	read_msg_id = 0xffff;
	read_msg_size = 0;
	read_msg_pos = 0;
	usbTiny(0);
        recovery_abort();
        signing_abort();
        ethereum_signing_abort();
}

void emulatorRandom(void *buffer, size_t size) {
	const uint8_t *rnd = fuzzer_input(size);
	static uint32_t counter = 0;
	if (rnd) {
		memcpy(buffer, rnd, size);
	} else {
		memcpy(buffer, &counter, size);
		counter++;
	}
}

void emulatorSocketInit(void) {
}

static uint8_t initialize_msg[64] = {
 '?', '#', '#', 0, 0, 0,0,0,0, 
};
size_t emulatorSocketRead(void *buffer, size_t size) {
	const uint8_t* input = fuzzer_input(1);
	uint8_t avail = input ? *input : 1;
	if (avail) {
		input = fuzzer_input(size);
		if (input) {
			memcpy(buffer, input, size);
			return size;
		} else {
			memcpy(buffer, initialize_msg, 64);
			return 64;
		}
	}
	return 0;
}

size_t emulatorSocketWrite(const void *buffer, size_t size) {
	(void) buffer;
	return size;
}

uint16_t buttonRead(void) {
	const uint8_t* input = fuzzer_input(1);
	static uint16_t lastbutton = 0;
	// if no more input, toggle right to okay everything	
	uint16_t state = input ? *input : (lastbutton ^= BTN_PIN_YES);
	return state;
}

extern int trezor_main(void);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	fuzzer_ptr = data;
	fuzzer_length = size;
	if (size < 64) {
		return 0;
	}
	trezor_main();
	return 0;
}
