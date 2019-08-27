#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>

#include <errno.h>
#include "recorder.h"
#include "util.h"

ApplicationBaseData basedata;

int main_running = 1;

void sigint_handler(int sig)
{
	printf("Interrupt\r\n");
    basedata.shutdown = 1;
}

int main(int argc, char *argv[]) 
{
    printf("RTSP Recorder\r\n");
    if(argc != 2)
    {
        printf("Usage:\r\n");
        printf("      ./rtsp_recorder [base_rtsp_url]\r\n");
        exit(1);
    }
    
    //Init basedata shared between all parts of application
    memset((void*)&basedata, 0, sizeof(ApplicationBaseData));
    
    //Store argc and argv so they can be passed on to recorder when init gstreamer
    basedata.argc = argc;
    basedata.argv = argv;

    basedata.rtsp_url = argv[1];
    basedata.outbox = "outbox/";
    basedata.shutdown = 0;

    printf("rtsp_url: %s\r\n", basedata.rtsp_url);
    
    int r = SUCCESS;
    
    if(recorderInit(&basedata) != SUCCESS)
    {
        printf("Error starting recorder module.");                
        r = FAIL;    
    }
    else
    {                
        //Main loop
        char ch = ' ';
        while (ch != 'q' && basedata.shutdown == 0) {
            ch = getchar();
            if (ch != 255) 
            {
                if(ch == '1')
                {
                    //recorderStart();
                }
                else
                {
                    if(ch == '2')
                    {
                        //recorderStop();
                    }
                    else
                    {
                        printf("key: %d\r\nq: exit\r\n1:start recording\r\n2:stop recording\r\n", ch);
                    }
                }
            }
        }
        //This signals all threads to shutdown
        basedata.shutdown = 1;
        
        recorderExit();
    }//End of recorder
    printf("Exiting %d\r\n", r);
    return r;
}
