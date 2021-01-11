#include "vd_vaapi.h"

static void demuxer_pad_added_cb(GstElement* demuxer, GstPad* new_pad, UserData* user_data){
    GstPadLinkReturn state;
    GstPad* sink_pad = gst_element_get_static_pad(user_data->m_queue, "sink");

    // 过滤掉不符合的pad
    GstCaps* caps = gst_pad_get_current_caps(new_pad);
    GstStructure* structure = gst_caps_get_structure(caps, 0);
    const gchar* new_pad_type = gst_structure_get_name(structure);
    if(!g_str_has_prefix(new_pad_type, "video/x-h265")){
        g_print("%s has not video/x-h265 type!\n", new_pad_type);
        goto exit;
    }
    state = gst_pad_link(new_pad, sink_pad);
    if(GST_PAD_LINK_FAILED(state)){
        g_printerr("%s link %s, but, gst_pad link failed!\n", gst_pad_get_name(new_pad), gst_pad_get_name(sink_pad));
    }else{
        g_print("%s link %s success!\n", gst_pad_get_name(new_pad), gst_pad_get_name(sink_pad));
    }
exit:
    if(caps){
        gst_caps_unref(caps);
    }
    gst_object_unref(sink_pad);
}

GstElement* build_pipeline_with_gstvaapi(const char* filename, UserData& user_data){
    // TODO: what if filename is not h265 ?
    GstElementFactory* filesrc_fac = gst_element_factory_find("filesrc");
    GstElementFactory* matroska_fac = gst_element_factory_find("matroskademux");
    GstElementFactory* queue_fac = gst_element_factory_find("queue");
    GstElementFactory* parser_fac = gst_element_factory_find("h265parse");
    GstElementFactory* vaapi_fac = gst_element_factory_find("vaapih265dec");
    GstElementFactory* vasink_fac = gst_element_factory_find("vaapisink");
    if(!filesrc_fac || !matroska_fac || !queue_fac || !parser_fac || !vaapi_fac || !vasink_fac){
        g_printerr("failed to create some factory!\n");
        return NULL;
    }
    user_data.m_srcfile = gst_element_factory_create(filesrc_fac, "video-src");
    user_data.m_demuxer = gst_element_factory_create(matroska_fac, "video-demuxer");
    user_data.m_queue = gst_element_factory_create(queue_fac, "video-queue");
    user_data.m_parser = gst_element_factory_create(parser_fac, "video-parser");
    user_data.m_decoder = gst_element_factory_create(vaapi_fac, "video-decoder");
    user_data.m_display = gst_element_factory_create(vasink_fac, "video-disp");
    // todo. what if failed to create element ?

    user_data.pipeline = gst_pipeline_new("video-vaapi-pipeline");
    gboolean state = FALSE;
    GstPadLinkReturn pad_link_ret;
    gst_bin_add_many(GST_BIN_CAST(user_data.pipeline), 
                            user_data.m_srcfile, 
                            user_data.m_demuxer, 
                            user_data.m_queue, 
                            user_data.m_parser, 
                            user_data.m_decoder, 
                            user_data.m_display, NULL);

    state = gst_element_link(user_data.m_srcfile, user_data.m_demuxer);
    if(state != TRUE){
        g_printerr("srcfile link to demuxer failed!\n");
        return NULL;
    }
    state = gst_element_link_many(user_data.m_queue, user_data.m_parser, user_data.m_decoder, user_data.m_display, NULL);
    if(state != TRUE){
        g_printerr("link element failed!\n");
        return NULL;
    }
    // matroskademux Availability: Sometimes, 所以在创建Element的时候, src pad是不存在的，需要添加监听，在src pad创建的时候连接到其他Element.
    g_signal_connect(GST_OBJECT_CAST(user_data.m_demuxer), "pad_added", G_CALLBACK(demuxer_pad_added_cb), &user_data);

    // 给filesrc element 设置location属性
    g_object_set(GST_OBJECT_CAST(user_data.m_srcfile), "location", filename);

    gst_element_set_state(user_data.pipeline, GST_STATE_PLAYING);
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE_CAST(user_data.pipeline));
    if(!bus){
        g_printerr("gst_pipeline_get_bus failed!");
        return NULL;
    }
    GstMessage* msg = NULL;
    bool stop = false;
    while(!stop){
        msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GstMessageType(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
        switch(msg->type){
            case GST_MESSAGE_EOS:
                g_print("stream reaches eos!\n");
                stop = true;
                break;
            case GST_MESSAGE_ERROR:
                g_printerr("occur an error!\n");
                stop = true;
                break;
            default:
                g_printerr("unexceptly type: %s\n", gst_message_type_get_name(msg->type));
                stop = true;
                break;
        }
    }
    if(msg){
        gst_message_unref(msg);
    }
    return user_data.pipeline;
}/*
(gdb) bt
#0  0x00007ffff6d57d80 in vaCreateSurfaces () at /lib/x86_64-linux-gnu/libva.so.2
#1  0x00007ffff720e043 in  () at /usr/lib/x86_64-linux-gnu/gstreamer-1.0/libgstvaapi.so
#2  0x00007ffff720e5f1 in  () at /usr/lib/x86_64-linux-gnu/gstreamer-1.0/libgstvaapi.so
#3  0x00007ffff71d8d37 in  () at /usr/lib/x86_64-linux-gnu/gstreamer-1.0/libgstvaapi.so
#4  0x00007ffff71da5d7 in  () at /usr/lib/x86_64-linux-gnu/gstreamer-1.0/libgstvaapi.so
#5  0x00007ffff71e4b90 in  () at /usr/lib/x86_64-linux-gnu/gstreamer-1.0/libgstvaapi.so
#6  0x00007ffff761147e in gst_base_sink_change_state (element=0x5555557f6cf0 [GstElement|video-disp], transition=GST_STATE_CHANGE_NULL_TO_READY) at gstbasesink.c:5161
#7  0x00007ffff7ec39f2 in gst_element_change_state (element=element@entry=0x5555557f6cf0 [GstElement|video-disp], transition=transition@entry=GST_STATE_CHANGE_NULL_TO_READY)
    at gstelement.c:2952
#8  0x00007ffff7ec4139 in gst_element_set_state_func (element=0x5555557f6cf0 [GstElement|video-disp], state=GST_STATE_READY) at gstelement.c:2906
#9  0x00007ffff7ea01b8 in gst_bin_element_set_state
--Type <RET> for more, q to quit, c to continue without paging--
    (next=GST_STATE_READY, current=GST_STATE_NULL, start_time=0 [0:00:00.000000000], base_time=0 [0:00:00.000000000], element=0x5555557f6cf0 [GstElement|video-disp], bin=0x5555558041e0 [GstBin|video-vaapi-pipeline]) at gstbin.c:2605
#10 gst_bin_change_state_func (element=0x5555558041e0 [GstElement|video-vaapi-pipeline], transition=GST_STATE_CHANGE_NULL_TO_READY) at gstbin.c:2947
#11 0x00007ffff7ec39f2 in gst_element_change_state (element=element@entry=0x5555558041e0 [GstElement|video-vaapi-pipeline], transition=transition@entry=GST_STATE_CHANGE_NULL_TO_READY)
    at gstelement.c:2952
#12 0x00007ffff7ec4139 in gst_element_set_state_func (element=0x5555558041e0 [GstElement|video-vaapi-pipeline], state=GST_STATE_PLAYING) at gstelement.c:2906
#13 0x000055555555c2f6 in build_pipeline_with_gstvaapi(char const*, UserData&)
    (filename=0x7fffffffe0fc "/home/vandy/Videos/HEVC_bbb-1920x1080-cfg02_Video2248.5kbps_60FPS_Audio238.58kbps_aac_6channels.mkv", user_data=...)
    at /home/vandy/code/c++/gstreamer/src/vd_vaapi.cpp:75
#14 0x000055555555a302 in main(int, char**) (argc=2, argv=0x7fffffffdd18) at /home/vandy/code/c++/gstreamer/main.cpp:262
*/




