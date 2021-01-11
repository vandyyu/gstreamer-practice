#ifndef _VD_VAAPI_HEADER
#define _VD_VAAPI_HEADER
#include <gst/gst.h>

#define GST_OBJECT_UNREF_SAFE(ptr) \
do { \
    if(ptr){ \
        gst_object_unref(ptr); \
    } \
}while(0); \

struct UserData{
    GstElement* m_srcfile;
    GstElement* m_demuxer;
    GstElement* m_queue;

    GstElement* m_parser;
    GstElement* m_decoder;
    GstElement* m_display;

    GstElement* pipeline;
    UserData(){
        memset(this, 0, sizeof(UserData));
    }
};

/**
 * using gstreamer-vaapi plugin.
 */
GstElement* build_pipeline_with_gstvaapi(const char* filename, UserData& user_data);



/**
 * using libav plugin.
 */
GstElement* build_pipeline_with_libav(const char* filename, UserData& user_data);


void free_resources(UserData& user_data);


#endif