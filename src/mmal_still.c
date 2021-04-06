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

// We use some GNU extensions (asprintf, basename)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "mmal_still.h"

#include "mmal_common.h"

#define MAX_USER_EXIF_TAGS 32
#define MAX_EXIF_PAYLOAD_LENGTH 128

/** Struct used to pass information in encoder port userdata to callback
 */
typedef struct {
    FILE *file_handle;  /// File handle to write buffer data to.
    VCOS_SEMAPHORE_T
    complete_semaphore;  /// semaphore which is posted when we reach end of
                         /// frame (indicates end of capture or fault)
    RASPISTILL_STATE
    *pstate;  /// pointer to our state in case required in callback
} PORT_USERDATA;

static struct {
    char *format;
    MMAL_FOURCC_T encoding;
} encoding_xref[] = {{"jpg", MMAL_ENCODING_JPEG}, {"bmp", MMAL_ENCODING_BMP},
                     {"gif", MMAL_ENCODING_GIF},  {"png", MMAL_ENCODING_PNG},
                     {"ppm", MMAL_ENCODING_PPM},  {"tga", MMAL_ENCODING_TGA}};

static int encoding_xref_size =
    sizeof(encoding_xref) / sizeof(encoding_xref[0]);

MMAL_FOURCC_T getEncodingByExtension(const char *extension) {
    int len = strlen(extension);
    if (len) {
        int j;
        for (j = 0; j < encoding_xref_size; j++) {
            if (strcmp(encoding_xref[j].format, extension) == 0) {
                return encoding_xref[j].encoding;
            }
        }
    }
    // not found, using default as jpg
    return MMAL_ENCODING_JPEG;
}

const char *getExtensionByEncoding(MMAL_FOURCC_T encoding) {
    for (int i = 0; i < encoding_xref_size; i++) {
        if (encoding_xref[i].encoding == encoding)
            return encoding_xref[i].format;
    }
    return "jpg";  // default encoding extension
}

/**
 * Assign a default set of parameters to the state passed in
 *
 * @param state Pointer to state structure to assign defaults to
 */
void default_still_status(RASPISTILL_STATE *state) {
    if (!state) {
        vcos_assert(0);
        return;
    }

    // TODO: remmove unused variables
    memset(state, 0, sizeof(*state));

    state->width = 0;
    state->height = 0;
    state->filename = NULL;
    state->verbose = 0;
    state->cameraNum = 0;
    state->sensor_mode = 0;
    // state->gps = 0;

    state->timeout = -1;  // replaced with 5000ms later if unset
    state->quality = 85;
    state->wantRAW = 0;
    state->linkname = NULL;
    state->frameStart = 0;
    state->thumbnailConfig.enable = 1;
    state->thumbnailConfig.width = 64;
    state->thumbnailConfig.height = 48;
    state->thumbnailConfig.quality = 35;
    state->demoMode = 0;
    state->demoInterval = 250;  // ms
    state->camera_component = NULL;
    state->encoder_component = NULL;
    state->preview_connection = NULL;
    state->encoder_connection = NULL;
    state->encoder_pool = NULL;
    state->encoding = MMAL_ENCODING_JPEG;
    state->numExifTags = 0;
    // state->enableExifTags = 1; // RaspiStill default
    state->enableExifTags = 0;  // default is disable
    state->timelapse = 0;
    state->fullResPreview = 0;
    state->useGL = 0;
    state->glCapture = 0;
    state->burstCaptureMode = 0;
    state->datetime = 0;
    state->timestamp = 0;
    state->restart_interval = 0;

    // Setup preview window defaults
    raspipreview_set_defaults(&state->preview_parameters);

    // Set up the camera_parameters to default
    raspicamcontrol_set_defaults(&state->camera_parameters);

    // Set initial GL preview state
    // raspitex_set_defaults(&state->raspitex_state);
}

/**
 * Dump image state parameters to stdout Used for debugging
 *
 * @param state Pointer to state structure to assign defaults to
 */
