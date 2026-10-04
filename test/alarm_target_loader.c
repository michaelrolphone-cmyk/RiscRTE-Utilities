#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <assert.h>
#include "esp_elf.h"
#include "private/elf_platform.h"
extern bool esp_elf_validate_file(const uint8_t*,size_t);
static uint8_t *arena;static size_t cursor;static unsigned residue,blocks;
static struct{uint8_t *p;size_t n;} live[128];
void *esp_elf_malloc(uint32_t n,bool exec){(void)exec;assert(blocks<128);cursor=(cursor+63)&~63u;cursor+=64+residue;assert(cursor+n+64<8*1024*1024);uint8_t*p=arena+cursor;memset(p-32,0xa5,n+64);live[blocks++]=(typeof(live[0])){p,n};cursor+=n+64;return p;}
void esp_elf_free(void*p){(void)p;}
uintptr_t elf_remap_text(esp_elf_t*e,uintptr_t p){return p>=e->sec[0].addr&&p<e->sec[0].addr+e->sec[0].size?p+0x06000000u:p;}
int esp_elf_arch_flush(esp_elf_t*e){(void)e;return 0;}
uintptr_t elf_find_sym_default(const char*n){uint32_t h=0;while(*n)h=h*33+(uint8_t)*n++;return 0x40000000u+(h&0xffffcu);}
bool esp_elf_privileged_os_cpu_scope_owned_v1(void){return false;}
bool esp_elf_privileged_os_cpu_relocation_enter_v1(const void*m){return m!=NULL;}
bool esp_elf_privileged_os_cpu_relocation_leave_v1(const void*m){return m!=NULL;}
int main(int argc,char**argv){assert(argc>1);arena=mmap((void*)0x3c000000,8*1024*1024,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(arena!=MAP_FAILED);for(int f=1;f<argc;f++){FILE*in=fopen(argv[f],"rb");assert(in);fseek(in,0,SEEK_END);size_t len=ftell(in);rewind(in);uint8_t*bytes=malloc(len);assert(fread(bytes,1,len,in)==len);fclose(in);assert(esp_elf_validate_file(bytes,len));unsigned relocations=0;
for(residue=0;residue<32;residue+=4){cursor=blocks=0;esp_elf_t elf;assert(!esp_elf_init(&elf));assert(!esp_elf_relocate(&elf,bytes));elf32_hdr_t*h=(void*)bytes;elf32_shdr_t*s=(void*)(bytes+h->shoff);
/* Check each mapped section against independently relocated reference bytes. */
for(unsigned k=0;k<ELF_SECS;k++){if(!elf.sec[k].size)continue;uint8_t*want=calloc(1,elf.sec[k].size);if(k!=ELF_SEC_BSS)memcpy(want,bytes+elf.sec[k].offset,elf.sec[k].size);
for(unsigned j=0;j<h->shnum;j++)if(s[j].type==SHT_RELA){elf32_rela_t*r=(void*)(bytes+s[j].offset);elf32_sym_t*syms=(void*)(bytes+s[s[j].link].offset);char*str=(void*)(bytes+s[s[s[j].link].link].offset);for(unsigned l=0;l<s[j].size/sizeof(*r);l++){uint32_t at=r[l].offset;if(at<elf.sec[k].v_addr||at>=elf.sec[k].v_addr+elf.sec[k].size)continue;assert(at+4<=elf.sec[k].v_addr+elf.sec[k].size);unsigned kind=ELF_R_TYPE(r[l].info);uint32_t*w=(void*)(want+at-elf.sec[k].v_addr);if(kind==5){if(*w)*w=elf_remap_text(&elf,esp_elf_map_sym(&elf,*w));}else if(kind==4||kind==3){elf32_sym_t*sym=&syms[ELF_R_SYM(r[l].info)];uintptr_t val=kind==4&&sym->value?esp_elf_map_sym(&elf,sym->value):elf_find_sym_default(str+sym->name);*w=elf_remap_text(&elf,val);}else assert(kind==2);relocations++;}}
assert(!memcmp(want,(void*)(uintptr_t)elf.sec[k].addr,elf.sec[k].size));free(want);}
for(unsigned k=0;k<blocks;k++)for(unsigned i=0;i<32;i++){assert(live[k].p[-32+(int)i]==0xa5);assert(live[k].p[live[k].n+i]==0xa5);}esp_elf_deinit(&elf);}
printf("PASS %s: 8 alignments, %u relocation-site checks, exact sections and redzones intact\n",argv[f],relocations);free(bytes);}return 0;}
