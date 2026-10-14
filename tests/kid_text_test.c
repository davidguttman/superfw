/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "kid_mode.h"
#include "fonts/font_render.h"
extern const uint32_t font_ascii_embedded[];
#include "utf_util.h"
void *font_base_addr = (void*)font_ascii_embedded;

static void check_lines(const char *text, unsigned width) {
  char line[KID_TITLE_BYTES];
  unsigned iterations = 0;
  while (*text) {
    const char *next = kid_text_line(text, width, font_width_cap, line);
    assert(next > text && ++iterations <= 64);
    assert(font_width(line) <= width);
    for (unsigned i=0; line[i]; i+=utf8_chlen(line+i))
      assert(i+utf8_chlen(line+i) <= strlen(line));
    text = next;
  }
}
int main(void) {
  char line[KID_TITLE_BYTES]; const char *next;
  assert(font_width("WWW") > font_width("iii"));
  next = kid_text_line("Hello world", font_width("Hello"), font_width_cap, line);
  assert(!strcmp(line,"Hello") && !strcmp(next,"world"));
  next = kid_text_line("WWWW", font_width("WW"), font_width_cap, line);
  assert(!strcmp(line,"WW") && !strcmp(next,"WW"));
  next = kid_text_line("  Pok\xc3\xa9mon  Adventure  ", 72, font_width_cap, line);
  assert(!strcmp(line,"Pok\xc3\xa9mon") && !strcmp(next,"Adventure  "));
  next = kid_text_line("\xc3\xa9\xc3\xa9", font_width("\xc3\xa9"), font_width_cap, line);
  assert(!strcmp(line,"\xc3\xa9") && !strcmp(next,"\xc3\xa9"));
  next = kid_text_line("W", 0, font_width_cap, line);
  assert(!*line && !*next); /* Too-wide glyph consumed, never overdrawn. */
  next = kid_text_line("", 72, font_width_cap, line);
  assert(!*line && !*next);
  check_lines("WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW",72);
  check_lines("Pok\xc3\xa9mon \xe6\x97\xa5\xe6\x9c\xac LongUnbrokenAdventure",72);
  check_lines("   ",72);
  char oversized[130]; memset(oversized,'i',sizeof(oversized)-1); oversized[129]=0;
  next=kid_text_line(oversized,1000,font_width_cap,line);
  assert(strlen(line)==63 && next==oversized+63);
  memset(oversized,'i',62); memcpy(oversized+62,"\xc3\xa9XYZ",6);
  next=kid_text_line(oversized,1000,font_width_cap,line);
  assert(strlen(line)==62 && next==oversized+62); /* Buffer cap cannot split UTF-8. */
  /* Actual bus16 font rendering with guards: only the right panel may change. */
  struct { uint16_t before; uint16_t pixels[240*160/2]; uint16_t after; } screen;
  screen.before=screen.after=0xabcd;
  for (unsigned i=0;i<240*160/2;i++) screen.pixels[i]=0x1111;
  const char *text="WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW";
  unsigned y=0;
  while (*text && y+16<=160) {
    text=kid_text_line(text,72,font_width_cap,line);
    draw_text_idx8_bus16_range(line,(uint8_t*)screen.pixels+y*240+164,0,72,240,18);
    y+=16;
  }
  assert(screen.before==0xabcd && screen.after==0xabcd);
  unsigned changed=0;
  for (unsigned y=0;y<160;y++) for (unsigned x=0;x<240;x++) {
    unsigned value=((uint8_t*)screen.pixels)[y*240+x];
    if (x<164 || x>=236) assert(value==17);
    if (value!=17) changed++;
  }
  assert(changed);
  puts("kid variable-font UTF-8 wrapping and bus16 panel guards passed");
}
