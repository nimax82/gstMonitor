#include <gst/gst.h>

static void handoff_callback(GstElement *identity, GstBuffer *buffer, GstPad *pad, gpointer user_data) {
    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (caps) {
        gchar *caps_str = gst_caps_to_string(caps);
        g_print("Caps: %s\n", caps_str);
        g_free(caps_str);
        gst_caps_unref(caps);
    }
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    GstElement *pipeline = gst_parse_launch(
        "udpsrc port=5057 ! application/x-rtp,encoding-name=H264,payload=96 ! "
        "rtph264depay ! h264parse ! identity name=identity_check ! avdec_h264 ! videoconvert ! autovideosink", nullptr);

    if (!pipeline) {
        g_printerr("Pipeline could not be created.\n");
        return -1;
    }


    // Get the identity element and connect the handoff callback
    GstElement *identity = gst_bin_get_by_name(GST_BIN(pipeline), "identity_check");
    if (identity) {
        g_signal_connect(identity, "handoff", G_CALLBACK(handoff_callback), nullptr);
        gst_object_unref(identity); // Release the reference to the identity element
    } else {
        g_printerr("Identity element not found in the pipeline.\n");
    }

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_print("Receiver pipeline running...\n");

    GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);

    
/*
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_print("Receiver pipeline running...\n");

    GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);*/

    return 0;
}

//gst-launch-1.0 udpsrc port=5057 ! application/x-rtp,encoding-name=H265,payload=96 ! rtph265depay ! h265parse ! avdec_h265 ! videoconvert ! autovideosink