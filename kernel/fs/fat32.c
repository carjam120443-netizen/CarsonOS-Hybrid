#include <stdint.h>
#include "../include/ata.h"
#include "../include/fat32.h"
#define SS 512u
#define EOC 0x0FFFFFF8u
#define BAD 0x0FFFFFF7u
static uint8_t sec[SS],fb[SS];
static uint32_t vol,volsz,fatl,fatsz,data,root,clusters,spc;
static int mounted;
static const char *msg="not initialized";
static uint16_t r16(const uint8_t*p){return(uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t r32(const uint8_t*p){return(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int p2(uint32_t x){return x&&!(x&(x-1));}
static int readrel(uint32_t n,uint8_t*b){return n<volsz&&vol<=0xFFFFFFFFu-n&&ata_read_sector(vol+n,b);}
static int bpb(const uint8_t*b){
 if(r16(b+11)!=SS||!p2(b[13])||b[13]>128)return 0;
 uint32_t res=r16(b+14),nf=b[16],fs=r32(b+36),total=r32(b+32);if(!total)total=r16(b+19);
 if(!res||!nf||nf>2||!fs||!total||r16(b+42))return 0;
 uint32_t rt=r32(b+44);uint64_t first=(uint64_t)res+(uint64_t)nf*fs;
 if(first>=total||rt<2||rt>=0x0FFFFFF0u)return 0;
 uint32_t count=(uint32_t)((total-first)/b[13]);if(!count||count>0x0FFFFFF5u||rt>=count+2u)return 0;
 volsz=total;spc=b[13];fatsz=fs;fatl=res;data=(uint32_t)first;root=rt;clusters=count;return 1;
}
int fs_init(void){
 mounted=0;msg="primary-master ATA disk not detected";if(!ata_is_ready())return 0;
 if(!ata_read_sector(0,sec)){msg="cannot read disk sector zero";return 0;}
 uint32_t part=0;
 if(sec[510]==0x55&&sec[511]==0xAA)for(unsigned i=0;i<4;++i){const uint8_t*p=sec+446+i*16;uint8_t t=p[4];if(t==0x0B||t==0x0C||t==0x1B||t==0x1C){part=r32(p+8);if(part)break;}}
 vol=part;if(!ata_read_sector(vol,sec)){msg="cannot read volume boot sector";return 0;}
 if(!bpb(sec)){if(!part||!ata_read_sector(0,sec)||!bpb(sec)){msg="no supported FAT32 volume found";return 0;}vol=0;}
 if(vol>ata_sector_count()||volsz>ata_sector_count()-vol){msg="FAT32 volume exceeds disk capacity";return 0;}
 mounted=1;msg="FAT32 mounted read-only";return 1;
}
static uint32_t nextc(uint32_t c){uint32_t off=c*4u,si=off/SS;if(si>=fatsz||!readrel(fatl+si,fb))return 0;return r32(fb+off%SS)&0x0FFFFFFFu;}
static uint32_t clsec(uint32_t c){if(c<2||c>=clusters+2u)return 0xFFFFFFFFu;uint64_t x=(uint64_t)data+(uint64_t)(c-2u)*spc;return x>0xFFFFFFFFu?0xFFFFFFFFu:(uint32_t)x;}
static int shortname(const char*s,uint8_t out[11]){
 for(unsigned i=0;i<11;++i)out[i]=' ';
 unsigned base=0,ext=0;int dot=0,any=0;
 while(*s&&*s!='/'){char c=*s++;if(c=='.'){if(dot)return 0;dot=1;continue;}if(c>='a'&&c<='z')c=(char)(c-32);if(c<'!'||c>'~'||c=='"'||c=='*'||c=='+'||c==','||c==':'||c==';'||c=='<'||c=='='||c=='>'||c=='?'||c=='['||c=='\\'||c==']'||c=='|')return 0;if(!dot){if(base>=8)return 0;out[base++]=(uint8_t)c;}else{if(ext>=3)return 0;out[8+ext++]=(uint8_t)c;}any=1;}
 return any&&base;
}
struct ent{uint32_t cluster,size;uint8_t attr;};
static int lookup(uint32_t dir,const uint8_t name[11],struct ent*out){
 uint32_t c=dir;for(uint32_t chain=0;chain<clusters;++chain){uint32_t first=clsec(c);if(first==0xFFFFFFFFu)return 0;
  for(uint32_t s=0;s<spc;++s){if(!readrel(first+s,sec))return 0;for(unsigned o=0;o<SS;o+=32){const uint8_t*e=sec+o;if(!e[0])return 0;if(e[0]==0xE5||e[11]==0x0F||(e[11]&8))continue;unsigned k=0;while(k<11&&e[k]==name[k])++k;if(k==11){out->cluster=((uint32_t)r16(e+20)<<16)|r16(e+26);out->size=r32(e+28);out->attr=e[11];return 1;}}}
  uint32_t n=nextc(c);if(n>=EOC||n==BAD||n<2)return 0;c=n;
 }return 0;
}
int fs_read_file(const char*path,uint8_t*out,uint32_t cap,uint32_t*size_out){
 if(size_out)*size_out=0;if(!mounted||!path||!out||!cap)return 0;while(*path=='/')++path;
 uint32_t dir=root;struct ent e={0,0,0};
 while(*path){uint8_t name[11];if(!shortname(path,name))return 0;while(*path&&*path!='/')++path;int more=0;while(*path=='/'){++path;more=1;}if(!lookup(dir,name,&e))return 0;
  if(more){if(!(e.attr&0x10)||e.cluster<2)return 0;dir=e.cluster;}
  else{if((e.attr&0x10)||e.size>cap||(e.size&&e.cluster<2))return 0;uint32_t left=e.size,pos=0,c=e.cluster;
   for(uint32_t chain=0;left&&chain<clusters;++chain){uint32_t first=clsec(c);if(first==0xFFFFFFFFu)return 0;for(uint32_t s=0;s<spc&&left;++s){if(!readrel(first+s,sec))return 0;uint32_t n=left<SS?left:SS;for(uint32_t i=0;i<n;++i)out[pos+i]=sec[i];pos+=n;left-=n;}if(left){uint32_t n=nextc(c);if(n>=EOC||n==BAD||n<2)return 0;c=n;}}
   if(left)return 0;if(size_out)*size_out=e.size;return 1;}
 }return 0;
}
int fs_is_mounted(void){return mounted;}
uint32_t fs_volume_sectors(void){return mounted?volsz:0;}
const char*fs_status(void){return msg;}
