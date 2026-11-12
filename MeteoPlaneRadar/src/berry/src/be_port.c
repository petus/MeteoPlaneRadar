/********************************************************************
** MeteoPlaneRadar's port of Berry: where its output goes. There is no
** console to read from and no file system for scripts, so everything
** else here answers "no".
********************************************************************/
#include "berry.h"
#include <stddef.h>

/* Provided by Script.cpp: print() of a card script goes to the serial log. */
void mpr_be_print(const char *buffer, size_t length);

BERRY_API void be_writebuffer(const char *buffer, size_t length)
{
    mpr_be_print(buffer, length);
}

BERRY_API char* be_readstring(char *buffer, size_t size)
{
    (void)buffer; (void)size;
    return NULL;
}

void* be_fopen(const char *filename, const char *modes)
{
    (void)filename; (void)modes;
    return NULL;
}

int be_fclose(void *hfile)
{
    (void)hfile;
    return 0;
}

size_t be_fwrite(void *hfile, const void *buffer, size_t length)
{
    (void)hfile; (void)buffer; (void)length;
    return 0;
}

size_t be_fread(void *hfile, void *buffer, size_t length)
{
    (void)hfile; (void)buffer; (void)length;
    return 0;
}

char* be_fgets(void *hfile, void *buffer, int size)
{
    (void)hfile; (void)buffer; (void)size;
    return NULL;
}

int be_fseek(void *hfile, long offset)
{
    (void)hfile; (void)offset;
    return 0;
}

long int be_ftell(void *hfile)
{
    (void)hfile;
    return 0;
}

long int be_fflush(void *hfile)
{
    (void)hfile;
    return 0;
}

size_t be_fsize(void *hfile)
{
    (void)hfile;
    return 0;
}
