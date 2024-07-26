#include <string>
#include <chrono>
#include <ctime>
//rename
#include <cstdio>

#include <gst/gst.h>
#include <opencv2/opencv.hpp>

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _PipelineData
{
  GstElement *pipeline;
  GstElement *source; //udpsrc port=5057 application/x-rtp,encoding-name=H264,payload=96
  GstElement *rtpdepay; // rtph264depay
  GstElement *parse; // h264parse
  GstElement *split; //tee name=t t.
  //GstElement *reencode_b1; //queu ! avdec_h264 ! videoconvert ! video/x-raw,format=RGB,width=640,height=480 
  GstElement *queue_b1; //queue
  GstElement *decode_b1; //avdec_h264
  GstElement *convert_b1; //videoconvert
  GstElement *filter_b1; //
  //GstElement *reencode_b2; //queue ! avdec_h264 ! x264enc tune=zerolatency ! queue ! mpegtsmux
  GstElement *queue1_b2; //queue
  GstElement *decode_b2; //avdec_h264
  GstElement *encode_b2; //x264enc tune=zerolatency 
  GstElement *queue2_b2; //queue
  GstElement *muxer_b2; //mpegtsmux

  GstElement *sink_b1; //appsink name=sink
  GstElement *sink_b2; //hlssink max-files=5 target-duration=1 location=/var/www/html/hls/segment%05d.ts playlist-location=/var/www/html/hls/playlist.m3u8
} PipelineData;

/// A simple assertion function + macro
inline void myAssert(bool b, const std::string &s = "MYASSERT ERROR !") {
    if (!b)
        throw std::runtime_error(s);
}

#define MY_ASSERT(x) myAssert(x, "MYASSERT ERROR :" #x)

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

/* //Handler for the hls pad-update signal
static void hls_pad_handler (GstElement * src, GstPad * pad, PipelineData * data);*/

//////////////////////////////////////////////////////////////////////////
// GLOBAL DATA FOR MOTION DETECTION
std::string videoPath = "/home/promax/Dev/playground/gstreamer_pl/resources/video/static/";
std::string filename = "";
cv::Mat frame, gray, prevGray, diff;
cv::VideoWriter videoWriter;
int noMotionCounter = 0;
const int noMotionThreshold = 10;
bool isRecording = false;

// Variables for time management
auto lastTime = std::chrono::high_resolution_clock::now();
auto startTime = lastTime;

/* The appsink has received a buffer */
static GstFlowReturn imageProcessing(GstElement *sink, PipelineData *data) {

  int imW = 0;/*= static_cast<int>(capture.get(cv::CAP_PROP_FRAME_WIDTH));*/
  int imH = 0;/*= static_cast<int>(capture.get(cv::CAP_PROP_FRAME_HEIGHT));*/

  GstSample *sample;
  /* Retrieve the buffer */
  g_signal_emit_by_name (sink, "pull-sample", &sample);
  if (sample) {
    /* Process current frame */

    // Get width and height from sample caps (NOT element caps)
    GstCaps *caps = gst_sample_get_caps(sample);
    if (caps != nullptr) {
      GstStructure *s = gst_caps_get_structure(caps, 0);
      //int imW, imH;
      MY_ASSERT(gst_structure_get_int(s, "width", &imW));
      MY_ASSERT(gst_structure_get_int(s, "height", &imH));

      auto currentTime = std::chrono::high_resolution_clock::now();

      GstBuffer *buffer = gst_sample_get_buffer(sample);
      GstMapInfo m;
      MY_ASSERT(gst_buffer_map(buffer, &m, GST_MAP_READ));
      MY_ASSERT(m.size == imW * imH * 3);

      // convert sample to opencv frame
      cv::Mat frame(imH, imW, CV_8UC3, (void *) m.data);

      //ERROR unexpected frame (mosaïque) WIP

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
                    std::cerr << "Could not open the output video file for writing\n";
                } else {
                  std::cout << "NEW MOTION VIDEO: " << timeString << std::endl;
                  isRecording = true;
                }
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
            std::cout << "END VIDEO" << std::endl;
          }
        }
      }
      cv::cvtColor(frame, prevGray, cv::COLOR_BGR2GRAY);
      gst_buffer_unmap(buffer, &m);
    } else {
      g_print("ERROR - imageProcessing: unable to retrive sample width and height\n");
    }
    
    gst_sample_unref (sample);
    return GST_FLOW_OK;
  }


  return GST_FLOW_ERROR;
}


