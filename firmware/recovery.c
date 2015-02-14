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
#include "oled.h"
#include "usb.h"
#include "buttons.h"
#include "util.h"
#include "pinmatrix.h"

#define MAXLETTER 4
#define MAXCHOICES 16
#define MAXDISPLAY 4

static uint32_t askedletter[24*MAXLETTER/32];
static unsigned int numletters;
static int letter_pos;
static uint32_t word_count;
static bool awaiting_word = false;
static bool enforce_wordlist;
static char letters[24][MAXLETTER];

static void draw_progress_bar(void) {
	unsigned int progress = 
		(word_count * MAXLETTER - numletters) * (OLED_WIDTH - 4)
		/ (word_count * MAXLETTER);

	// progress layout
	oledFrame(0, OLED_HEIGHT - 8, OLED_WIDTH - 1, OLED_HEIGHT - 1);
	oledBox(2, OLED_HEIGHT - 6, 1 + progress, OLED_HEIGHT - 3, 1);
}

static void fake_letter(void) {
	const char* word = mnemonic_wordlist()[random_uniform(2048)];
	char letter = word[random_uniform(MAXLETTER)];

	layoutDialog(DIALOG_ICON_INFO, NULL, NULL, NULL, "Please enter", NULL, letter ? "the letter " : "an empty word", NULL, NULL, NULL);
	if (letter)
		oledDrawZoomedChar(20 + oledStringWidth("the letter "), 10, 2, letter);
	letter_pos = -1;
	draw_progress_bar();
	oledRefresh();

	WordRequest resp;
	memset(&resp, 0, sizeof(WordRequest));
	msg_write(MessageType_MessageType_WordRequest, &resp);
}

void next_word(void) {
	int rand = random_uniform(numletters);
	letter_pos = 0;
	while ((askedletter[letter_pos / 32] & (1<<(letter_pos & 31))) != 0)
		letter_pos++;
	while (rand > 0) {
		letter_pos++;
		while ((askedletter[letter_pos / 32] & (1<<(letter_pos & 31))) != 0)
			letter_pos++;
		rand--;
	}

	int word_pos = (letter_pos / MAXLETTER) + 1;
	layoutDialog(DIALOG_ICON_INFO, NULL, NULL, NULL, "Please enter", NULL, "word ", "of your mnemonic", NULL, NULL);
	oledDrawString(76, 2 * 9, "letter");
	if (word_pos >= 10) {
		oledDrawBitmap(45, 10, bmp_digits[word_pos / 10]);
		oledInvert(45, 10, 45+15, 10+15);
	}
	oledDrawBitmap(58, 10, bmp_digits[word_pos % 10]);
	oledInvert(58, 10, 58+15, 10+15);
	oledDrawBitmap(104, 10, bmp_digits[1 + (letter_pos % MAXLETTER)]);
	oledInvert(104, 10, 104+15, 10+15);

	draw_progress_bar();
	oledRefresh();

	askedletter[letter_pos / 32] |= (1<<(letter_pos & 31));
	numletters--;
	
	WordRequest resp;
	memset(&resp, 0, sizeof(WordRequest));
	msg_write(MessageType_MessageType_WordRequest, &resp);
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
	memset(askedletter, 0, sizeof(askedletter));
	memset(&letters, 1, sizeof(letters));
	numletters = word_count * MAXLETTER;
	next_word();
}

void layout_choices(const char**choices, unsigned int choice, 
					unsigned int numchoices) {
	unsigned int i;
	oledBox(20, 1*9+2, OLED_WIDTH-1, (MAXDISPLAY+1)*9+3-1, false);
	if (numchoices <= MAXDISPLAY) {
		for (i = 0; i < numchoices; i++) {
			oledDrawString(25, (i+1)*9 + 3, choices[i]);
			if (i == choice) {
				oledInvert(20, (i+1)*9 + 2, OLED_WIDTH-1, (i+2)*9 + 3 -1);
			}
		}
	} else {
		for (i = 0; i < MAXDISPLAY; i++) {
			oledDrawString(25, (i+1) * 9 + 3, choices[(choice+i) % numchoices]);
		}
		oledInvert(20, 1*9+2, OLED_WIDTH-1, 2*9+3-1);
	}
	oledRefresh();
}

static int check_word(const char *word, const char lttrs[MAXLETTER]) {
	int i;
	for (i = 0; i < MAXLETTER; i++) {
		if (lttrs[i] != 1 && lttrs[i] != word[i])
			return 0;
	}
	return 1;
}

