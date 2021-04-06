/*
 * Includes for broadcom and ilclient headers wrapped in C style.
 */
#ifndef MMAL_COMMON_H_
#define MMAL_COMMON_H_

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
// Log macro definition to stdout
//
///////////////////////////////////////////////////////////////////////////////
#define DLOG(msg) printf("(%s:%d): %s\n", __FILE__, __LINE__, msg);

#define DLOG_FORMAT(format, args...)        \
    printf("(%s:%d):", __FILE__, __LINE__); \
    printf(format, args);                   \
    printf("\n");

#define DLOG_ERROR(format, args...)                  \
    fprintf(stderr, "(%s:%d):", __FILE__, __LINE__); \
    fprintf(stderr, format, args);                   \
    fprintf(stderr, "\n");

#define DLOG_ERROR_0(message)                        \
    fprintf(stderr, "(%s:%d):", __FILE__, __LINE__); \
    fprintf(stderr, message);                        \
    fprintf(stderr, "\n");

#ifdef __MMAL_ENCODER_DEBUG__
#define DEBUG_LOG(format, args...)          \
    printf("(%s:%d):", __FILE__, __LINE__); \
    printf(format, args)
#else
#define DEBUG_LOG(format, args...)
#endif /* __MMAL_ENCODER_DEBUG__ */

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

// Stills format information
// 0 implies variable
#define STILLS_FRAME_RATE_NUM 0
#define STILLS_FRAME_RATE_DEN 1

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // MMAL_COMMON_H_
