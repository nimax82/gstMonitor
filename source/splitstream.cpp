// Created by IT-JIM
// VIDEO1 : Send udp stream to appsink, display with cv::imshow()

#include <iostream>
#include <string>
#include <thread>

#include <atomic>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

//rename
#include <cstdio>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <opencv2/opencv.hpp>

//======================================================================================================================
/// A simple assertion function + macro
inline void myAssert(bool b, const std::string &s = "MYASSERT ERROR !") {
    if (!b)
        throw std::runtime_error(s);
}

#define MY_ASSERT(x) myAssert(x, "MYASSERT ERROR :" #x)

//======================================================================================================================
/// Check GStreamer error, exit on error
inline void checkErr(GError *err) {
    if (err) {
        std::cerr << "checkErr : " << err->message << std::endl;
        exit(0);
    }
}

//======================================================================================================================
/// Our global data, serious gstreamer apps should always have this !
struct GoblinData {
    GstElement *pipeline = nullptr;
    GstElement *sinkVideo = nullptr;
    /*GstElement *httpPipeline = nullptr;
    std::atomic<bool> enableHttpStream{false};*/
};


//======================================================================================================================
/// convert time to string
std::string timePointToString(const std::chrono::time_point<std::chrono::high_resolution_clock>& tp) {
    // Convert time_point to system time
    auto systemTime = std::chrono::system_clock::to_time_t(
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            tp - std::chrono::high_resolution_clock::now() + std::chrono::system_clock::now()
        )
    );

    // Convert system time to tm structure
    std::tm* tm = std::localtime(&systemTime);

    // Create a string stream to format the time
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

//======================================================================================================================
/// Process a single bus message, log messages, exit on error, return false on eof
static bool busProcessMsg(GstElement *pipeline, GstMessage *msg, const std::string &prefix) {
    using namespace std;

    GstMessageType mType = GST_MESSAGE_TYPE(msg);
    cout << "[" << prefix << "] : mType = " << mType << " ";
    switch (mType) {
        case (GST_MESSAGE_ERROR):
            // Parse error and exit program, hard exit
            GError *err;
            gchar *dbg;
            gst_message_parse_error(msg, &err, &dbg);
            cout << "ERR = " << err->message << " FROM " << GST_OBJECT_NAME(msg->src) << endl;
            cout << "DBG = " << dbg << endl;
            g_clear_error(&err);
            g_free(dbg);
            exit(1);
        case (GST_MESSAGE_EOS) :
            // Soft exit on EOS
            cout << " EOS !" << endl;
            return false;
        case (GST_MESSAGE_STATE_CHANGED):
            // Parse state change, print extra info for pipeline only
            cout << "State changed !" << endl;
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
                GstState sOld, sNew, sPenging;
                gst_message_parse_state_changed(msg, &sOld, &sNew, &sPenging);
                cout << "Pipeline changed from " << gst_element_state_get_name(sOld) << " to " <<
                     gst_element_state_get_name(sNew) << endl;
            }
            break;
        case (GST_MESSAGE_STEP_START):
            cout << "STEP START !" << endl;
            break;
        case (GST_MESSAGE_STREAM_STATUS):
            cout << "STREAM STATUS !" << endl;
            break;
        case (GST_MESSAGE_ELEMENT):
            cout << "MESSAGE ELEMENT !" << endl;
            break;

            // You can add more stuff here if you want

        default:
            cout << endl;
    }
    return true;
}

//======================================================================================================================
/// Run the message loop for one bus
void codeThreadBus(GstElement *pipeline, GoblinData &data, const std::string &prefix) {
    using namespace std;
    GstBus *bus = gst_element_get_bus(pipeline);

    int res;
    while (true) {
        GstMessage *msg = gst_bus_timed_pop(bus, GST_CLOCK_TIME_NONE);
        MY_ASSERT(msg);
        res = busProcessMsg(pipeline, msg, prefix);
        gst_message_unref(msg);
        if (!res)
            break;
    }
    gst_object_unref(bus);
    cout << "BUS THREAD FINISHED : " << prefix << endl;
}

