#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> 
#include <unistd.h>
#include <gst/gst.h>
#include <gst/base/gstbaseparse.h>

#include "util.h"

time_t recording_id = 0;

//substitute for gst stop mechanism
int killrecording = 0;

//Global application information
ApplicationBaseData* pbasedata;

//Threads
pthread_t recorder_record_thread = (pthread_t)NULL;
pthread_t recorder_housekeeping_thread = (pthread_t)NULL;

enum State {idle=0, starting=1, recording=2, stopping=3};
enum State current_state = idle;
char* state_strings[] = {"idle", "starting", "recording", "stopping"};

//######################################################################
//#
//# Gstreamer structure
//#
//######################################################################
typedef struct _CustomData {
    GstElement *pipeline;

    GstElement *rtspsrc_small;
    GstElement *rtph264depay;
    GstElement *h264parse;
    GstElement *queue_video;

    GstElement *hlssink;

    GMainLoop *loop;
    GstBus *bus;
} CustomData;
CustomData data;


//######################################################################
//#
//# Bus callback
//#
//######################################################################

int bus_callback(GstBus *bus, GstMessage *msg, gpointer d) {
    GError *err;
    gchar *debug_info;

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR:
            gst_message_parse_error(msg, &err, &debug_info);
            g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
            g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
            g_clear_error(&err);
            g_free(debug_info);
            //g_main_loop_quit(data.loop);
            break;
        case GST_MESSAGE_EOS:
            g_print("End-Of-Stream reached.\n");
            
            if(gst_element_set_state(data.pipeline, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE) printf("ERROR A\r\n");
            
            gst_element_send_event(data.rtspsrc_small,    gst_event_new_flush_start ());


            break;
        case GST_MESSAGE_STATE_CHANGED:
            // We are only interested in state-changed messages from the pipeline 
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data.pipeline)) {
                GstState old_state, new_state, pending_state;
                gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                g_print("Pipeline state changed from %s to %s:\n",
                        gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
   
            } else {
                GstState old_state, new_state, pending_state;
                gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                g_print("Element %s state changed from %s to %s:\n", GST_OBJECT_NAME(GST_MESSAGE_SRC(msg)),
                        gst_element_state_get_name(old_state), gst_element_state_get_name(new_state));
            }

            break;
        case GST_MESSAGE_PROGRESS:
            {
                g_print("Progress notify\r\n");
                GstProgressType type;
                gchar *code, *text;
                gst_message_parse_progress (msg, &type, &code, &text);
                switch (type) {
                    case GST_PROGRESS_TYPE_START:
                        printf("GST_PROGRESS_TYPE_START\r\n");
                        break;
                    case GST_PROGRESS_TYPE_CONTINUE:
                        printf("GST_PROGRESS_TYPE_CONTINUE\r\n");
                        break;
                    case GST_PROGRESS_TYPE_COMPLETE:
                        printf("PROGRESS_TYPE_COMPLETE\r\n");
                        break;
                    case GST_PROGRESS_TYPE_CANCELED:
                        printf("GST_PROGRESS_TYPE_CANCELED\r\n");
                        break;
                    case GST_PROGRESS_TYPE_ERROR:
                        printf("GST_PROGRESS_TYPE_ERROR\r\n");
                        break;
                    default:
                        printf("DEFAULT\r\n");
                        break;
                }
                printf("Progress: (%s) %s\n", code, text);
                g_free (code);
                g_free (text);
            }
            break;
        case GST_MESSAGE_NEW_CLOCK:
            {
                GstClock *clock;
                gst_message_parse_new_clock (msg, &clock);
                printf("New clock: %s\n", (clock ? GST_OBJECT_NAME (clock) : "NULL"));
                
            }
            break;
        case GST_MESSAGE_STREAM_START:
            {
                guint group_id;
                gst_message_parse_group_id (msg, &group_id);    
                printf("Stream start group id: %d\n", group_id);
            }
            break;
        case GST_MESSAGE_ASYNC_DONE:
            {
                GstClockTime t;
                gst_message_parse_async_done(msg, &t);
                printf("Async done: %u ns\n", t);
            }
            break;
        case GST_MESSAGE_STREAM_STATUS:
            {
                //GstClockTime t;
                //GstElement *owner;
                //gst_message_parse_stream_status(msg, &t, &owner);
                printf("Stream status\n");
            }
            break;
        case GST_MESSAGE_LATENCY:
            {
                //GstClockTime t;
                //GstElement *owner;
                //gst_message_parse_stream_status(msg, &t, &owner);
                printf("Latency\n");
            }
            break;
        case GST_MESSAGE_ELEMENT:
            {
                static gint handled_cnt = 0;

                {
                    handled_cnt++;

                    const GstStructure *s = gst_message_get_structure (msg);
                    const gchar *name = gst_structure_get_name (s);
                    if (strcmp (name, "splitmuxsink-fragment-opened") == 0)
                    {
                        char* l;
                        time_t t;
                        gst_structure_get(s, 
                            "location", G_TYPE_STRING, &l, 
                            "running-time", GST_TYPE_CLOCK_TIME, &t,
                            NULL);
                    }
                    else if (strcmp (name, "splitmuxsink-fragment-closed") == 0)
                    {
                        //Read message parameters
                        char* l;
                        time_t t;
                        gst_structure_get(s, 
                            "location", G_TYPE_STRING, &l, 
                            "running-time", GST_TYPE_CLOCK_TIME, &t,
                            NULL);
                        printf("location: %s \r\n", l);
                        printf("running-time: %lu\r\n", t);
                        
                        //printf("msg->src->name: %s\r\n", msg->src->name);
                        //printf("msg->src->parent->name: %s\r\n", msg->src->parent->name);
                        //printf("msg->src->parent->name: %s\r\n", msg->src->parent->name);

                        //Fetch playlist file
                        char * playlist_location;
                        g_object_get (msg->src->parent, "playlist-location", &playlist_location, NULL);
                        printf("Playlist read from source is %s\r\n", playlist_location);
                        
                        //Figure out playlist path to store playlist in
                        char playlist_segment[256];
                        strcpy(playlist_segment, l);
                        strcpy(strstr(playlist_segment, ".ts"), ".m3u8");
                        
                        printf("playlist_location: %s\r\n", playlist_location);
                        printf("playlist_segment: %s\r\n", playlist_segment);

                        //Copy playlist
                        FILE* a = fopen(playlist_location, "r");
                        FILE* b = fopen(playlist_segment, "w");
                        char ch;
                        while( ( ch = fgetc(a) ) != EOF )
                            fputc(ch, b);
                        fclose(a);
                        fclose(b);
                        free(playlist_location);
                    }
                    /*g_message ("get message %d: %s", handled_cnt,
                            gst_structure_get_name (gst_message_get_structure(msg)));*/
                    
                }
            }
            break;
        
        default:
            // We should not reach here 
            g_printerr("Unexpected message received. Type: %s\n", GST_MESSAGE_TYPE_NAME(msg));
            
            break;
    }
    return TRUE;
}


