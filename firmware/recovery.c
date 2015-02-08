/*
 * This file is part of the TREZOR project.
 *
 * Copyright (C) 2014 Pavol Rusnak <stick@satoshilabs.com>
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

#include "recovery.h"
#include "fsm.h"
#include "storage.h"
#include "layout2.h"
#include "protect.h"
#include "types.pb.h"
#include "messages.h"
#include "rng.h"
#include "bip39.h"
#include "usb.h"
#include "buttons.h"
#include "util.h"
#include "oled.h"

static uint32_t word_count;
static bool awaiting_word = false;
static bool enforce_wordlist;
static uint32_t word_pos;
static uint32_t windex;
static char words[24][12];

static void show_word(void) {
	const char * const *wl = mnemonic_wordlist();
	char desc[] = "Choose ##th word";
	char *number = desc + 7;
	int pos = word_pos + 1;
	if (pos < 10) {
		number[0] = ' ';
	} else {
		number[0] = '0' + pos / 10;
	}
	number[1] = '0' + pos % 10;
	if (pos == 1 || pos == 21) {
			number[2] = 's'; number[3] = 't';
	} else
	if (pos == 2 || pos == 22) {
		number[2] = 'n'; number[3] = 'd';
	} else
	if (pos == 3 || pos == 23) {
		number[2] = 'r'; number[3] = 'd';
	}
	layoutDialogSwipe(DIALOG_ICON_QUESTION, "prev", "next", NULL, desc, "of your mnemonic", "here: ", wl[windex], "Then press Enter.", NULL);
}

static void ask_word(void)
{
	WordRequest resp;
	memset(&resp, 0, sizeof(WordRequest));
	msg_write(MessageType_MessageType_WordRequest, &resp);
}

static void recovery_button(int numpressed, int delta) {
	if (numpressed < 5) {
		delay(5000000);
	} else if (numpressed < 15) {
		delay(2000000);
	} else if (numpressed < 30) {
		delay(1000000);
	} else if (numpressed < 60) {
		delay(500000);
	} else if (numpressed > 120) {
		delta *= 4;
	} else if (numpressed > 90) {
		delta *= 2;
	}

	const char * const *wl = mnemonic_wordlist();
	windex = (windex + delta) & 0x7FF;
	oledBox(20, 3*9, OLED_WIDTH-1, 4*9-1, false);
	oledDrawString(20, 3 * 9, wl[windex]);
	oledRefresh();
}

bool choose_words(void)
{
	const char * const *wl = mnemonic_wordlist();
	bool result = false;
#if DEBUG_LINK
	bool debug_decided = false;
#endif

	windex = 0;

	usbTiny(1);
	ask_word();
	show_word();

	for (;;) {
		usbPoll();

		// check buttons
		buttonUpdate();
		if (button.YesDown) {
			recovery_button(button.YesDown, 1);
		}
		if (button.NoDown) {
			recovery_button(button.NoDown, -1);
		}

		// check for Cancel / Initialize
		if (msg_tiny_id == MessageType_MessageType_Cancel || msg_tiny_id == MessageType_MessageType_Initialize) {
			if (msg_tiny_id == MessageType_MessageType_Initialize) {
				protectAbortedByInitialize = true;
			}
			msg_tiny_id = 0xFFFF;
			result = false;
			break;
		}

#if DEBUG_LINK
		if (msg_tiny_id == MessageType_MessageType_DebugLinkGetState) {
			msg_tiny_id = 0xFFFF;
			fsm_msgDebugLinkGetState((DebugLinkGetState *)msg_tiny);
		}
#endif

		if (msg_tiny_id == MessageType_MessageType_WordAck) {
			msg_tiny_id = 0xFFFF;
			strlcpy(words[word_pos], wl[windex], sizeof(words[word_pos]));
			word_pos++;
			if (word_pos < word_count) {
				show_word();
				ask_word();
			} else {
				uint32_t i;
				strlcpy(storage.mnemonic, words[0], sizeof(storage.mnemonic));
				for (i = 1; i < word_count; i++) {
					strlcat(storage.mnemonic, " ", sizeof(storage.mnemonic));
					strlcat(storage.mnemonic, words[i], sizeof(storage.mnemonic));
				}
				storage.has_mnemonic = true;
				storage_commit();
				fsm_sendSuccess("Device recovered");
				result = true;
				break;
			}
		}
	}
	usbTiny(0);

	return result;
}

void recovery_init(uint32_t _word_count, bool passphrase_protection, bool pin_protection, const char *language, const char *label, bool _enforce_wordlist)
{
	if (_word_count != 12 && _word_count != 18 && _word_count != 24) {
		fsm_sendFailure(FailureType_Failure_SyntaxError, "Invalid word count (has to be 12, 18 or 24 bits)");
		layoutHome();
		return;
	}

	word_count = _word_count;
	enforce_wordlist = _enforce_wordlist;

	if (pin_protection && !protectChangePin()) {
		fsm_sendFailure(FailureType_Failure_ActionCancelled, "PIN change failed");
		layoutHome();
		return;
	}

	storage.has_passphrase_protection = true;
	storage.passphrase_protection = passphrase_protection;
	storage_setLanguage(language);
	storage_setLabel(label);

	awaiting_word = true;
	word_pos = 0;
	choose_words();
}

void recovery_word(const char *word __attribute__((unused)))
{
	fsm_sendFailure(FailureType_Failure_UnexpectedMessage, "Not in Recovery mode");
	layoutHome();
	return;
}

void recovery_abort(void)
{
	if (awaiting_word) {
		layoutHome();
		awaiting_word = false;
	}
}

uint32_t recovery_get_word_pos(void)
{
	return word_pos;
}
