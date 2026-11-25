#include <stdint.h>
#include "../../include/net.h"
#include "../../include/memory.h"
#define CTRL 0x0000
#define STATUS 0x0008
#define RCTL 0x0100
#define TCTL 0x0400
#define TIPG 0x0410
#define RDBAL 0x2800
#define RDBAH 0x2804
#define RDLEN 0x2808
#define RDH 0x2810
#define RDT 0x2818
#define TDBAL 0x3800
#define TDBAH 0x3804
#define TDLEN 0x3808
#define TDH 0x3810
#define TDT 0x3818
#define RAL 0x5400
#define RAH 0x5404
#define MTA 0x5200
#define VFTA 0x5600
#define RING_COUNT 8
#define BUFFER_SIZE 2048
#define KERNEL_VMA_BASE 0xFFFFFFFF80000000ull
#define KERNEL_LMA_BASE 0x00100000ull
struct rx_desc { uint64_t address; uint16_t length,checksum; uint8_t status,errors; uint16_t special; } __attribute__((packed));
struct tx_desc { uint64_t address; uint16_t length; uint8_t checksum_offset,command,status,checksum_start; uint16_t special; } __attribute__((packed));
static volatile uint32_t *regs;
static struct rx_desc rx_ring[RING_COUNT] __attribute__((aligned(16)));
static struct tx_desc tx_ring[RING_COUNT] __attribute__((aligned(16)));
static uint8_t rx_buffers[RING_COUNT][BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buffers[RING_COUNT][BUFFER_SIZE] __attribute__((aligned(16)));
static unsigned rx_head;
static unsigned tx_head;
static uint64_t rx_packets;
static int ready;
static uint64_t phys(const void *p){uint64_t v=(uint64_t)(uintptr_t)p;if(v<KERNEL_VMA_BASE)return UINT64_MAX;uint64_t a=KERNEL_LMA_BASE+(v-KERNEL_VMA_BASE);return a<0x100000000ull?a:UINT64_MAX;}
static void wr(uint32_t o,uint32_t v){regs[o/4]=v;__asm__ volatile("mfence":::"memory");}
static uint32_t rd(uint32_t o){uint32_t v=regs[o/4];__asm__ volatile("lfence":::"memory");return v;}
int e1000_init(const struct pci_device_info *d){
 ready=0;rx_packets=0;rx_head=0;tx_head=0;
 if(!d||d->vendor_id!=0x8086u||d->device_id!=0x100Eu||!memory_hhdm_offset())return 0;
 uint32_t bar=d->bar0;if((bar&1u)||(bar&6u)==4u)return 0;
 uint64_t mmio=(uint64_t)(bar&~0xFu);if(!mmio||UINT64_MAX-memory_hhdm_offset()<mmio)return 0;
 regs=(volatile uint32_t *)memory_map_mmio(mmio, 0x6000u);
 if(!regs)return 0;
 uint64_t rp=phys(rx_ring),tp=phys(tx_ring);if(rp==UINT64_MAX||tp==UINT64_MAX||(rp&15u)||(tp&15u))return 0;
 for(unsigned i=0;i<RING_COUNT;++i){uint64_t rb=phys(rx_buffers[i]),tb=phys(tx_buffers[i]);if(rb==UINT64_MAX||tb==UINT64_MAX)return 0;
  rx_ring[i].address=rb;rx_ring[i].length=0;rx_ring[i].checksum=0;rx_ring[i].status=0;rx_ring[i].errors=0;rx_ring[i].special=0;
  tx_ring[i].address=tb;tx_ring[i].length=0;tx_ring[i].checksum_offset=0;tx_ring[i].command=0;tx_ring[i].status=1;tx_ring[i].checksum_start=0;tx_ring[i].special=0;
 }
 if(!pci_enable_memory_bus_master(d))return 0;
 wr(CTRL,rd(CTRL)|(1u<<26));unsigned wait;for(wait=0;wait<1000000u;++wait){if(!(rd(CTRL)&(1u<<26)))break;__asm__ volatile("pause");}if(wait==1000000u)return 0;
 wr(0x00D8,0xFFFFFFFFu);wr(0x00D0,0);for(unsigned i=0;i<128;++i){wr(MTA+i*4u,0);wr(VFTA+i*4u,0);}
 uint32_t ral=rd(RAL),rah=rd(RAH);if((rah&0x80000000u)==0&&ral==0&&rah==0)return 0;
 wr(RDBAL,(uint32_t)rp);wr(RDBAH,(uint32_t)(rp>>32));wr(RDLEN,sizeof(rx_ring));wr(RDH,0);wr(RDT,RING_COUNT-1);
 wr(TDBAL,(uint32_t)tp);wr(TDBAH,(uint32_t)(tp>>32));wr(TDLEN,sizeof(tx_ring));wr(TDH,0);wr(TDT,0);
 wr(TIPG,0x0060200Au);wr(TCTL,0x0004010Au);wr(RCTL,0x04008002u);ready=1;return 1;
}
void e1000_poll(void){ /* Upper layers pull completed frames with e1000_receive_frame(). */ }
int e1000_send_frame(const void *data,uint16_t length){
 if(!ready||!data||length<14u||length>BUFFER_SIZE)return 0;
 volatile struct tx_desc *d=&tx_ring[tx_head];
 unsigned wait;
 for(wait=0;wait<1000000u;++wait){if(d->status&1u)break;__asm__ volatile("pause");}
 if(wait==1000000u)return 0;
 const uint8_t *src=(const uint8_t *)data;
 for(uint16_t i=0;i<length;++i)tx_buffers[tx_head][i]=src[i];
 d->length=length;d->command=0x0Bu;d->status=0;
 __asm__ volatile("mfence":::"memory");
 unsigned next=(tx_head+1u)%RING_COUNT;wr(TDT,next);tx_head=next;
 for(wait=0;wait<1000000u;++wait){if(d->status&1u)return 1;__asm__ volatile("pause");}
 return 0;
}
int e1000_receive_frame(void *buffer,uint16_t capacity,uint16_t *received){
 if(received)*received=0;
 if(!ready||!buffer||!received||!capacity)return 0;
 volatile struct rx_desc *d=&rx_ring[rx_head];
 if(!(d->status&1u))return 0;
 uint16_t length=d->length;
 if(length>=14u&&length<=BUFFER_SIZE&&length<=capacity){
  uint8_t *dst=(uint8_t *)buffer;
  for(uint16_t i=0;i<length;++i)dst[i]=rx_buffers[rx_head][i];
  *received=length;++rx_packets;
 }
 d->status=0;d->length=0;d->errors=0;
 __asm__ volatile("mfence":::"memory");
 wr(RDT,rx_head);rx_head=(rx_head+1u)%RING_COUNT;
 return *received!=0;
}
int e1000_is_ready(void){return ready;}
int e1000_link_up(void){return ready&&(rd(STATUS)&2u)!=0;}
uint64_t e1000_received_packets(void){return rx_packets;}
