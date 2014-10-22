#ifndef __chck_cdl__
#define __chck_cdl__

void* chckDlLoad(const char *file, const char **outError);
void* chckDlLoadSymbol(void *handle, const char *name, const char **outError);
void chckDlUnload(void *handle);

#endif /* __chck_cdl__ */

/* vim: set ts=8 sw=3 tw=0 :*/
