/*
 * Includes for broadcom and ilclient headers wrapped in C style.
 */
#ifndef __MMAL_VIDEO_H__
#define __MMAL_VIDEO_H__

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

///////////////////////////////////////////////////////////////////////////////
//
// MMAL related header files
//
///////////////////////////////////////////////////////////////////////////////
#include <semaphore.h>

#include "bcm_host.h"
#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_parameters_camera.h"
#include "interface/mmal/util/mmal_connection.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/vcos/vcos.h"
#include "raspicamcontrol.h"
#include "raspicli.h"
#include "raspipreview.h"

#define RASPI_CAM_FIXED_WIDTH 1296
#define RASPI_CAM_FIXED_HEIGHT 972

///////////////////////////////////////////////////////////////////////////////
//
// Log macro definition for stdout
//
///////////////////////////////////////////////////////////////////////////////
#define DLOG(msg) printf("(%s:%d): %s\n", __FILE__, __LINE__, msg);

#define DLOG_FORMAT(format, args...)        \
    printf("(%s:%d):", __FILE__, __LINE__); \
    printf(format, args);                   \
    printf("\n");

#define DLOG_ERR(msg, err)                                     \
    fprintf(err, "(%s:%d): %s: %s\n", __FILE__, __LINE__, msg, \
            omx_errorcode_to_str(err));

#ifdef __MMAL_ENCODER_DEBUG__
#define DEBUG_LOG(format, args...)          \
    printf("(%s:%d):", __FILE__, __LINE__); \
    printf(format, args)
#else
#define DEBUG_LOG(format, args...)
#endif /* __MMAL_ENCODER_DEBUG__ */

///////////////////////////////////////////////////////////////////////////////
//
// Definition from RaspiVid.c
//
///////////////////////////////////////////////////////////////////////////////
// Forward
typedef struct RASPIVID_STATE_S RASPIVID_STATE;

// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

// Port configuration for the splitter component
#define SPLITTER_OUTPUT_PORT 0
#define SPLITTER_PREVIEW_PORT 1

// Video format information
// 0 implies variable
#define VIDEO_FRAME_RATE_NUM 30
#define VIDEO_FRAME_RATE_DEN 1

/// Video render needs at least 2 buffers.
#define VIDEO_OUTPUT_BUFFERS_NUM 3

#define VIDEO_INTRAFRAME_PERIOD 3  /// 3 seconds

///////////////////////////////////////////////////////////////////////////////
//
// Structre from RaspiVid.c to share with warpper class
//
///////////////////////////////////////////////////////////////////////////////
//
/** Struct used to pass information in encoder port userdata to callback
 */

// frame assemble buffer size
#define FRAME_BUFSIZE (30 * 1000)

typedef struct {
    RASPIVID_STATE
    *pstate;    /// pointer to our state in case required in callback
    int abort;  /// Set to 1 in callback if an error occurs to attempt to abort
                /// the capture
    int frame_buff[FRAME_BUFSIZE];  /// frame buffer to assemble segmented frame
                                    /// from encoder
    int frame_buff_pos;
} PORT_USERDATA;

/** Possible raw output formats
 */
typedef enum {
    RAW_OUTPUT_FMT_YUV = 1,
    RAW_OUTPUT_FMT_RGB,
    RAW_OUTPUT_FMT_GRAY,
} RAW_OUTPUT_FMT;

/** Structure containing all state information for the current run
 */
struct RASPIVID_STATE_S {
    //
    // keeping the width, height, verbose, cameraNum, settings, sensor_mode
    // instead of moving to common_settings of newly updated RaspiVid.c
    //
    int width;               /// Requested width of image
    int height;              /// requested height of image
    MMAL_FOURCC_T encoding;  /// Requested codec video encoding (MJPEG or H264)
    int bitrate;             /// Requested bitrate
    int framerate;           /// Requested frame rate (fps)
    int intraperiod;         /// Intra-refresh period (key frame rate)
    int quantisationParameter;  /// Quantisation parameter - quality. Set
                                /// bitrate 0 and set this for variable bitrate