//======================================================================================================================
/// Appsink process thread
void codeThreadProcessV(GoblinData &data) {
    using namespace std;

    // Variables for time management
    auto lastTime = std::chrono::high_resolution_clock::now();
    auto startTime = lastTime;
    int frameCount = 0;

    // Motion detector
    cv::Mat frame, gray, prevGray, diff;
    cv::VideoWriter videoWriter;
    int noMotionCounter = 0;
    const int noMotionThreshold = 10;
    bool isRecording = false;
    // Define the codec and create VideoWriter object
    int imW = 0;/*= static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH));*/
    int imH = 0;/*= static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT));*/
    int fps = 30; // or capture.get(cv::CAP_PROP_FPS);


    std::string videoPath = "/home/promax/Dev/playground/gstreamer_pl/resources/video/static/";
    std::string filename = "";

    for (;;) {
        // Exit on EOS
        if (gst_app_sink_is_eos(GST_APP_SINK(data.sinkVideo))) {
            cout << "EOS !" << endl;
            break;
        }

        // Pull the sample (synchronous, wait)
        GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(data.sinkVideo));
        if (sample == nullptr) {
            cout << "NO sample !" << endl;
            break;
        }

        // Get width and height from sample caps (NOT element caps)
        GstCaps *caps = gst_sample_get_caps(sample);
        MY_ASSERT(caps != nullptr);
        GstStructure *s = gst_caps_get_structure(caps, 0);
        //int imW, imH;
        MY_ASSERT(gst_structure_get_int(s, "width", &imW));
        MY_ASSERT(gst_structure_get_int(s, "height", &imH));
        //cout << "Sample: W = " << imW << ", H = " << imH << endl;

        // Increment frame count
        frameCount++;
        // Get the current time
        auto currentTime = std::chrono::high_resolution_clock::now();
        // // Calculate the duration since the last frame
        // std::chrono::duration<dou  ble> elapsedTime = currentTime - lastTime;
        // // Update lastTime to currentTime
        // lastTime = currentTime;
        // // Calculate the duration since the start of the program
        // std::chrono::duration<double> totalElapsedTime = currentTime - startTime;
        // // If one second has passed, print the frame count and reset
        // if (totalElapsedTime.count() >= 1.0) {
        //     std::cout << "FPS: " << frameCount << std::endl;
        //     frameCount = 0;
        //     startTime = currentTime;
        // }


//        cout << "sample !" << endl;
        // Process the sample
        // "buffer" and "map" are used to access raw data in the sample
        // "buffer" is a single data chunk, for raw video it's 1 frame
        // "buffer" is NOT a queue !
        // "Map" is the helper to access raw data in the buffer
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstMapInfo m;
        MY_ASSERT(gst_buffer_map(buffer, &m, GST_MAP_READ));
        MY_ASSERT(m.size == imW * imH * 3);
//        cout << "size = " << map.size << " ==? " << imW*imH*3 << endl;

        // Wrap the raw data in OpenCV frame and show on screen
        cv::Mat frame(imH, imW, CV_8UC3, (void *) m.data);

        if (!prevGray.empty()) {
            // Convert to grayscale
            cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

            // Compute the absolute difference between the current frame and the previous frame
            cv::absdiff(gray, prevGray, diff);

            // Threshold the difference image to get the motion regions
            cv::threshold(diff, diff, 25, 255, cv::THRESH_BINARY);

            // Check if there is any motion
            bool motionDetected = cv::countNonZero(diff) > 0;

            if (motionDetected) {
                noMotionCounter = 0;
                if (!isRecording) {
                    // Start recording
                    std::string timeString = timePointToString(currentTime);
                    filename = ".motion_output_" + timeString + ".mp4";
                    //3 -> fps
                    //AVI videoWriter.open(filename, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 3, cv::Size(imW, imH));
                    //videoWriter.open(filename, cv::VideoWriter::fourcc('H', '2', '6', '4'), 3, cv::Size(imW, imH));
                    videoWriter.open(videoPath + filename, cv::VideoWriter::fourcc('a', 'v', 'c', '1'), 3, cv::Size(imW, imH));
                    if (!videoWriter.isOpened()) {
                        std::cerr << "Could not open the output video file for write\n";
                        continue;
                    }
                    cout << "NEW MOTION VIDEO: " << timeString << endl;
                    isRecording = true;
                }
                // Write the frame to the video file
                videoWriter.write(frame);
            } else {
                noMotionCounter++;
                if (isRecording && noMotionCounter > noMotionThreshold) {
                    // Stop recording
                    videoWriter.release();
                    isRecording = false;

                    //rename the file
                    std::string old_name = videoPath + filename;
                    std::string new_name = videoPath + filename.substr(1);  // Remove the first character (".")

                    // Rename the file
                    if (std::rename(old_name.c_str(), new_name.c_str()) != 0) {
                        std::perror("Error renaming file");
                    }
                    std::cout << "File renamed successfully\n";

                    filename = "";
                    cout << "END VIDEO" << endl;
                }
            }
        }

        cv::cvtColor(frame, prevGray, cv::COLOR_BGR2GRAY);



/*        cv::imshow("frame", frame);
        int key = cv::waitKey(1);*/

        // Don't forget to unmap the buffer and unref the sample
        gst_buffer_unmap(buffer, &m);
        gst_sample_unref(sample);
/*        if (27 == key)
            exit(0);*/
    }
}

