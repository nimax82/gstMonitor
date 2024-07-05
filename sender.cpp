#include <gst/gst.h>

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    GstElement *pipeline = gst_parse_launch(
        "v4l2src device=/dev/video0 ! video/x-raw,width=640,height=480,framerate=5/1 ! "
        "videoconvert ! x264enc tune=zerolatency ! rtph264pay ! "
        "udpsink host=192.168.1.23 port=5000", nullptr);

    if (!pipeline) {
        g_printerr("Pipeline could not be created.\n");
        return -1;
    }

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_print("Sender pipeline running...\n");

    GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);

    return 0;
}