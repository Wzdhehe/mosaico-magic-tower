// SPDX-License-Identifier: MIT
#include "host_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

bool game_host_save(void *context, const uint8_t *bytes, size_t size)
{
    const char *path=context;
    if (!path || !bytes) return false;
    const size_t length=strlen(path);
    char *temporary=malloc(length+sizeof(".tmp"));
    if (!temporary) return false;
    memcpy(temporary,path,length);
    memcpy(temporary+length,".tmp",sizeof(".tmp"));
    FILE *file=fopen(temporary,"wb");
    if (!file) { free(temporary); return false; }
    bool ok=fwrite(bytes,1,size,file)==size;
    if (fclose(file)!=0) ok=false;
    if (ok) {
#ifdef _WIN32
        ok=MoveFileExA(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
        ok=rename(temporary,path)==0;
#endif
    }
    if (!ok) (void)remove(temporary);
    free(temporary);
    return ok;
}

bool game_host_load(void *context, uint8_t *bytes, size_t size)
{
    if (!context || !bytes) return false;
    FILE *file=fopen(context,"rb");
    if (!file) return false;
    bool ok=fread(bytes,1,size,file)==size && fgetc(file)==EOF && !ferror(file);
    if (fclose(file)!=0) ok=false;
    return ok;
}