//======================================================================================================================
/// HTTP server process thread
void codeThreadProcessHttp(GoblinData &data) {
    while (true) {
        std::cout << "THREAD HTTP RUNNING" << std::endl;
        gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
        GstBus *bus = gst_element_get_bus(data.pipeline);
        GstMessage *msg;
        do {
            std::cout << "THREAD HTTP PRE MSG" << std::endl;
            msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
            std::cout << "THREAD HTTP POST MSG" << std::endl;
            if (msg != nullptr) {
                std::cout << "THREAD HTTP MSG" << std::endl;
                GError *err;
                gchar *dbg_info;
                switch (GST_MESSAGE_TYPE(msg)) {
                    case GST_MESSAGE_ERROR:
                        gst_message_parse_error(msg, &err, &dbg_info);
                        g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
                        g_printerr("Debugging information: %s\n", dbg_info ? dbg_info : "none");
                        g_clear_error(&err);
                        g_free(dbg_info);
                        break;
                    case GST_MESSAGE_EOS:
                        g_print("End-Of-Stream reached.\n");
                        break;
                    default:
                        // We should not reach here because we only asked for ERRORs and EOS
                        g_printerr("Unexpected message received.\n");
                        break;
                }
                gst_message_unref(msg);
            }
        } while (!msg);
        gst_object_unref(bus);
        gst_element_set_state(data.pipeline, GST_STATE_NULL);
        std::cout << "THREAD HTTP ENDED" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

//======================================================================================================================
int main(int argc, char **argv) {
    using namespace std;
    cout << "WIP split udp stream in two" << endl;

    // Init gstreamer
    gst_init(&argc, &argv);

    // Our global data
    GoblinData inputData;
    GoblinData motionData;
    GoblinData streamData;
    GError *err = nullptr;

    // Flag to enable HTTP streaming
    bool enableHttpStream = true;

    // Setup the receiving pipeline
    string rpipeStr = "udpsrc port=5057 ! application/x-rtp,encoding-name=H264,payload=96 ! rtph264depay ! h264parse ! tee name=t";
    err = nullptr;
    inputData.pipeline = gst_parse_launch(rpipeStr.c_str(), &err);
    checkErr(err);
    MY_ASSERT(inputData.pipeline);

    // Setup the motion pipeline (opencv processing)
    string mpipeStr = "t. ! queue ! avdec_h264 ! videoconvert ! video/x-raw,format=RGB,width=640,height=480 ! appsink name=sink";
    err = nullptr;
    motionData.pipeline = gst_parse_launch(mpipeStr.c_str(), &err);
    checkErr(err);
    MY_ASSERT(motionData.pipeline);
    // Find our appsink by name
    motionData.sinkVideo = gst_bin_get_by_name(GST_BIN (motionData.pipeline), "sink");
    MY_ASSERT(motionData.sinkVideo);

    // Setup the streaming pipeline (hls through http)
    string spipeStr = "t. ! queue ! avdec_h264 ! x264enc tune=zerolatency ! queue ! mpegtsmux ! hlssink max-files=5 target-duration=1 location=/var/www/html/hls/segment%05d.ts playlist-location=/var/www/html/hls/playlist.m3u8";
    err = nullptr;
    streamData.pipeline = gst_parse_launch(spipeStr.c_str(), &err);
    checkErr(err);
    MY_ASSERT(streamData.pipeline);

    // Play the pipeline
    MY_ASSERT(gst_element_set_state(inputData.pipeline, GST_STATE_PLAYING));
    MY_ASSERT(gst_element_set_state(motionData.pipeline, GST_STATE_PLAYING));


    // Start the bus thread
    thread threadBus([&inputData]() -> void {
        codeThreadBus(inputData.pipeline, inputData, "GOBLIN");
    });

    // Start the appsink process thread
    thread threadProcess([&motionData]() -> void {
        codeThreadProcessV(motionData);
    });

    if (enableHttpStream) {
        // Start the http stream process thread
        thread threadhttp([&streamData]() -> void {
            codeThreadProcessHttp(streamData);
        });

        threadhttp.join();
    }

    // Wait for threads
    threadBus.join();
    threadProcess.join();

    // Destroy the pipeline
    gst_element_set_state(inputData.pipeline, GST_STATE_NULL);
    gst_object_unref(inputData.pipeline);
    gst_element_set_state(motionData.pipeline, GST_STATE_NULL);
    gst_object_unref(motionData.pipeline);
    gst_element_set_state(streamData.pipeline, GST_STATE_NULL);
    gst_object_unref(streamData.pipeline);

    return 0;
}


//gst-launch-1.0 udpsrc port=5057 ! application/x-rtp,encoding-name=H264,payload=96 ! rtph264depay ! h264parse ! tee name=t ! queue ! videoscale ! 'video/x-raw,width=(int)960,height=(int)540' ! autovideosinkgst-launch-1.0 udpsrc port=5057 ! application/x-rtp,encoding-name=H264,payload=96 ! rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! autovideosink
//gst-launch-1.0 udpsrc port=5057 ! application/x-rtp,encoding-name=H264,payload=96 ! rtph264depay ! h264parse ! tee name=t ! queue ! avdec_h264 ! videoconvert ! autovideosink
//gst-launch-1.0 udpsrc port=5057 ! application/x-rtp,encoding-name=H264,payload=96 ! rtph264depay ! h264parse ! tee name=t ! queue ! avdec_h264 ! x264enc tune=zerolatency ! queue ! mpegtsmux ! hlssink max-files=5 target-duration=1 location=/var/www/html/hls/segment%05d.ts playlist-location=/var/www/html/hls/playlist.m3u8

//gst-launch-1.0 udpsrc port=5057 ! application/x-rtp,encoding-name=H264,payload=96 ! rtph264depay ! h264parse ! tee name=t t. ! queue ! avdec_h264 ! videoconvert ! autovideosink t. ! queue ! avdec_h264 ! x264enc tune=zerolatency ! queue ! mpegtsmux ! hlssink max-files=5 target-duration=1 location=/var/www/html/hls/segment%05d.ts playlist-location=/var/www/html/hls/playlist.m3u8