#include <stdint.h>
#include "../../include/arch.h"
#include "../../include/pci.h"
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC
static uint32_t pci_read32(uint8_t b,uint8_t d,uint8_t f,uint8_t o){uint32_t a=0x80000000u|((uint32_t)b<<16)|((uint32_t)d<<11)|((uint32_t)f<<8)|((uint32_t)o&0xFCu);arch_outl(PCI_CONFIG_ADDRESS,a);return arch_inl(PCI_CONFIG_DATA);}
static void pci_write32(uint8_t b,uint8_t d,uint8_t f,uint8_t o,uint32_t v){uint32_t a=0x80000000u|((uint32_t)b<<16)|((uint32_t)d<<11)|((uint32_t)f<<8)|((uint32_t)o&0xFCu);arch_outl(PCI_CONFIG_ADDRESS,a);arch_outl(PCI_CONFIG_DATA,v);}
int pci_enable_memory_bus_master(const struct pci_device_info *d){if(!d)return 0;uint16_t c=(uint16_t)pci_read32(d->bus,d->device,d->function,4);c|=6u;pci_write32(d->bus,d->device,d->function,4,c);return (((uint16_t)pci_read32(d->bus,d->device,d->function,4))&6u)==6u;}
int pci_find_e1000(struct pci_device_info *out){
 if(!out)return 0;
 for(uint32_t b=0;b<256;++b)for(uint32_t d=0;d<32;++d){
  uint32_t id0=pci_read32((uint8_t)b,(uint8_t)d,0,0);if((uint16_t)id0==0xFFFFu)continue;
  uint32_t h=pci_read32((uint8_t)b,(uint8_t)d,0,0x0C);uint32_t nf=((h>>16)&0x80u)?8u:1u;
  for(uint32_t f=0;f<nf;++f){uint32_t id=f?pci_read32((uint8_t)b,(uint8_t)d,(uint8_t)f,0):id0;uint16_t v=(uint16_t)id,di=(uint16_t)(id>>16);if(v!=0x8086u||di!=0x100Eu)continue;
   uint32_t cl=pci_read32((uint8_t)b,(uint8_t)d,(uint8_t)f,8);out->bus=(uint8_t)b;out->device=(uint8_t)d;out->function=(uint8_t)f;out->vendor_id=v;out->device_id=di;out->class_code=(uint8_t)(cl>>24);out->subclass=(uint8_t)(cl>>16);out->bar0=pci_read32((uint8_t)b,(uint8_t)d,(uint8_t)f,0x10);out->bar1=pci_read32((uint8_t)b,(uint8_t)d,(uint8_t)f,0x14);return 1;
  }
 }
 return 0;
}
