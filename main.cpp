#include <iostream>
#include <vector>
#include <string>

#include "vd_vaapi.h"

#include <gst/gst.h>
using namespace std;

vector<int> run_indexs = {1};
vector<void (*)()> basic_tutorials;

string native_uri = "file:///home/vandy/Videos/TCL_H264_3840x2160_CABAC_30FPS_77.9Mbps_AAC_48kHz.mp4";
string net_uri = "https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm";
void basic_tutorial_1(){
    GstElement* pipline;
    GstBus* bus;
    GstMessage* msg;

    // init gstreamer first
    gst_init(NULL, NULL);

    // auto build pipeline by calling gst_parse_launch().
    pipline = gst_parse_launch(("playbin uri=" + native_uri).c_str(), NULL);
    gst_element_set_state(pipline, GST_STATE_PLAYING);

    // wait until playing done.
    bus = gst_element_get_bus(pipline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GstMessageType(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));

    // free resource
    if(msg){
        gst_message_unref(msg);
    }
    if(bus){
        gst_object_unref(bus);
    }
    if(pipline){
        gst_element_set_state(pipline, GST_STATE_NULL);
        gst_object_unref(pipline);
    }
}

void basic_tutorial_2(){
    GstElement* pipeline;
    GstElement* source;
    GstElement* sink;
    GstMessage* msg;
    GstBus* bus;

    gst_init(NULL, NULL);
    pipeline = gst_pipeline_new("pipeline");

    // create element from factory, the factory is inner factory. "videotestsrc", "autovideosink" already registied inside.
    // the element of videotestsrc type can provide many various test video, they're auto generated.
    source = gst_element_factory_make("videotestsrc", "source");
    sink = gst_element_factory_make("autovideosink", "sink");

    if(!pipeline || !source || !sink){
        g_printerr("failed to create elements!");
        return;
    }
    // add source and sink to pipeline
    gst_bin_add_many(GST_BIN_CAST(pipeline), source, sink, NULL);
    // link source and sink
    gboolean stat = gst_element_link(source, sink);
    if(stat != TRUE){
        g_printerr("gst_element_link failed!");
        gst_object_unref(pipeline);
        return ;
    }
    // set source property, customize source video behavior.
    // pattern=1, make the source test video is white noise.
    g_object_set(G_OBJECT(source), "pattern", 1, NULL);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GstMessageType(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
    if(msg){
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
}

typedef struct {
    GstElement* pipeline;
    GstElement* srcbin;
    GstElement* convert;
    GstElement* resample;
    GstElement* sink;

    GstBus* bus;
    GstMessage* msg;
} CustomData;
void pad_added_signal_handle(GstElement* srcbin, GstPad* new_pad, CustomData* data){
    GstPadLinkReturn state;
    GstPad* sink_pad = gst_element_get_static_pad(data->convert, "sink");
    
    // 需要获取到当前pad是哪里的pad, 是什么类型，只会将source pad链接到convert sink.
    GstCaps* caps = gst_pad_get_current_caps(new_pad);
    GstStructure* structure = gst_caps_get_structure(caps, 0);
    const gchar* new_pad_type = gst_structure_get_name(structure);
    if(!g_str_has_prefix(new_pad_type, "audio/x-raw")){
        g_print("%s has not audio/x-raw type!\n", new_pad_type);
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
// dynamic link pads
void basic_tutorial_3(){
    CustomData data;

    gst_init(NULL, NULL);
    // pipeline也是bin，一种特殊的Element.
    data.pipeline = gst_pipeline_new("audio-pipeline");

    // bin是一种特殊的Element, 可以容纳其他Element. uridecodebin这种类型的Element, 
    // 在创建的时候会自动创建source, demuxer, decoder三种Element, 所以它可以把设置进来的uri
    // 最终转换成裸码流的解码输出。在pipeline跑起来之前，里面Element的source pads是不存在的。
    data.srcbin = gst_element_factory_make("uridecodebin", "audio-srcbin");

    // 将输入的音频转成希望的format，此时输入是已经decode的结果。
    data.convert = gst_element_factory_make("audioconvert", "audio-convert");

    // 音频采样率转换
    data.resample = gst_element_factory_make("audioresample", "audio-resample");

    // 把结果送到音频设备的Element
    data.sink = gst_element_factory_make("autoaudiosink", "audio-sink");

    if(!data.pipeline || !data.srcbin || !data.convert || !data.resample || !data.sink){
        g_printerr("failed to create elements!");
        return ;
    }
    // 所有Element加入pipeline, 注意：参数必须以NULL结尾
    gst_bin_add_many(GST_BIN_CAST(data.pipeline), data.srcbin, data.convert, data.resample, data.sink, NULL);

    // 因为现在pipeline还没有跑起来，所以此时srcbin这种Element里面是没有source pads的，无法与别人链接。
    // 后面三个的Element会在链接是创建pads, 先全部链接起来。注意：参数必须以NULL结尾
    gboolean state = gst_element_link_many(data.convert, data.resample, data.sink, NULL);
    if(state != TRUE){
        g_printerr("gst_element_link failed!");
        gst_object_unref(data.pipeline);
        return;
    }
    // 当pipeline跑起来后， uridecodebin这种Element里面有数据产生了，会自动生成source pads。
    // 添加监听，当自动生成source pads时，把它链接到convert sink上。
    g_signal_connect(data.srcbin, "pad-added", G_CALLBACK(pad_added_signal_handle), &data);

    // 设置uri
    g_object_set(G_OBJECT(data.srcbin), "uri", native_uri.c_str());
    
    // set play status for pipeline
    gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

    data.bus = gst_element_get_bus(data.pipeline);
    
    gboolean stop = FALSE;
    // 循环监听总线上的消息
    while(!stop){
        data.msg = gst_bus_timed_pop_filtered(data.bus, GST_CLOCK_TIME_NONE, GstMessageType(GST_MESSAGE_EOS | GST_MESSAGE_ERROR | GST_MESSAGE_STATE_CHANGED));
        switch(GST_MESSAGE_TYPE(data.msg)){
            case GST_MESSAGE_EOS:
                g_print("end of file.");
                stop = TRUE;
                break;
            case GST_MESSAGE_ERROR:
                g_printerr("occur an error!");
                stop = TRUE;
                break;
            case GST_MESSAGE_STATE_CHANGED: // 总线上messagae状态有改变时
                // 消息来自pipeline
                if(GST_MESSAGE_SRC(data.msg) == GST_OBJECT(data.pipeline)){
                    GstState old_state, new_state, pending_state;
                    // 捕捉message状态
                    gst_message_parse_state_changed(data.msg, &old_state, &new_state, &pending_state);
                    g_print("message state changed from %s to %s, pending is: %s.\n", 
                            gst_element_state_get_name(old_state), gst_element_state_get_name(new_state), gst_element_state_get_name(pending_state));
                }
                break;
            default:
                g_printerr("unexcepted type!");
                stop = TRUE;
        }
    }

    // free resource
    if(data.msg){
        gst_message_unref(data.msg);
    }
    gst_object_unref(data.bus);
    
    gst_element_set_state(data.pipeline, GST_STATE_NULL);
    gst_object_unref(data.pipeline);
}

void basic_tutorial_4(){
    cout << "run 4" << endl;
}

void basic_tutorial_5(){

}

void basic_tutorial_6(){

}

void run_basic_tutorial(int i){
    cout << "====================== " << "start basic_tutorial_" << i << "() ======================" << endl;
    basic_tutorials[i-1]();
    cout << endl;
}

int main_tutorial(int argc, char *argv[]) {
    basic_tutorials = {
        basic_tutorial_1,
        basic_tutorial_2,
        basic_tutorial_3,
        basic_tutorial_4,
        basic_tutorial_5,
        basic_tutorial_6
    };
    vector<int>::iterator iter = run_indexs.begin();
    for(;iter != run_indexs.end();++iter){
        run_basic_tutorial(*iter);
    }
    return 0;
}

int main(int argc, char* argv[]){
    GError* err;
    gboolean state = gst_init_check(&argc, &argv, &err);
    if(state == FALSE){
        g_error_free(err);
        g_printerr("gst_init failed!\n");
        return -1;
    }
    if(argc <= 1){
        g_printerr("please input video file path!\n");
        return -1;
    }
    UserData gstva_data;
    GstElement* gstvaapi_pipeline = NULL;
    GstElement* libav_pipeline = NULL;

    if(argc == 2 || (argc > 2 && !strncmp(argv[2], "1", 1))){
        g_print("using gstreamer-vaapi plugin!\n");
        gstvaapi_pipeline = build_pipeline_with_gstvaapi(argv[1], gstva_data);
        if(!gstvaapi_pipeline){
            goto FAILED;
        }
    }
    if(argc > 2 && !strncmp(argv[2], "2", 1)){
        g_print("using gst-libav plugin!\n");
        libav_pipeline = build_pipeline_with_libav(argv[1], gstva_data);
        if(!gstvaapi_pipeline){
            goto FAILED;
        }
    }

FAILED:
    if(gstva_data.pipeline){
        gst_element_set_state(gstva_data.pipeline, GST_STATE_NULL);
    }
    GST_OBJECT_UNREF_SAFE(gstva_data.pipeline);
    if(libav_pipeline){
        gst_element_set_state(libav_pipeline, GST_STATE_NULL);
    }
    GST_OBJECT_UNREF_SAFE(libav_pipeline)
    return 0;
}