//######################################################################
//#
//# Callback for rtsp sources to connect to pipeline
//#
//######################################################################


static void pad_added_handler(GstElement *src, GstPad *new_pad, CustomData* data) 
{
    GstPad *sink_pad;

    if(src == data->rtspsrc_small)
    {
        sink_pad = gst_element_get_static_pad(data->rtph264depay, "sink");
    }else
    {
        printf("Unknown pad");
    }

    if (gst_pad_is_linked (sink_pad)) 
    {
        g_print ("We are already linked. Ignoring.\n");
        goto exit;
    }

    g_print("Received new pad '%s' from '%s':\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));

    GstCaps *new_pad_caps = gst_pad_get_current_caps(new_pad);
    GstStructure *new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    const gchar *new_pad_type = gst_structure_get_name(new_pad_struct);
    
    if (g_str_has_prefix(new_pad_type, "application/x-rtp")) {
        GstPadLinkReturn ret = gst_pad_link (new_pad, sink_pad);
        if (GST_PAD_LINK_FAILED(ret)) {
            g_print("Type is '%s' but link failed.\n", new_pad_type);
        } else {
            g_print("Link succeeded (type '%s') from %s to %s.\n", new_pad_type, GST_PAD_NAME(new_pad), GST_PAD_NAME(sink_pad));
        }
        return;
    }    

exit:
    //Unreference the new pad's caps, if we got them
    if (new_pad_caps != NULL)
    {
        gst_caps_unref(new_pad_caps);
    }
    //Unreference the sink pad
    gst_object_unref(sink_pad); 
}


//######################################################################
//#
//# Housekeeping Thread
//#
//######################################################################
void* housekeepingThread(gpointer data)
{
    while(!pbasedata->shutdown)
    {
        printf("current: %s\r\n", state_strings[current_state]);
        //webserverWebsocketBroadcast(test, strlen(test));
        usleep(1000000);
    }
}


//######################################################################
//#
//# Recorder Thread
//#
//######################################################################
void *recorderThread(void *p)
{
    printf("RECORDER: Starting thread()\r\n");
    current_state = starting;

    //Pickup time and use for recording ID
    time ( &recording_id );
     

    //Gstreamer init
    guint major, minor, micro, nano;
    gst_version(&major, &minor, &micro, &nano);    
    printf("RECORDER: Gstreamer version %d %d %d %d\r\n", major, minor, micro, nano);
    gst_debug_set_default_threshold(3); //Debug logging messages, 3 = FIXME, 2 = WARNING, 1 = ERROR

    //Gstreamer components init
    memset(&data, 0, sizeof(CustomData));

    data.pipeline = gst_pipeline_new("pipeline");

    if (data.pipeline == NULL)
    {
        printf("Pipeline could not be created.");
    }
    else
    {
        //Create and add elements
        data.rtspsrc_small = gst_element_factory_make("rtspsrc", "rtspsrc_small");
        data.rtph264depay = gst_element_factory_make("rtph264depay", "rtph264depay");
        data.h264parse = gst_element_factory_make("h264parse", "h264parse");
        data.queue_video = gst_element_factory_make("queue", "queue_video");
        data.hlssink = gst_element_factory_make("hlssink2", "hlssink");
        if ( !data.rtspsrc_small || !data.rtph264depay || !data.h264parse || !data.queue_video || !data.hlssink)
        {
            g_printerr("Fatal -> Not all elements could be created.\n");
            pbasedata->shutdown = 1;
            return NULL;
        }
        gst_bin_add_many(GST_BIN(data.pipeline),
            data.rtspsrc_small,
            data.rtph264depay,
            data.h264parse,
            data.queue_video,
            data.hlssink, NULL);


        //Set parameters
        gst_base_parse_set_pts_interpolation ((GstBaseParse *)(data.h264parse),TRUE);

        //Create base pipeline
        char buffer[100];
        sprintf(buffer, "%s", pbasedata->base_rtsp_url);
        g_object_set(data.rtspsrc_small,
            "location", buffer,
            "ntp-sync", TRUE,
            "protocols", (1 << 2), //GST_RTSP_LOWER_TRANS_TCP
            "do-rtsp-keep-alive", FALSE,
            "debug", TRUE,
                NULL);

        g_object_set(data.hlssink,
            "max-files", 0,
            "playlist-length", 0,
            "target-duration", 15,
            NULL);
            
        // Listen to the bus 
        data.bus = gst_element_get_bus(data.pipeline);
        gst_bus_add_watch(data.bus, bus_callback, &data);
        
        //Setup callback to connect rtsp pads to other modules
        g_signal_connect(data.rtspsrc_small, "pad-added", G_CALLBACK(pad_added_handler), &(data));
        
        data.loop = g_main_loop_new(NULL, FALSE);    
        
        {
            //Create paths for hlssink
            char* base_filename = getBaseFileName(pbasedata->camera_id, recording_id, "S");    
            char* segment_string = getSegmentString(base_filename);
            char* segment_string_outbox = getOutboxPath(segment_string, pbasedata->outbox);
            
            char* playlist_string = getPlaylistString(base_filename);
            char* playlist_string_outbox = getTmpPath(playlist_string);

            //Set the paths
            g_object_set(data.hlssink,
                "location", segment_string_outbox,
                "playlist-location", playlist_string_outbox,
                NULL);

            printf("Starting recording \r\n%s\r\n%s\r\n", segment_string_outbox, playlist_string_outbox);

            //free strings
            free(base_filename);
            free(segment_string);
            free(segment_string_outbox);
            free(playlist_string);
            free(playlist_string_outbox);
        }    


        //Link pipeline
        gst_element_link_many(
            data.rtph264depay, 
            data.h264parse,
            data.queue_video,
            data.hlssink,
            NULL);    
        
        
        //gst_element_send_event(data.rtspsrc_small,    gst_event_new_flush_stop (TRUE));

        //Start pipeline
        GstPadLinkReturn ret;
        ret = gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            g_printerr("Cant set ready.\n");
            gst_object_unref(data.pipeline);
            pbasedata->shutdown = 1;
            return NULL;
        }
        
        current_state = recording;
        g_main_loop_run(data.loop);
        
        current_state = stopping;
        ret = gst_element_set_state (data.pipeline, GST_STATE_NULL);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            g_printerr("Cant set ready.\n");
            gst_object_unref(data.pipeline);
            pbasedata->shutdown = 1;
            return NULL;
        }
        gst_object_unref(data.pipeline);        
    }

    current_state = idle;
    printf("RECORDER: Ending thread\r\n");
    gst_deinit();
}

