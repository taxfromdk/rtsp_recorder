#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//Convert filename to path in outbox
char* getOutboxPath(char* s, char* outbox)
{
    char* r = (char*)malloc(strlen(outbox)+strlen(s)+1);
    sprintf(r, "%s%s", outbox, s);
    return r;
}

char* getTmpPath(char* s)
{
    char* r = (char*)malloc(128);
    sprintf(r, "/tmp/%s", s);
    return r;
}

char* getBaseFileName(char* camera_id, unsigned long recording_id, char* type)
{
    char* r = (char*)malloc(128);
    snprintf(r, 128, "%s_%lu_%s", camera_id, recording_id, type);
    return r;
}

char* getSegmentString(char* bfn)
{
    char* r = (char*)malloc(128);
    sprintf(r, "%s_%%08d.ts", bfn);
    return r;
}


char* getPlaylistString(char* bfn)
{
    char* r = (char*)malloc(128);
    sprintf(r, "%s.m3u8", bfn);
    return r;
}