const char* choose_word(unsigned int word_pos, 
						const char**choices, unsigned int numchoices) {

	char desc[] = "Choose ##th word";
	word_pos++;
	desc[7] = word_pos < 10 ? ' ' : ('0' + word_pos / 10);
	desc[8] = '0' + (word_pos % 10);
	if (word_pos == 1 || word_pos == 21)
		memcpy(&desc[9], "st", 2);
	else if (word_pos == 2 || word_pos == 22)
		memcpy(&desc[9], "nd", 2);
	else if (word_pos == 3 || word_pos == 23)
		memcpy(&desc[9], "rd", 2);
	
	layoutDialogSwipe(DIALOG_ICON_QUESTION, "down", "okay", NULL, desc, NULL, NULL, NULL, NULL, NULL);
	
	unsigned int choice = 0;
	layout_choices(choices, choice, numchoices);
	for (;;) {
		usbPoll();

		buttonUpdate();
		if (button.NoDown) {
			choice = (choice + 1) % numchoices;
			layout_choices(choices, choice, numchoices);
			delay(10000000);
		}
		
		if (button.YesUp) {
			return choices[choice];
		}
		
		if (msg_tiny_id == MessageType_MessageType_Cancel 
			|| msg_tiny_id == MessageType_MessageType_Initialize) {
			if (msg_tiny_id == MessageType_MessageType_Initialize) {
				protectAbortedByInitialize = true;
			}
			msg_tiny_id = 0xFFFF;
			return NULL;
		}

#if DEBUG_LINK
		if (msg_tiny_id == MessageType_MessageType_DebugLinkGetState) {
			msg_tiny_id = 0xFFFF;
			fsm_msgDebugLinkGetState((DebugLinkGetState *)msg_tiny);
		}
#endif
	}
}

void recovery_word(const char *word)
{
	const char * const *wl;
	int i;
	unsigned int word_pos, numchoices;
	if (!awaiting_word) {
		fsm_sendFailure(FailureType_Failure_UnexpectedMessage, "Not in Recovery mode");
		layoutHome();
		return;
	}

	// If we asked for a real letter, put it into the letters array.
	// Ignore the word if we asked for a fake letter.
	if (letter_pos >= 0) {
		word_pos = letter_pos / MAXLETTER;
		letters[word_pos][letter_pos % MAXLETTER] = word[0];

		// check number of choices remaining
		wl = mnemonic_wordlist();
		numchoices = 0;
		while (*wl) {
			if (check_word(*wl, letters[word_pos])) {
				numchoices++;
				if (numchoices > MAXCHOICES) {
					break;
				}
			}
			wl++;
		}
		
		if (numchoices == 0) {
			storage_reset();
			fsm_sendFailure(FailureType_Failure_SyntaxError, "Wrong letter");
			layoutHome();
			return;
		}

		if (numchoices <= MAXCHOICES) {
			/* word is unique enough. Don't ask for it again */
			unsigned int pos = word_pos * MAXLETTER;
			for (i = 0; i < MAXLETTER; i++) {
				if ((askedletter[pos / 32] & (1 << (pos & 31))) == 0) {
					numletters--;
					askedletter[pos / 32] |= (1 << (pos & 31));
				}
				pos++;
			}
		}

		/* assuming we ask for about 2 letters per word, this will
		 * introduce about 4 fake letters in total.
		 */
		if (random_uniform(word_count / 2) == 0) {
			fake_letter();
			return;
		}
	}
	
	if (numletters > 0) {
		next_word();
		return;
	}

	// We are done with asking, now let the user choose the words.
	const char* choices[MAXCHOICES];
	usbTiny(1);
	for (word_pos = 0; word_pos < word_count; word_pos++) {

		wl = mnemonic_wordlist();
		numchoices = 0;
		while (*wl) {
			if (check_word(*wl, letters[word_pos])) {
				choices[numchoices++] = *wl;
			}
			wl++;
		}

		word = choose_word(word_pos, choices, numchoices);
		if (word == NULL) {
			storage_reset();
			layoutHome();
			return;
		}
			
		if (word_pos == 0) {
			strlcpy(storage.mnemonic, word, sizeof(storage.mnemonic));
		} else {
			strlcat(storage.mnemonic, " ", sizeof(storage.mnemonic));
			strlcat(storage.mnemonic, word, sizeof(storage.mnemonic));
		}
	}
	usbTiny(0);
	storage.has_mnemonic = true;
	storage_commit();
	fsm_sendSuccess("Device recovered");
	layoutHome();
}

void recovery_abort(void)
{
	if (awaiting_word) {
		layoutHome();
		awaiting_word = false;
	}
}