void dump_still_status(RASPISTILL_STATE *state) {
    if (!state) {
        vcos_assert(0);
        return;
    }

    DLOG_FORMAT("Quality %d, Raw %s", state->quality,
                state->wantRAW ? "yes" : "no");
    DLOG_FORMAT("Thumbnail enabled %s, width %d, height %d, quality %d",
                state->thumbnailConfig.enable ? "Yes" : "No",
                state->thumbnailConfig.width, state->thumbnailConfig.height,
                state->thumbnailConfig.quality);

    DLOG_FORMAT("Time delay %d, Timelapse %d", state->timeout,
                state->timelapse);
    DLOG("Link to latest frame enabled ");
    if (state->linkname) {
        DLOG_FORMAT(" yes, -> %s", state->linkname);
    } else {
        DLOG(" no");
    }
    DLOG_FORMAT("Full resolution preview %s",
                state->fullResPreview ? "Yes" : "No");

    DLOG("EXIF tags disabled");
    raspipreview_dump_parameters(&state->preview_parameters);
    raspicamcontrol_dump_parameters(&state->camera_parameters);
}

/**
 * Create the camera component, set up its ports
 *
 * @param state Pointer to state control struct. camera_component member set to
 * the created camera_component if successful.
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
// Orig: MMAL_STATUS_T create_camera_component(RASPISTILL_STATE *state)
MMAL_STATUS_T create_camera_still_component(RASPISTILL_STATE *state) {
    MMAL_COMPONENT_T *camera = 0;
    MMAL_ES_FORMAT_T *format;
    MMAL_PORT_T *preview_port = NULL, *video_port = NULL, *still_port = NULL;
    MMAL_STATUS_T status;

    /* Create the component */
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);

    if (status != MMAL_SUCCESS) {
        DLOG_ERROR_0("Failed to create camera component");
        goto error;
    }

    status = raspicamcontrol_set_stereo_mode(
        camera->output[0], &state->camera_parameters.stereo_mode);
    status += raspicamcontrol_set_stereo_mode(
        camera->output[1], &state->camera_parameters.stereo_mode);
    status += raspicamcontrol_set_stereo_mode(
        camera->output[2], &state->camera_parameters.stereo_mode);

    if (status != MMAL_SUCCESS) {
        DLOG_ERROR("Could not set stereo mode : error %d", status);
        goto error;
    }

    MMAL_PARAMETER_INT32_T camera_num = {
        {MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)}, state->cameraNum};

    status = mmal_port_parameter_set(camera->control, &camera_num.hdr);

    if (status != MMAL_SUCCESS) {
        DLOG_ERROR("Could not select camera : error %d", status);
        goto error;
    }

    if (!camera->output_num) {
        status = MMAL_ENOSYS;
        DLOG_ERROR_0("Camera doesn't have output ports");
        goto error;
    }

    status = mmal_port_parameter_set_uint32(
        camera->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG,
        state->sensor_mode);

    if (status != MMAL_SUCCESS) {
        DLOG_ERROR("Could not set sensor mode : error %d", status);
        goto error;
    }

    preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
    video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
    still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];

    // Enable the camera, and tell it its control callback function
    status = mmal_port_enable(camera->control, default_camera_control_callback);

    if (status != MMAL_SUCCESS) {
        DLOG_ERROR("Unable to enable control port : error %d", status);
        goto error;
    }

    //  set up the camera configuration
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config = {
            {MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config)},
            .max_stills_w = state->width,
            .max_stills_h = state->height,
            .stills_yuv422 = 0,
            .one_shot_stills = 1,
            .max_preview_video_w =
                state->preview_parameters.previewWindow.width,
            .max_preview_video_h =
                state->preview_parameters.previewWindow.height,
            .num_preview_video_frames = 3,
            .stills_capture_circular_buffer_height = 0,
            .fast_preview_resume = 0,
            .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC};

        if (state->fullResPreview) {
            cam_config.max_preview_video_w = state->width;
            cam_config.max_preview_video_h = state->height;
        }

        mmal_port_parameter_set(camera->control, &cam_config.hdr);
    }

    raspicamcontrol_set_all_parameters(camera, &state->camera_parameters);

    // Now set up the port formats

    format = preview_port->format;
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;

    if (state->camera_parameters.shutter_speed > 6000000) {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {
            {MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
            {5, 1000},
            {166, 1000}};
        mmal_port_parameter_set(preview_port, &fps_range.hdr);
    } else if (state->camera_parameters.shutter_speed > 1000000) {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {
            {MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
            {166, 1000},
            {999, 1000}};
        mmal_port_parameter_set(preview_port, &fps_range.hdr);
    }
    if (state->fullResPreview) {
        // In this mode we are forcing the preview to be generated from the full
        // capture resolution. This runs at a max of 15fps with the OV5647
        // sensor.
        format->es->video.width = VCOS_ALIGN_UP(state->width, 32);
        format->es->video.height = VCOS_ALIGN_UP(state->height, 16);
        format->es->video.crop.x = 0;
        format->es->video.crop.y = 0;
        format->es->video.crop.width = state->width;
        format->es->video.crop.height = state->height;
        format->es->video.frame_rate.num = FULL_RES_PREVIEW_FRAME_RATE_NUM;
        format->es->video.frame_rate.den = FULL_RES_PREVIEW_FRAME_RATE_DEN;
    } else {
        // Use a full FOV 4:3 mode
        format->es->video.width =
            VCOS_ALIGN_UP(state->preview_parameters.previewWindow.width, 32);
        format->es->video.height =
            VCOS_ALIGN_UP(state->preview_parameters.previewWindow.height, 16);
        format->es->video.crop.x = 0;
        format->es->video.crop.y = 0;
        format->es->video.crop.width =
            state->preview_parameters.previewWindow.width;
        format->es->video.crop.height =
            state->preview_parameters.previewWindow.height;
        format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM;
        format->es->video.frame_rate.den = PREVIEW_FRAME_RATE_DEN;
    }

    status = mmal_port_format_commit(preview_port);
    if (status != MMAL_SUCCESS) {
        DLOG_ERROR_0("camera viewfinder format couldn't be set");
        goto error;
    }

    // Set the same format on the video  port (which we don't use here)
    mmal_format_full_copy(video_port->format, format);
    status = mmal_port_format_commit(video_port);

    if (status != MMAL_SUCCESS) {
        DLOG_ERROR_0("camera video format couldn't be set");
        goto error;
    }

    // Ensure there are enough buffers to avoid dropping frames
    if (video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

    format = still_port->format;

    if (state->camera_parameters.shutter_speed > 6000000) {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {
            {MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
            {5, 1000},
            {166, 1000}};
        mmal_port_parameter_set(still_port, &fps_range.hdr);
    } else if (state->camera_parameters.shutter_speed > 1000000) {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {
            {MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
            {167, 1000},
            {999, 1000}};
        mmal_port_parameter_set(still_port, &fps_range.hdr);
    }
    // Set our stills format on the stills (for encoder) port
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->es->video.width = VCOS_ALIGN_UP(state->width, 32);
    format->es->video.height = VCOS_ALIGN_UP(state->height, 16);
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = state->width;
    format->es->video.crop.height = state->height;
    format->es->video.frame_rate.num = STILLS_FRAME_RATE_NUM;
    format->es->video.frame_rate.den = STILLS_FRAME_RATE_DEN;

    status = mmal_port_format_commit(still_port);

    if (status != MMAL_SUCCESS) {
        DLOG_ERROR_0("camera still format couldn't be set");
        goto error;
    }

    /* Ensure there are enough buffers to avoid dropping frames */
    if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

    /* Enable component */
    status = mmal_component_enable(camera);

    if (status != MMAL_SUCCESS) {
        DLOG_ERROR_0("camera component couldn't be enabled");
        goto error;
    }

    state->camera_component = camera;

    if (state->verbose) DLOG_ERROR_0("Camera component done");

    return status;

error:

    if (camera) mmal_component_destroy(camera);

    return status;
}

/**
 * Destroy the camera component
 *
 * @param state Pointer to state control struct
 *
 */
void destroy_camera_still_component(RASPISTILL_STATE *state) {
    if (state->camera_component) {
        mmal_component_destroy(state->camera_component);
        state->camera_component = NULL;
    }
}

/**
 * Create the encoder component, set up its ports
 *
 * @param state Pointer to state control struct. encoder_component member set to
 * the created camera_component if successful.
 *
 * @return a MMAL_STATUS, MMAL_SUCCESS if all OK, something else otherwise
 */
// orig: MMAL_STATUS_T create_encoder_component(RASPISTILL_STATE *state)
MMAL_STATUS_T create_encoder_still_component(RASPISTILL_STATE *state) {
    MMAL_COMPONENT_T *encoder = 0;
    MMAL_PORT_T *encoder_input = NULL, *encoder_output = NULL;
    MMAL_STATUS_T status;
    MMAL_POOL_T *pool;

    status =
        mmal_component_create(MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER, &encoder);

    if (status != MMAL_SUCCESS) {
        DLOG_ERROR_0("Unable to create JPEG encoder component");
        goto error;
    }

    if (!encoder->input_num || !encoder->output_num) {
        status = MMAL_ENOSYS;
        DLOG_ERROR_0("JPEG encoder doesn't have input/output ports");
        goto error;
    }

    encoder_input = encoder->input[0];
    encoder_output = encoder->output[0];

    // We want same format on input and output
    mmal_format_copy(encoder_output->format, encoder_input->format);

    // Specify out output format
    encoder_output->format->encoding = state->encoding;

    encoder_output->buffer_size = encoder_output->buffer_size_recommended;

    if (encoder_output->buffer_size < encoder_output->buffer_size_min)
        encoder_output->buffer_size = encoder_output->buffer_size_min;

    encoder_output->buffer_num = encoder_output->buffer_num_recommended;

    if (encoder_output->buffer_num < encoder_output->buffer_num_min)
        encoder_output->buffer_num = encoder_output->buffer_num_min;

    // Commit the port changes to the output port
    status = mmal_port_format_commit(encoder_output);

    if (status != MMAL_SUCCESS) {
        DLOG_ERROR_0("Unable to set format on video encoder output port");
        goto error;
    }

    // Set the JPEG quality level
    status = mmal_port_parameter_set_uint32(
        encoder_output, MMAL_PARAMETER_JPEG_Q_FACTOR, state->quality);

    if (status != MMAL_SUCCESS) {
        DLOG_ERROR_0("Unable to set JPEG quality");
        goto error;
    }

    // Set the JPEG restart interval
    status = mmal_port_parameter_set_uint32(
        encoder_output, MMAL_PARAMETER_JPEG_RESTART_INTERVAL,
        state->restart_interval);

    if (state->restart_interval && status != MMAL_SUCCESS) {
        DLOG_ERROR_0("Unable to set JPEG restart interval");
        goto error;
    }

    // Set up any required thumbnail
    {
        MMAL_PARAMETER_THUMBNAIL_CONFIG_T param_thumb = {
            {MMAL_PARAMETER_THUMBNAIL_CONFIGURATION,
             sizeof(MMAL_PARAMETER_THUMBNAIL_CONFIG_T)},
            0,
            0,
            0,
            0};

        if (state->thumbnailConfig.enable && state->thumbnailConfig.width > 0 &&
            state->thumbnailConfig.height > 0) {
            // Have a valid thumbnail defined
            param_thumb.enable = 1;
            param_thumb.width = state->thumbnailConfig.width;
            param_thumb.height = state->thumbnailConfig.height;
            param_thumb.quality = state->thumbnailConfig.quality;
        }
        status = mmal_port_parameter_set(encoder->control, &param_thumb.hdr);
    }

    //  Enable component
    status = mmal_component_enable(encoder);

    if (status != MMAL_SUCCESS) {
        DLOG_ERROR_0("Unable to enable video encoder component");
        goto error;
    }

    /* Create pool of buffer headers for the output port to consume */
    pool = mmal_port_pool_create(encoder_output, encoder_output->buffer_num,
                                 encoder_output->buffer_size);

    if (!pool) {
        DLOG_ERROR(
            "Failed to create buffer header pool for encoder output port %s",
            encoder_output->name);
    }

    state->encoder_pool = pool;
    state->encoder_component = encoder;

    if (state->verbose) DLOG_ERROR_0("Encoder component done");

    return status;

error:

    if (encoder) mmal_component_destroy(encoder);

    return status;
}

/**
 * Destroy the encoder component
 *
 * @param state Pointer to state control struct
 *
 */
void destroy_encoder_still_component(RASPISTILL_STATE *state) {
    // Get rid of any port buffers first
    if (state->encoder_pool) {
        mmal_port_pool_destroy(state->encoder_component->output[0],
                               state->encoder_pool);
    }

    if (state->encoder_component) {
        mmal_component_destroy(state->encoder_component);
        state->encoder_component = NULL;
    }
}

