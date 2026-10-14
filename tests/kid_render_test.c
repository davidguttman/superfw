/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Run the actual kid-only menu layout with production fonts and bus16 writes. */
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include "../src/menu.c"
extern const uint32_t font_ascii_embedded[];
void *font_base_addr=(void*)font_ascii_embedded;
void dma_memcpy16(volatile void *dst,const void *src,uint16_t count) {
  volatile uint16_t *out=dst; const uint16_t *in=src;
  while(count--) *out++=*in++;
}
static void map(unsigned address,unsigned length) {
  assert(mmap((void*)(uintptr_t)address,length,PROT_READ|PROT_WRITE,
              MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=(void*)-1);
}
int main(void) {
  map(0x08000000,0x10000); map(0x04000000,0x1000); map(0x05000000,0x1000);
  struct { uint16_t before, frame[240*160/2], after; } screen;
  screen.before=screen.after=0xabcd;
  kid_sdr->library.count=1; kid_art_valid=true;
  strcpy(kid_sdr->library.entries[0].title,"WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW");
  for(unsigned i=0;i<KID_ART_WORDS;i++) kid_sdr->pixels[i]=0x1514;
  const char *notices[]={NULL,"Could not save. Ask a grown-up","Could not remember. Ask a grown-up",
    "Some games could not be read","Showing some of your games","Please ask a grown-up for help",
    "Could not open game save","Could not play that game","That game is unavailable"};
  for(unsigned n=0;n<sizeof(notices)/sizeof(*notices);n++) {
    kid_message=notices[n]; kid_render((uint8_t*)screen.frame);
    assert(screen.before==0xabcd && screen.after==0xabcd);
    for(unsigned y=0;y<160;y++) for(unsigned x=0;x<80;x++)
      assert(screen.frame[y*120+x]==0x1514); /* No text, arrows or border on art. */
    unsigned rows=notices[n] ? kid_text_rows(notices[n],72) : 0;
    assert(rows<=5); /* Every current notice is fully readable, not truncated. */
    unsigned hint_y=160-(rows+1)*16;
    uint16_t expected[240*160/2]; for(unsigned i=0;i<240*160/2;i++) expected[i]=0x1111;
    kid_text_box("A:Play",(uint8_t*)expected,164,hint_y,72,hint_y+16);
    if(notices[n]) kid_text_box(notices[n],(uint8_t*)expected,164,hint_y+16,72,160);
    for(unsigned y=hint_y;y<160;y++) for(unsigned x=80;x<120;x++)
      assert(screen.frame[y*120+x]==expected[y*120+x]); /* Title cannot overlap. */
    for(unsigned y=0;y<160;y++) {
      assert(screen.frame[y*120+80]==0x1111 && screen.frame[y*120+81]==0x1111);
      assert(screen.frame[y*120+118]==0x1111 && screen.frame[y*120+119]==0x1111);
    }
  }
  kid_art_valid=false; kid_render((uint8_t*)screen.frame);
  for(unsigned i=0;i<240*160/2;i++) assert((screen.frame[i]&255)<20 && (screen.frame[i]>>8)<20);
  kid_sdr->library.count=0;
  for(unsigned error=0;error<2;error++) {
    kid_sdr->library.io_error=error; kid_boot_pending_ok=!error;
    kid_render((uint8_t*)screen.frame);
    assert(screen.before==0xabcd && screen.after==0xabcd);
  }
  puts("kid production menu layout/art/notice/empty-library guards passed");
}
