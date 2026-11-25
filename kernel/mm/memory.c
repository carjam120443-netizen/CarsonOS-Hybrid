#include <stdint.h>
#include "../include/memory.h"

struct limine_memmap_entry { uint64_t base, length, type; };
struct limine_memmap_response { uint64_t revision, entry_count; struct limine_memmap_entry **entries; };
struct limine_memmap_request { uint64_t id[4], revision; struct limine_memmap_response *response; };
struct limine_hhdm_response { uint64_t revision, offset; };
struct limine_hhdm_request { uint64_t id[4], revision; struct limine_hhdm_response *response; };

__attribute__((used, section(".limine_requests"), aligned(8)))
static volatile struct limine_memmap_request memmap_request = {
 .id={0xc7b1dd30df4c8b88,0x0a82e883a194f07b,0x67cf3d9d378a806f,0xe304acdfc50c3c62},.revision=0,.response=0
};
__attribute__((used, section(".limine_requests"), aligned(8)))
static volatile struct limine_hhdm_request hhdm_request = {
 .id={0xc7b1dd30df4c8b88,0x0a82e883a194f07b,0x48dcf1cb8ad2b852,0x63984e959a98244b},.revision=0,.response=0
};

static uint64_t usable_bytes, region_count, hhdm_offset;
#define MMIO_WINDOW 0xFFFFFF0000000000ull
#define PAGE_SIZE 4096ull
#define HUGE_SIZE 0x200000ull
#define MMIO_PML4_INDEX 510u
static uint64_t mmio_pdpt[512] __attribute__((aligned(4096)));
static uint64_t mmio_pd[512] __attribute__((aligned(4096)));
static uint64_t mapped_mmio_base;
static int mmio_mapped;

static uint64_t kernel_phys(const void *pointer)
{
 const uint64_t v=(uint64_t)(uintptr_t)pointer;
 if(v<0xFFFFFFFF80000000ull) return UINT64_MAX;
 const uint64_t p=0x00100000ull+(v-0xFFFFFFFF80000000ull);
 return p<0x100000000ull?p:UINT64_MAX;
}

void memory_init(void)
{
 usable_bytes=0; region_count=0; hhdm_offset=0;
 struct limine_memmap_response *r=memmap_request.response;
 if(r&&r->entries) for(uint64_t i=0;i<r->entry_count;++i){
  struct limine_memmap_entry *e=r->entries[i]; if(!e) continue;
  if(e->type==0){if(UINT64_MAX-usable_bytes>=e->length)usable_bytes+=e->length;++region_count;}
 }
 struct limine_hhdm_response *h=hhdm_request.response;
 if(h)hhdm_offset=h->offset;
}
uint64_t memory_usable_bytes(void){return usable_bytes;}
uint64_t memory_region_count(void){return region_count;}
uint64_t memory_hhdm_offset(void){return hhdm_offset;}

/*
 * Install one 2 MiB uncached identity-physical mapping in an otherwise-unused
 * PML4 slot. The dedicated virtual window is intentionally isolated from the
 * kernel image and Limine HHDM. Refuse to overwrite any existing mapping.
 */
void *memory_map_mmio(uint64_t physical, uint64_t length)
{
 if(!hhdm_offset||!length||physical>0xFFFFFFFFull) return 0;
 uint64_t base=physical&~(HUGE_SIZE-1);
 uint64_t offset=physical-base;
 if(length>HUGE_SIZE-offset) return 0;
 if(mmio_mapped) return mapped_mmio_base==base?(void *)(uintptr_t)(MMIO_WINDOW+offset):0;
 uint64_t cr3; __asm__ volatile("mov %%cr3,%0":"=r"(cr3));
 uint64_t pml4_phys=cr3&0x000FFFFFFFFFF000ull;
 if(pml4_phys>UINT64_MAX-hhdm_offset) return 0;
 volatile uint64_t *pml4=(volatile uint64_t *)(uintptr_t)(hhdm_offset+pml4_phys);
 if(pml4[MMIO_PML4_INDEX]&1u) return 0;
 uint64_t pdpt_phys=kernel_phys(mmio_pdpt),pd_phys=kernel_phys(mmio_pd);
 if(pdpt_phys==UINT64_MAX||pd_phys==UINT64_MAX||(pdpt_phys&4095u)||(pd_phys&4095u)) return 0;
 for(unsigned i=0;i<512;++i){mmio_pdpt[i]=0;mmio_pd[i]=0;}
 mmio_pdpt[0]=pd_phys|0x003u;
 mmio_pd[0]=base|0x09Bu; /* present, writable, PWT, PCD, 2 MiB page */
 __asm__ volatile("mfence":::"memory");
 pml4[MMIO_PML4_INDEX]=pdpt_phys|0x003u;
 __asm__ volatile("mfence":::"memory");
 __asm__ volatile("mov %%cr3,%%rax; mov %%rax,%%cr3":::"rax","memory");
 mapped_mmio_base=base; mmio_mapped=1;
 return (void *)(uintptr_t)(MMIO_WINDOW+offset);
}