/****************************************************************************************/

/**
 * NOTE: 
 * 1. 简单起见，不考虑异常情况。
 * 2. gst_element_factory_make()即为gst_element_factory_find()和gst_element_factory_create()的结合。
 */
GstElement* build_pipeline_with_libav(const char* filename, UserData& user_data){
    user_data.m_srcfile = gst_element_factory_make("filesrc", "video-src");
    user_data.m_demuxer = gst_element_factory_make("matroskademux", "video-demuxer");
    user_data.m_queue = gst_element_factory_make("queue", "video-queue");
    user_data.m_parser = gst_element_factory_make("h265parse", "video-parser");
    user_data.m_decoder = gst_element_factory_make("avdec_h265", "video-decoder");  // 使用libav插件中的Element
    user_data.m_display = gst_element_factory_make("vaapisink", "video-disp");

    user_data.pipeline = gst_pipeline_new("video-vaapi-pipeline");
    gboolean state = FALSE;
    GstPadLinkReturn pad_link_ret;
    gst_bin_add_many(GST_BIN_CAST(user_data.pipeline), 
                            user_data.m_srcfile, 
                            user_data.m_demuxer, 
                            user_data.m_queue, 
                            user_data.m_parser, 
                            user_data.m_decoder, 
                            user_data.m_display, NULL);

    state = gst_element_link(user_data.m_srcfile, user_data.m_demuxer);
    if(state != TRUE){
        g_printerr("srcfile link to demuxer failed!\n");
        return NULL;
    }
    state = gst_element_link_many(user_data.m_queue, user_data.m_parser, user_data.m_decoder, user_data.m_display, NULL);
    if(state != TRUE){
        g_printerr("link element failed!\n");
        return NULL;
    }
    // matroskademux Availability: Sometimes, 所以在创建Element的时候, src pad是不存在的，需要添加监听，在src pad创建的时候连接到其他Element.
    g_signal_connect(GST_OBJECT_CAST(user_data.m_demuxer), "pad_added", G_CALLBACK(demuxer_pad_added_cb), &user_data);

    // 给filesrc element 设置location属性
    g_object_set(GST_OBJECT_CAST(user_data.m_srcfile), "location", filename);

    gst_element_set_state(user_data.pipeline, GST_STATE_PLAYING);
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE_CAST(user_data.pipeline));
    if(!bus){
        g_printerr("gst_pipeline_get_bus failed!");
        return NULL;
    }
    GstMessage* msg = NULL;
    bool stop = false;
    while(!stop){
        msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GstMessageType(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
        switch(msg->type){
            case GST_MESSAGE_EOS:
                g_print("stream reaches eos!\n");
                stop = true;
                break;
            case GST_MESSAGE_ERROR:
                g_printerr("occur an error!\n");
                stop = true;
                break;
            default:
                g_printerr("unexceptly type: %s\n", gst_message_type_get_name(msg->type));
                stop = true;
                break;
        }
    }
    if(msg){
        gst_message_unref(msg);
    }
    return user_data.pipeline;
}/*
(gdb) bt
#0  0x00007ffff3fd4d80 in vaCreateSurfaces () at /lib/x86_64-linux-gnu/libva.so.2
#1  0x00007fffec02f043 in  () at /usr/lib/x86_64-linux-gnu/gstreamer-1.0/libgstvaapi.so
#2  0x00007fffec02f5f1 in  () at /usr/lib/x86_64-linux-gnu/gstreamer-1.0/libgstvaapi.so
#3  0x00007fffebff9d37 in  () at /usr/lib/x86_64-linux-gnu/gstreamer-1.0/libgstvaapi.so
#4  0x00007fffebffb5d7 in  () at /usr/lib/x86_64-linux-gnu/gstreamer-1.0/libgstvaapi.so
#5  0x00007fffec005b90 in  () at /usr/lib/x86_64-linux-gnu/gstreamer-1.0/libgstvaapi.so
#6  0x00007ffff761147e in gst_base_sink_change_state (element=0x555555890170 [GstElement|video-disp], transition=GST_STATE_CHANGE_NULL_TO_READY) at gstbasesink.c:5161
#7  0x00007ffff7ec39f2 in gst_element_change_state (element=element@entry=0x555555890170 [GstElement|video-disp], transition=transition@entry=GST_STATE_CHANGE_NULL_TO_READY)
    at gstelement.c:2952
#8  0x00007ffff7ec4139 in gst_element_set_state_func (element=0x555555890170 [GstElement|video-disp], state=GST_STATE_READY) at gstelement.c:2906
#9  0x00007ffff7ea01b8 in gst_bin_element_set_state
    (next=GST_STATE_READY, current=GST_STATE_NULL, start_time=0 [0:00:00.000000000], base_time=0 [0:00:00.000000000], element=0x555555890170 [GstElement|video-disp], bin=0x55555588a180 [Gs--Type <RET> for more, q to quit, c to continue without paging--
tBin|video-vaapi-pipeline]) at gstbin.c:2605
#10 gst_bin_change_state_func (element=0x55555588a180 [GstElement|video-vaapi-pipeline], transition=GST_STATE_CHANGE_NULL_TO_READY) at gstbin.c:2947
#11 0x00007ffff7ec39f2 in gst_element_change_state (element=element@entry=0x55555588a180 [GstElement|video-vaapi-pipeline], transition=transition@entry=GST_STATE_CHANGE_NULL_TO_READY)
    at gstelement.c:2952
#12 0x00007ffff7ec4139 in gst_element_set_state_func (element=0x55555588a180 [GstElement|video-vaapi-pipeline], state=GST_STATE_PLAYING) at gstelement.c:2906
#13 0x000055555555c612 in build_pipeline_with_libav(char const*, UserData&)
    (filename=0x7fffffffe0fa "/home/vandy/Videos/HEVC_bbb-1920x1080-cfg02_Video2248.5kbps_60FPS_Audio238.58kbps_aac_6channels.mkv", user_data=...)
    at /home/vandy/code/c++/gstreamer/src/vd_vaapi.cpp:146
#14 0x000055555555a363 in main(int, char**) (argc=3, argv=0x7fffffffdd18) at /home/vandy/code/c++/gstreamer/main.cpp:26
*/

void free_resources(UserData& user_data){

}
