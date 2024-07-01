/*
Copyright (c) 2024 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define RAM_PAGES 0x10

void system_reboot(char *rom, int soft);
void system_inspect(void);
int system_error(char *msg, const char *err);
int system_boot(Uint8 *ram, char *rom);

Uint8 system_dei(Uint8 addr);
void system_deo(Uint8 addr);

extern char *boot_rom;