/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef KID_STATE_H
#define KID_STATE_H
#include <stdbool.h>
#include "fatfs/ff.h"
/* Shared by firmware and the independently linked IGM. No library/VRAM data. */
#define KID_RECORD_PATH_BYTES 256
#define KID_LAST_FILE "/.superfw/kid-last.bin"
#define KID_SKIP_FILE "/.superfw/kid-skip-once"
#define KID_ATTEMPT_FILE "/.superfw/kid-attempt"
enum { KID_STATE_OK, KID_STATE_MISSING, KID_STATE_INVALID, KID_STATE_ERROR };
unsigned kid_last_read(char path[KID_RECORD_PATH_BYTES]);
bool kid_last_write(const char *path);
/* Create a checked persistent guard, then stage a non-autostartable record
 * before any launch SRAM/ROM work. Promotion removes the guard only after a
 * complete write and successful close; any failure leaves history ineligible. */
bool kid_last_stage(const char *path);
bool kid_last_clear(void);
/* A present/unreadable guard suppresses this boot. Invalidate last history
 * BEFORE removing the guard, retaining it if either cleanup step fails. */
unsigned kid_attempt_consume(bool *bypass);
bool kid_skip_write(void);
/* Any present, stale, malformed, or unreadable marker suppresses this boot.
 * Only a successfully removed marker permits autostart on a later boot. */
unsigned kid_skip_consume(bool *bypass);
bool kid_remove_file(const char *path);
/* Never erase SRAM or remove its sentinel after a failed flush. */
bool kid_pending_ready(void);
#endif