    //  Quantization Parameters
    int quantisationInitialParameter;  // Initial quantization parameter
    int quantisationMaxParameter;      /// Maximum quantization parameter
    int quantisationMinParameter;      /// Minimum quantization parameter
    // remove

    int videoRateControl;  /// Video Rate Control
    int bInlineHeaders;    /// Insert inline headers to stream (SPS, PPS)
    int verbose;           /// !0 if want detailed run information
    int immutableInput;    /// Flag to specify whether encoder works in place or
                         /// creates a new buffer. Result is preview can display
                         /// either
    /// the camera output or the encoder output (with compression artifacts)
    int profile;  /// H264 profile to use for encoding
    int level;    /// H264 level to use for encoding

    RASPIPREVIEW_PARAMETERS preview_parameters;    /// Preview setup parameters
    RASPICAM_CAMERA_PARAMETERS camera_parameters;  /// Camera setup parameters

    MMAL_COMPONENT_T *camera_component;    /// Pointer to the camera component
    MMAL_COMPONENT_T *splitter_component;  /// Pointer to the splitter component
    MMAL_COMPONENT_T *encoder_component;   /// Pointer to the encoder component

    MMAL_CONNECTION_T *preview_connection;   /// Pointer to the connection from
                                             /// camera or splitter to preview
    MMAL_CONNECTION_T *splitter_connection;  /// Pointer to the connection from
                                             /// camera to splitter
    MMAL_CONNECTION_T *encoder_connection;   /// Pointer to the connection from
                                             /// camera to encoder

    MMAL_POOL_T *splitter_pool;  /// Pointer to the pool of buffers used by
                                 /// splitter output port 0
    MMAL_POOL_T *encoder_pool;   /// Pointer to the pool of buffers used by
                                 /// encoder output port

    PORT_USERDATA callback_data;  /// Used to move data to the encoder callback

    int bCapturing;           /// State of capture/pause
    int inlineMotionVectors;  /// Encoder outputs inline Motion Vectors

    int cameraNum;           /// Camera number
    int settings;            /// Request settings from the camera
    int sensor_mode;         /// Sensor mode. 0=auto. Check docs/forum for modes
                             /// selected by other values.
    int intra_refresh_type;  /// What intra refresh type to use. -1 to not set.

    MMAL_BOOL_T addSPSTiming;
    int slices;
};

///////////////////////////////////////////////////////////////////////////////
//
// Function prototypes
//
///////////////////////////////////////////////////////////////////////////////
//
// mmal_video.c
//

void default_status(RASPIVID_STATE *state);
void dump_status(RASPIVID_STATE *state);
void camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
void update_annotation_data(RASPIVID_STATE *state);
void encoder_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
MMAL_STATUS_T create_camera_component(RASPIVID_STATE *state);
void destroy_camera_component(RASPIVID_STATE *state);
MMAL_STATUS_T create_splitter_component(RASPIVID_STATE *state);
void destroy_splitter_component(RASPIVID_STATE *state);
MMAL_STATUS_T create_encoder_component(RASPIVID_STATE *state);
void destroy_encoder_component(RASPIVID_STATE *state);
MMAL_STATUS_T connect_ports(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port,
                            MMAL_CONNECTION_T **connection);
void check_disable_port(MMAL_PORT_T *port);

//
// mmal_video_reset.c
//
MMAL_STATUS_T reset_encoder_component(RASPIVID_STATE *state);
MMAL_STATUS_T reset_camera_component(RASPIVID_STATE *state);

//
// mmal_util.c
//
void check_camera_model(int cam_num);
MMAL_STATUS_T check_installed_camera(void);
int mmal_status_to_int(MMAL_STATUS_T status);
void dump_component(char *prefix, MMAL_COMPONENT_T *component);
void dump_all_mmal_component(RASPIVID_STATE *state);
void dump_buffer_flag(char *buf, int buflen, int flags);
void dump_mmal_buffer(int id, const MMAL_BUFFER_HEADER_T *buf);
MMAL_STATUS_T check_installed_camera(void);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // __MMAL_VIDEO_H__