//######################################################################
//#
//# Start recording - called from UI logic
//#
//######################################################################

int recorderStart() 
{
    printf("###### RECORDER: Start\r\n");
    if(current_state == idle)
    {
        killrecording = 0;
        if(pthread_create(&recorder_record_thread, NULL, recorderThread, NULL)) 
        {
            fprintf(stderr, "Error creating uploader_thread\n");
            return FAIL;
        }
    }
    else
    {
        printf("recording already in progress.\r\n");
        return FAIL;
    }
    return SUCCESS;
}

//######################################################################
//#
//# Stop recording - called from UI logic
//#
//######################################################################

int recorderStop() {
    printf("###### RECORDER: Stop\r\n");
    if(current_state != idle)
    {
        printf("###### sending stop signals\r\n");
        gst_element_send_event(data.rtspsrc_small, gst_event_new_eos());    
        printf("1\r\n");
        //gst_element_send_event(data.rtspsrc_small,    gst_event_new_flush_stop (TRUE));
        //printf("2\r\n");
        //gst_element_unlink_many(data.rtph264depay, data.h264parse, data.queue_video, data.hlssink, NULL);    
        printf("3\r\n");
        g_main_loop_quit (data.loop);
        printf("4\r\n");
        return SUCCESS;
    }
    else
    {
        printf("###### we are already idle\r\n");
    }
    
    return FAIL;
}


//######################################################################
//#
//# Init / exit
//#
//######################################################################

int recorderInit(ApplicationBaseData* bd)
{
    gst_init(&pbasedata->argc, &pbasedata->argv);    
    

    printf("RECORDER Init()\r\n");
    pbasedata = bd;
    if(pthread_create(&recorder_housekeeping_thread, NULL, housekeepingThread, NULL)) 
    {
		fprintf(stderr, "Error creating uploader_thread\n");
		return FAIL;
	}
    return SUCCESS;
}


int recorderExit()
{
    printf("RECORDER Exit()\r\n");

    if(current_state != idle)
    {
        printf("RECORDER: Stopping recording in progress.\r\n");
        recorderStop();
    } 

    if(recorder_record_thread != NULL)
    {
        if(pthread_join(recorder_record_thread, NULL)) 
        {
            fprintf(stderr, "Error joining recorder thread\n");
            return FAIL;
        }
    }
    
    if(pthread_join(recorder_housekeeping_thread, NULL)) 
    {
		fprintf(stderr, "Error joining housekeeping thread\n");
		return FAIL;
	}     
    return SUCCESS;
}