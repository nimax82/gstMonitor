#include <gst/gst.h>
#include <opencv2/opencv.hpp>

// Callback function to process each video frame
static GstPadProbeReturn on_frame_probe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data) {
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    buffer = gst_buffer_make_writable(buffer);
    
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        // Assuming the video frame is in RGB format with 3 channels
        cv::Mat frame(480, 640, CV_8UC3, (void*)map.data);
        
        // Process the frame using OpenCV
        cv::cvtColor(frame, frame, cv::COLOR_RGB2BGR);
        //cv::imshow("Received Video", frame);
        //cv::waitKey(1); // Needed to display the frame properly

        std::string filename = "currentframe.png";

        // Save the frame to the file
        bool success = cv::imwrite(filename, frame);

        // Check if the frame was saved successfully
        if (success) {
            std::cout << "Image saved successfully to " << filename << std::endl;
        } else {
            std::cout << "Failed to save the image to " << filename << std::endl;
        }
        
        gst_buffer_unmap(buffer, &map);
    }

    return GST_PAD_PROBE_OK;
}

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    GstElement *pipeline = gst_parse_launch(
        "udpsrc port=5000 ! application/x-rtp,encoding-name=H264,payload=96 ! "
        "rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! video/x-raw,format=RGB,width=640,height=480 ! appsink name=sink", nullptr);

    if (!pipeline) {
        g_printerr("Pipeline could not be created.\n");
        return -1;
    }

    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    if (!sink) {
        g_printerr("Appsink element not found in the pipeline.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    GstPad *sinkpad = gst_element_get_static_pad(sink, "sink");
    gst_pad_add_probe(sinkpad, GST_PAD_PROBE_TYPE_BUFFER, (GstPadProbeCallback)on_frame_probe, NULL, NULL);
    gst_object_unref(sinkpad);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    g_print("Receiver pipeline running...\n");

    GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
    g_main_loop_run(loop);

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);

    return 0;
}
