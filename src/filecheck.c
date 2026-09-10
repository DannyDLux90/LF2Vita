#include "filecheck.h"
#include "asset_pack.h"
#include "log.h"
#include <stdio.h>

int lf2_verify_bundle(const char *pak_path,lf2_check_progress_cb cb,void *userdata,char *error,int error_cap){
    lf2_log_stage("filecheck:init","pak=%s",pak_path);
    int rc=lf2_pak_init(pak_path);if(rc<0){if(error&&error_cap)snprintf(error,error_cap,"Spieldatenpaket kann nicht geöffnet werden (%d).",rc);return rc;}
    int total=lf2_pak_count();lf2_logf("INFO","filecheck entries=%d",total);
    for(int i=0;i<total;i++){
        const char *name=lf2_pak_entry_name(i);rc=lf2_pak_verify_index(i,error,error_cap);
        if(cb)cb(i+1,total,name,rc==0,userdata);
        if(rc<0){lf2_log_stage("filecheck:error","index=%d file=%s rc=%d",i,name?name:"?",rc);return -100+i;}
        if((i%50)==0)lf2_logf("INFO","filecheck progress %d/%d %s",i+1,total,name?name:"");
    }
    lf2_log_stage("filecheck:ok","entries=%d",total);return 0;
}
