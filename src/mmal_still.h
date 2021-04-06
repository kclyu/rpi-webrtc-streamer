/*
Copyright (c) 2018, Raspberry Pi (Trading) Ltd.
Copyright (c) 2013, Broadcom Europe Ltd.
Copyright (c) 2013, James Hughes
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * \file RaspiStill.c
 * Command line program to capture a still frame and encode it to file.
 * Also optionally display a preview/viewfinder of current camera input.
 *
 * Description
 *
 * 3 components are created; camera, preview and JPG encoder.
 * Camera component has three ports, preview, video and stills.
 * This program connects preview and stills to the preview and jpg
 * encoder. Using mmal we don't need to worry about buffers between these
 * components, but we do need to handle buffers from the encoder, which
 * are simply written straight to the file in the requisite buffer callback.
 *
 * We use the RaspiCamControl code to handle the specific camera settings.
 */

#ifndef MMAL_STILL_H_
#define MMAL_STILL_H_

// We use some GNU extensions (asprintf, basename)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "mmal_common.h"
#include "stdio.h"
#include "unistd.h"

// Stills format information
// 0 implies variable

#define MAX_USER_EXIF_TAGS 32
#define MAX_EXIF_PAYLOAD_LENGTH 128

/// Amount of time before first image taken to allow settling of
/// exposure etc. in milliseconds.
#define CAMERA_SETTLE_TIME 1000

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** Structure containing all state information for the current run
 */
typedef struct {
    char camera_name[MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN];  // Name of the
                                                               // camera sensor
    int width;        /// Requested width of image
    int height;       /// requested height of image
    char *filename;   /// filename of output file
    int cameraNum;    /// Camera number
    int sensor_mode;  /// Sensor mode. 0=auto. Check docs/forum for modes
                      /// selected by other values.
    int verbose;      /// !0 if want detailed run information
    int gps;          /// Add real-time gpsd output to output

    int timeout;  /// Time taken before frame is grabbed and app then shuts
                  /// down. Units are milliseconds
    int quality;  /// JPEG quality setting (1-100)
    int wantRAW;  /// Flag for whether the JPEG metadata also contains the RAW
                  /// bayer image
    char *linkname;  /// filename of output file
    int frameStart;  /// First number of frame output counter
    MMAL_PARAM_THUMBNAIL_CONFIG_T thumbnailConfig;
    int demoMode;            /// Run app in demo mode
    int demoInterval;        /// Interval between camera settings changes
    MMAL_FOURCC_T encoding;  /// Encoding to use for the output file.
    const char
        *exifTags[MAX_USER_EXIF_TAGS];  /// Array of pointers to tags supplied
                                        /// from the command line
    int numExifTags;                    /// Number of supplied tags
    int enableExifTags;                 /// Enable/Disable EXIF tags in output
    int timelapse;       /// Delay between each picture in timelapse mode. If 0,
                         /// disable timelapse
    int fullResPreview;  /// If set, the camera preview port runs at capture
                         /// resolution. Reduces fps.
    int frameNextMethod;   /// Which method to use to advance to next frame
    int useGL;             /// Render preview using OpenGL
    int glCapture;         /// Save the GL frame-buffer instead of camera output
    int burstCaptureMode;  /// Enable burst mode
    int datetime;          /// Use DateTime instead of frame#
    int timestamp;         /// Use timestamp instead of frame#
    int restart_interval;  /// JPEG restart interval. 0 for none.

    RASPIPREVIEW_PARAMETERS preview_parameters;    /// Preview setup parameters
    RASPICAM_CAMERA_PARAMETERS camera_parameters;  /// Camera setup parameters

    MMAL_COMPONENT_T *camera_component;   /// Pointer to the camera component
    MMAL_COMPONENT_T *encoder_component;  /// Pointer to the encoder component
    MMAL_COMPONENT_T
    *null_sink_component;  /// Pointer to the null sink component
    MMAL_CONNECTION_T *preview_connection;  /// Pointer to the connection from
                                            /// camera to preview
    MMAL_CONNECTION_T *encoder_connection;  /// Pointer to the connection from
                                            /// camera to encoder

    MMAL_POOL_T *encoder_pool;  /// Pointer to the pool of buffers used by
                                /// encoder output port
} RASPISTILL_STATE;

/** Struct used to pass information in encoder port userdata to callback
 */
typedef struct {
    FILE *file_handle;  /// File handle to write buffer data to.
    VCOS_SEMAPHORE_T
    complete_semaphore;  /// semaphore which is posted when we reach end of
                         /// frame (indicates end of capture or fault)
    RASPISTILL_STATE
    *pstate;  /// pointer to our state in case required in callback
} PORT_STILL_USERDATA;

MMAL_FOURCC_T getEncodingByExtension(const char *extension);
const char *getExtensionByEncoding(MMAL_FOURCC_T encoding);
void default_still_status(RASPISTILL_STATE *state);
void dump_still_status(RASPISTILL_STATE *state);
MMAL_STATUS_T create_camera_still_component(RASPISTILL_STATE *state);
void destroy_camera_still_component(RASPISTILL_STATE *state);
MMAL_STATUS_T create_encoder_still_component(RASPISTILL_STATE *state);
void destroy_encoder_still_component(RASPISTILL_STATE *state);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // MMAL_STILL_H_
