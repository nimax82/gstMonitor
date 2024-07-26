#include <gst/gst.h>
//#include <gst/inference/gstinference.h>

static void on_inference(GstElement *element, GstBuffer *buffer, gpointer user_data) {
    // Perform any deep learning processing on the buffer
    g_print("Inference performed on buffer\n");
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    GstElement *pipeline = gst_parse_launch(
        "udpsrc port=5000 ! application/x-rtp,encoding-name=H264,payload=96 ! "
        "rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! video/x-raw,width=640,height=480 ! "
        "gstinference name=inference model=your_model_path ! autovideosink", nullptr);

    if (!pipeline) {
        g_printerr("Pipeline could not be created.\n");
        return -1;
    }

    GstElement *inference = gst_bin_get_by_name(GST_BIN(pipeline), "inference");
    if (inference) {
        g_signal_connect(inference, "new-inference", G_CALLBACK(on_inference), nullptr);
    } else {
        g_printerr("Inference element not found in the pipeline.\n");
    }

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_print("Receiver pipeline running...\n");

    GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);

    return 0;
}