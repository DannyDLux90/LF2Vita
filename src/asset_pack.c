#include "asset_pack.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define LF2_PAK_PATH_LEN 96
#pragma pack(push,1)
typedef struct { char magic[8]; uint32_t version,count; } pak_hdr_t;
typedef struct { char path[LF2_PAK_PATH_LEN]; uint32_t offset,comp_size,raw_size,crc,method; } pak_entry_t;
#pragma pack(pop)
static FILE *g_fp=NULL;static pak_entry_t *g_entries=NULL;static int g_count=0;static long g_data_start=0;

static int find_entry(const char *path){if(!path)return -1;for(int i=0;i<g_count;i++)if(!strcmp(g_entries[i].path,path))return i;return -1;}
int lf2_pak_init(const char *path){
    if(g_fp)return 0;g_fp=fopen(path,"rb");if(!g_fp){lf2_logf("ERROR","pak open failed: %s",path);return -1;}
    pak_hdr_t h;if(fread(&h,1,sizeof(h),g_fp)!=sizeof(h)||memcmp(h.magic,"LF2PAK01",8)||h.version!=1||h.count==0||h.count>5000){lf2_logf("ERROR","pak header invalid");fclose(g_fp);g_fp=NULL;return -2;}
    g_count=(int)h.count;g_entries=(pak_entry_t*)calloc((size_t)g_count,sizeof(*g_entries));if(!g_entries){fclose(g_fp);g_fp=NULL;g_count=0;return -3;}
    if(fread(g_entries,sizeof(*g_entries),(size_t)g_count,g_fp)!=(size_t)g_count){free(g_entries);g_entries=NULL;fclose(g_fp);g_fp=NULL;g_count=0;return -4;}
    for(int i=0;i<g_count;i++)g_entries[i].path[LF2_PAK_PATH_LEN-1]=0;
    g_data_start=ftell(g_fp);lf2_logf("INFO","pak initialized path=%s entries=%d data_start=%ld",path,g_count,g_data_start);return 0;
}
void lf2_pak_shutdown(void){if(g_fp)fclose(g_fp);free(g_entries);g_fp=NULL;g_entries=NULL;g_count=0;g_data_start=0;}
int lf2_pak_count(void){return g_count;}
const char *lf2_pak_entry_name(int i){return i>=0&&i<g_count?g_entries[i].path:NULL;}
size_t lf2_pak_entry_size(int i){return i>=0&&i<g_count?g_entries[i].raw_size:0;}
uint32_t lf2_pak_entry_crc(int i){return i>=0&&i<g_count?g_entries[i].crc:0;}
int lf2_pak_read_index(int i,void **data_out,size_t *size_out){
    if(!g_fp||!data_out||i<0||i>=g_count)return -1;pak_entry_t *e=&g_entries[i];*data_out=NULL;if(size_out)*size_out=0;
    if(fseek(g_fp,g_data_start+(long)e->offset,SEEK_SET)!=0)return -2;
    unsigned char *out=(unsigned char*)malloc((size_t)e->raw_size+1);if(!out)return -3;
    if(e->method==0){if(fread(out,1,e->raw_size,g_fp)!=e->raw_size){free(out);return -4;}}
    else if(e->method==1){unsigned char *comp=(unsigned char*)malloc(e->comp_size);if(!comp){free(out);return -5;}if(fread(comp,1,e->comp_size,g_fp)!=e->comp_size){free(comp);free(out);return -6;}uLongf n=e->raw_size;int z=uncompress(out,&n,comp,e->comp_size);free(comp);if(z!=Z_OK||n!=e->raw_size){free(out);return -7;}}
    else {free(out);return -8;}out[e->raw_size]=0;*data_out=out;if(size_out)*size_out=e->raw_size;return 0;
}
int lf2_pak_read(const char *relpath,void **data_out,size_t *size_out){int i=find_entry(relpath);if(i<0){lf2_logf("ERROR","pak asset missing: %s",relpath?relpath:"(null)");return -10;}return lf2_pak_read_index(i,data_out,size_out);}
int lf2_pak_verify_index(int i,char *error,int error_cap){void *p=NULL;size_t n=0;int rc=lf2_pak_read_index(i,&p,&n);if(rc<0){if(error&&error_cap)snprintf(error,error_cap,"PAK Lesefehler %d: %s",rc,lf2_pak_entry_name(i));return rc;}uLong c=crc32(0L,Z_NULL,0);c=crc32(c,(const Bytef*)p,(uInt)n);free(p);if(n!=g_entries[i].raw_size||(uint32_t)c!=g_entries[i].crc){if(error&&error_cap)snprintf(error,error_cap,"CRC Fehler: %s",g_entries[i].path);return -20;}return 0;}
