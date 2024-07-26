#include <gst/gst.h>

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    GstElement *pipeline = gst_parse_launch(
        "udpsrc port=5000 ! application/x-rtp,encoding-name=H264,payload=96 ! "
        "rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! autovideosink", nullptr);

    if (!pipeline) {
        g_printerr("Pipeline could not be created.\n");
        return -1;
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