int main (int argc, char *argv[]) {
  
  PipelineData data;
  
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;
  //gboolean terminate = FALSE;

  /* Initialize GStreamer */
  gst_init (&argc, &argv);


  /* Create the elements */

  // udp source
  data.source = gst_element_factory_make ("udpsrc", "source");

  // rtp to raw h264
  data.rtpdepay = gst_element_factory_make("rtph264depay", "depay");
  data.parse = gst_element_factory_make("h264parse", "parse");

  // stream splitter
  data.split = gst_element_factory_make("tee", "t");

  if (!data.source || !data.rtpdepay || !data.parse || !data.split) {
    g_printerr ("Enable to create first part of the main pipeline.\n");
    return -1;
  }

  // Opencv processing branch
  data.queue_b1 = gst_element_factory_make("queue", "queue_b1");
  data.decode_b1 = gst_element_factory_make("avdec_h264", "decode_b1"); 
  data.convert_b1 = gst_element_factory_make("videoconvert", "convert_b1");
  data.filter_b1 = gst_element_factory_make("capsfilter", "filter_b1");
  data.sink_b1 =  gst_element_factory_make("appsink", "sink_b1");
  if (!data.queue_b1 || !data.decode_b1 || !data.convert_b1 || !data.filter_b1 || !data.sink_b1) {
    g_printerr ("Enable to create the first pipeline branch.\n");
    return -1;
  }

  // hls branch
  data.queue1_b2 = gst_element_factory_make("queue", "queue1_b2");
  data.decode_b2 = gst_element_factory_make("avdec_h264", "decode_b2");
  data.encode_b2 = gst_element_factory_make("x264enc", "encode_b2");
  data.queue2_b2 = gst_element_factory_make("queue", "queue2_b2");
  data.muxer_b2 = gst_element_factory_make("mpegtsmux", "muxer_b2");
  data.sink_b2 =  gst_element_factory_make("hlssink", "sink_b2");
  if (!data.queue1_b2 || !data.decode_b2 || !data.encode_b2 || !data.queue2_b2 || !data.muxer_b2 || !data.sink_b2) {
    g_printerr ("Enable to create the second pipeline branch.\n");
    return -1;
  }

  /* Set element properties */
  // src
  g_object_set(data.source, "port", 5057, NULL);
  GstCaps *caps = gst_caps_new_simple("application/x-rtp",
                              "encoding-name", G_TYPE_STRING, "H264",
                              "payload", G_TYPE_INT, 96,
                              NULL);
  g_object_set(data.source, "caps", caps, NULL);
  gst_caps_unref(caps);

  // appsink (b1)
  g_object_set (data.sink_b1, "emit-signals", TRUE, NULL);
  g_signal_connect (data.sink_b1, "new-sample", G_CALLBACK(imageProcessing), NULL);

  // encoder b2
  g_object_set(data.encode_b2, "tune", 4, NULL); // 4 = Zero latency

  // sink hls
  g_object_set(data.sink_b2, "max-files", 5, "target-duration", 1,
                "location", "/var/www/html/hls/segment%05d.ts",
                "playlist-location", "/var/www/html/hls/playlist.m3u8", NULL);

  /* Create the pipeline */
  data.pipeline = gst_pipeline_new("main_pipeline");
  if (!data.pipeline) {
    g_printerr ("Enable to create the main pipeline.\n");
    return -1;
  }

  /* Link all elements that can be automatically linked because they have "Always" pads */
  gst_bin_add_many (GST_BIN (data.pipeline), data.source, data.rtpdepay, data.parse, data.split,
                                            data.queue_b1, data.decode_b1, data.convert_b1, data.filter_b1, data.sink_b1,
                                            data.queue1_b2, data.decode_b2, data.encode_b2, data.queue2_b2, data.muxer_b2, data.sink_b2, NULL); 
  if ( (gst_element_link_many (data.source, data.rtpdepay, data.parse, data.split, NULL) != TRUE) || 
       (gst_element_link_many (data.queue_b1, data.decode_b1, data.convert_b1, data.filter_b1, data.sink_b1, NULL) != TRUE) || 
       (gst_element_link_many (data.queue1_b2, data.decode_b2, data.encode_b2, data.queue2_b2, data.muxer_b2, data.sink_b2, NULL) != TRUE) ) {
    g_printerr ("Elements could not be linked.\n");
    gst_object_unref (data.pipeline);
    return -1;
  }

  /* Manually link the Tee, which has "Request" pads */
  GstPad *split_b1_pad, *split_b2_pad;
  GstPad *queue_b1_pad, *queue_b2_pad;

  split_b1_pad = gst_element_request_pad_simple (data.split, "src_%u"); // from doc “sink” (for tee sink Pads) and “src_%u” (for the Request Pads)
  g_print("Obtained request pad %s for b1 branch (opencv processing).\n", gst_pad_get_name(split_b1_pad));
  queue_b1_pad = gst_element_get_static_pad (data.queue_b1, "sink"); // get sink pad of the data.queue_b1 element

  split_b2_pad = gst_element_request_pad_simple (data.split, "src_%u");
  g_print ("Obtained request pad %s for b2 branch (hls).\n", gst_pad_get_name (split_b2_pad));
  queue_b2_pad = gst_element_get_static_pad (data.queue1_b2, "sink");

  if (gst_pad_link (split_b1_pad, queue_b1_pad) != GST_PAD_LINK_OK ||
      gst_pad_link (split_b2_pad, queue_b2_pad) != GST_PAD_LINK_OK) {
    g_printerr ("Stream spitter (Tee) could not be linked.\n");
    gst_object_unref (data.pipeline);
    return -1;
  }
  gst_object_unref (queue_b1_pad);
  gst_object_unref (queue_b2_pad);

  /* Start playing the pipeline */
  gst_element_set_state (data.pipeline, GST_STATE_PLAYING);

  /* Wait until error or EOS */
  bus = gst_element_get_bus (data.pipeline);
  msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
      (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));


  /* Release the request pads from the Tee, and unref them */
  gst_element_release_request_pad (data.split, split_b1_pad);
  gst_element_release_request_pad (data.split, split_b2_pad);
  gst_object_unref (split_b1_pad);
  gst_object_unref (split_b2_pad);

  /* Free resources */
  if (msg != NULL)
    gst_message_unref (msg);
  gst_object_unref (bus);
  gst_element_set_state (data.pipeline, GST_STATE_NULL);
  gst_object_unref (data.pipeline);

  return 0;
}

