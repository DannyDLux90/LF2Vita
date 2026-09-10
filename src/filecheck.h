#ifndef LF2_FILECHECK_H
#define LF2_FILECHECK_H
typedef void (*lf2_check_progress_cb)(int done,int total,const char *path,int ok,void *userdata);
int lf2_verify_bundle(const char *pak_path, lf2_check_progress_cb cb, void *userdata, char *error, int error_cap);
#endif
