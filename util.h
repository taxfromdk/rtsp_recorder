#ifndef UTIL_H
#define UTIL_H

#define SUCCESS 0
#define FAIL 1




typedef struct _ApplicationBaseData {
    char* rtsp_url;
    char* outbox;
    int shutdown;
    int argc;
    char** argv;
} ApplicationBaseData;

char* getOutboxPath(char* s, char* outbox);
char* getTmpPath(char* s);
char* getBaseFileName(char* camera_id, unsigned long recording_id, char* type);
char* getSegmentString(char* bfn);
char* getPlaylistString(char* bfn);


#endif //UTIL_H