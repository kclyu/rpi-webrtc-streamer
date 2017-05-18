/*
 *  Lyu,KeunChang
 *
 * This is a stripped down version of the original RaspiVid module from the
 * raspberry pi userland-master branch,
 * Original copyright info below
 */
/*
Copyright (c) 2013, Broadcom Europe Ltd
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
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * \file RaspiVid.c
 * Command line program to capture a camera video stream and encode it to file.
 * Also optionally display a preview/viewfinder of current camera input.
 *
 * \date 28th Feb 2013
 * \Author: James Hughes
 *
 * Description
 *
 * 3 components are created; camera, preview and video encoder.
 * Camera component has three ports, preview, video and stills.
 * This program connects preview and stills to the preview and video
 * encoder. Using mmal we don't need to worry about buffers between these
 * components, but we do need to handle buffers from the encoder, which
 * are simply written straight to the file in the requisite buffer callback.
 *
 * We use the RaspiCamControl code to handle the specific camera settings.
 * We use the RaspiPreview code to handle the (generic) preview window
 */

// We use some GNU extensions (basename)
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <sysexits.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define VERSION_STRING "v1.3.12"

#include "mmal_encoder.h"


// Max bitrate we allow for recording
static const int MAX_BITRATE = 25000000; // 25Mbits/s

/// Interval at which we check for an failure abort during capture
const int ABORT_INTERVAL = 100; // ms


/// Structure to cross reference H264 profile strings against the MMAL parameter equivalent
static XREF_T  profile_map[] =
{
    {"baseline",     MMAL_VIDEO_PROFILE_H264_BASELINE},
    {"main",         MMAL_VIDEO_PROFILE_H264_MAIN},
    {"high",         MMAL_VIDEO_PROFILE_H264_HIGH},
//   {"constrained",  MMAL_VIDEO_PROFILE_H264_CONSTRAINED_BASELINE} // Does anyone need this?
};

static int profile_map_size = sizeof(profile_map) / sizeof(profile_map[0]);


static XREF_T  intra_refresh_map[] =
{
    {"cyclic",       MMAL_VIDEO_INTRA_REFRESH_CYCLIC},
    {"adaptive",     MMAL_VIDEO_INTRA_REFRESH_ADAPTIVE},
    {"both",         MMAL_VIDEO_INTRA_REFRESH_BOTH},
    {"cyclicrows",   MMAL_VIDEO_INTRA_REFRESH_CYCLIC_MROWS},
//   {"random",       MMAL_VIDEO_INTRA_REFRESH_PSEUDO_RAND} Cannot use random, crashes the encoder. No idea why.
};

static int intra_refresh_map_size = sizeof(intra_refresh_map) / sizeof(intra_refresh_map[0]);



/**
 * Assign a default set of parameters to the state passed in
 *
 * @param state Pointer to state structure to assign defaults to
 */
void default_status(RASPIVID_STATE *state)
{
    if (!state)
    {
        vcos_assert(0);
        return;
    }

    // Default everything to zero
    memset(state, 0, sizeof(RASPIVID_STATE));

#ifdef __WEBRTC_DEFAULT__
    state->width = RASPI_CAM_FIXED_WIDTH;	// default width for FOV
    state->height = RASPI_CAM_FIXED_HEIGHT;	// default width for FOV
    state->encoding = MMAL_ENCODING_H264;
    // state->bitrate = 17000000; 			// This is a decent default bitrate for 1080p
    state->bitrate = 0; 			// For variable bitrate setttings
    state->framerate = VIDEO_FRAME_RATE_NUM;
    state->intra_refresh_type = MMAL_VIDEO_INTRA_REFRESH_BOTH;		// cyclic intra rehash type
    state->intraperiod = VIDEO_FRAME_RATE_NUM * 3;	// every 3 second
    state->bInlineHeaders = MMAL_TRUE;	    // enabling Inline Header
    state->profile = MMAL_VIDEO_PROFILE_H264_CONSTRAINED_BASELINE;  
    // state->profile = MMAL_VIDEO_PROFILE_H264_MAIN;  
    state->verbose = MMAL_TRUE;

#define __RATECONTROL_DISABLE__

#ifdef __RATECONTROL_DISABLE__
    state->videoRateControl = MMAL_FALSE;
    state->quantisationParameter = MMAL_TRUE;
    state->quantisationInitialParameter = 26;   // Initial quantization parameter
    state->quantisationMaxParameter	= 35;       /// Maximum quantization parameter
    state->quantisationMinParameter  = 24;       /// Minimum quantization parameter
#else
    state->videoRateControl = MMAL_TRUE;
    state->quantisationParameter = MMAL_FALSE;   // disable quatization, Not working
    state->quantisationInitialParameter = 26;   // Initial quantization parameter
    state->quantisationMaxParameter	= 35;       /// Maximum quantization parameter
    state->quantisationMinParameter  = 22;       /// Minimum quantization parameter
#endif // __RATECONTROL_DISABLE__

    // state->settings = MMAL_TRUE;				// camera setting
    state->settings = MMAL_FALSE;

#else	/* RaspiVid defaults */
    state->width = 1920;       // Default to 1080p
    state->height = 1080;
    state->encoding = MMAL_ENCODING_H264;
    state->bitrate = 17000000; // This is a decent default bitrate for 1080p
    state->framerate = VIDEO_FRAME_RATE_NUM;
    state->intraperiod = -1;    // Not set
    state->intra_refresh_type = -1;
    state->bInlineHeaders = 0;
    state->quantisationParameter = MMAL_FALSE;
    state->profile = MMAL_VIDEO_PROFILE_H264_HIGH;
    state->verbose = MMAL_TRUE;
    state->videoRateControl = 0;
    state->settings = 0;
#endif

    state->immutableInput = 1;
    state->bCapturing = 0;

    state->cameraNum = 0;
    state->sensor_mode = 0;

    state->frame = 0;

    // Setup preview window defaults
    raspipreview_set_defaults(&state->preview_parameters);

    // Set up the camera_parameters to default
    raspicamcontrol_set_defaults(&state->camera_parameters);
}


/**
 * Dump image state parameters to stderr.
 *
 * @param state Pointer to state structure to assign defaults to
 */
void dump_status(RASPIVID_STATE *state)
{
    if (!state)
    {
        vcos_assert(0);
        return;
    }

    DLOG_FORMAT("Width %d, Height %d", state->width, state->height );
    DLOG_FORMAT("bitrate %d, framerate %d", state->bitrate, state->framerate );
    DLOG_FORMAT("H264 Profile %s", raspicli_unmap_xref(state->profile, profile_map, profile_map_size));
    DLOG_FORMAT("H264 Quantisation level %d, Inline headers %s",
                state->quantisationParameter, state->bInlineHeaders ? "Yes" : "No");
    DLOG_FORMAT("H264 Intra refresh type %s, period %d",
                raspicli_unmap_xref(state->intra_refresh_type, intra_refresh_map, intra_refresh_map_size),
                state->intraperiod);

    raspipreview_dump_parameters(&state->preview_parameters);
    raspicamcontrol_dump_parameters(&state->camera_parameters);
}



/**
 *  buffer header callback function for camera control
 *
 *  Callback will dump buffer data to the specific file
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
void camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    if (buffer->cmd == MMAL_EVENT_PARAMETER_CHANGED)
    {
        MMAL_EVENT_PARAMETER_CHANGED_T *param = (MMAL_EVENT_PARAMETER_CHANGED_T *)buffer->data;
        switch (param->hdr.id) {
        case MMAL_PARAMETER_CAMERA_SETTINGS:
        {
            MMAL_PARAMETER_CAMERA_SETTINGS_T *settings = (MMAL_PARAMETER_CAMERA_SETTINGS_T*)param;
            mmal_encoder_copy_camera_settings(settings);
        }
        break;
        default:
            DLOG_FORMAT("Unprocessed Camera Event: %d", (param->hdr.id));
            break;
        }
    }
    else if (buffer->cmd == MMAL_EVENT_ERROR)
    {
        vcos_log_error("No data received from sensor. Check all connections, including the Sunny one on the camera board");
    }
    else
    {
        vcos_log_error("Received unexpected camera control callback event, 0x%08x", buffer->cmd);
    }

    mmal_buffer_header_release(buffer);
}


/**
 * Update any annotation data specific to the video.
 * This simply passes on the setting from cli, or
 * if application defined annotate requested, updates
 * with the H264 parameters
 *
 * @param state Pointer to state control struct
 *
 */
void update_annotation_data(RASPIVID_STATE *state)
{
    // So, if we have asked for a application supplied string, set it to the H264 parameters
    if (state->camera_parameters.enable_annotate & ANNOTATE_APP_TEXT)
    {
        char *text;
        const char *refresh = raspicli_unmap_xref(state->intra_refresh_type, 
                intra_refresh_map, intra_refresh_map_size);

        asprintf(&text,  "%dk,%df,%s,%d,%s",
                 state->bitrate / 1000,  state->framerate,
                 refresh ? refresh : "(none)",
                 state->intraperiod,
                 raspicli_unmap_xref(state->profile, profile_map, profile_map_size));

        raspicamcontrol_set_annotate(state->camera_component, 
                state->camera_parameters.enable_annotate, text,
                state->camera_parameters.annotate_text_size,
                state->camera_parameters.annotate_text_colour,
                state->camera_parameters.annotate_bg_colour);

        free(text);
    }
    else
    {
        raspicamcontrol_set_annotate(state->camera_component, 
                state->camera_parameters.enable_annotate, 
                state->camera_parameters.annotate_string,
                state->camera_parameters.annotate_text_size,
                state->camera_parameters.annotate_text_colour,
                state->camera_parameters.annotate_bg_colour);
    }
}

/**
 * Create the camera component, set up its ports
 *
 * @param state Pointer to state control struct
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
MMAL_STATUS_T create_camera_component(RASPIVID_STATE *state) {
    MMAL_COMPONENT_T *camera = 0;
    MMAL_ES_FORMAT_T *format;
    MMAL_PORT_T *preview_port = NULL, *video_port = NULL, *still_port = NULL;
    MMAL_STATUS_T status;


    /* Create the component */
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);
    if (status != MMAL_SUCCESS) {
        vcos_log_error("Failed to create camera component");
        goto error;
    }

    status = raspicamcontrol_set_stereo_mode(camera->output[0], 
            &state->camera_parameters.stereo_mode);
    status += raspicamcontrol_set_stereo_mode(camera->output[1], 
            &state->camera_parameters.stereo_mode);
    status += raspicamcontrol_set_stereo_mode(camera->output[2], 
            &state->camera_parameters.stereo_mode);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("Could not set stereo mode : error %d", status);
        goto error;
    }

    MMAL_PARAMETER_INT32_T camera_num =
    {{MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)}, state->cameraNum};

    status = mmal_port_parameter_set(camera->control, &camera_num.hdr);
    if (status != MMAL_SUCCESS) {
        vcos_log_error("Could not select camera : error %d", status);
        goto error;
    }

    if (!camera->output_num) {
        status = MMAL_ENOSYS;
        vcos_log_error("Camera doesn't have output ports");
        goto error;
    }

    status = mmal_port_parameter_set_uint32(camera->control, 
            MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, state->sensor_mode);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Could not set sensor mode : error %d", status);
        goto error;
    }

    preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
    video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
    still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];

    if (state->settings) {
        MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T change_event_request =
        {   {MMAL_PARAMETER_CHANGE_EVENT_REQUEST, sizeof(MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T)},
            MMAL_PARAMETER_CAMERA_SETTINGS, 1
        };

        status = mmal_port_parameter_set(camera->control, &change_event_request.hdr);
        if ( status != MMAL_SUCCESS ) {
            vcos_log_error("No camera settings events");
        }
    }

    //
    mmal_encoder_set_camera_settings(camera);

    // Enable the camera, and tell it its control callback function
    status = mmal_port_enable(camera->control, camera_control_callback);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("Unable to enable control port : error %d", status);
        goto error;
    }

    //  set up the camera configuration
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config =
        {
            { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
            .max_stills_w = state->width,
            .max_stills_h = state->height,
            .stills_yuv422 = 0,
            .one_shot_stills = 0,
            .max_preview_video_w = state->width,
            .max_preview_video_h = state->height,
            .num_preview_video_frames = 3,
            .stills_capture_circular_buffer_height = 0,
            .fast_preview_resume = 0,
            .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RAW_STC
        };
        mmal_port_parameter_set(camera->control, &cam_config.hdr);
    }

    // Now set up the port formats

    // Set the encode format on the Preview port
    // HW limitations mean we need the preview to be the same size as the required recorded output

    format = preview_port->format;

    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;

    if(state->camera_parameters.shutter_speed > 6000000)
    {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
            { 50, 1000 }, {166, 1000}
        };
        mmal_port_parameter_set(preview_port, &fps_range.hdr);
    }
    else if(state->camera_parameters.shutter_speed > 1000000)
    {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
            { 166, 1000 }, {999, 1000}
        };
        mmal_port_parameter_set(preview_port, &fps_range.hdr);
    }

    //enable dynamic framerate if necessary
    if (state->camera_parameters.shutter_speed)
    {
        if (state->framerate > 1000000./state->camera_parameters.shutter_speed)
        {
            state->framerate=0;
            if (state->verbose)
                DLOG("Enable dynamic frame rate to fulfil shutter speed requirement");
        }
    }

    format->encoding = MMAL_ENCODING_OPAQUE;
    format->es->video.width = VCOS_ALIGN_UP(state->width, 32);
    format->es->video.height = VCOS_ALIGN_UP(state->height, 16);
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = state->width;
    format->es->video.crop.height = state->height;
    format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM;
    format->es->video.frame_rate.den = PREVIEW_FRAME_RATE_DEN;

    status = mmal_port_format_commit(preview_port);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("camera viewfinder format couldn't be set");
        goto error;
    }

    // Set the encode format on the video  port

    format = video_port->format;
    format->encoding_variant = MMAL_ENCODING_I420;

    if(state->camera_parameters.shutter_speed > 6000000)
    {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
            { 50, 1000 }, {166, 1000}
        };
        mmal_port_parameter_set(video_port, &fps_range.hdr);
    }
    else if(state->camera_parameters.shutter_speed > 1000000)
    {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
            { 167, 1000 }, {999, 1000}
        };
        mmal_port_parameter_set(video_port, &fps_range.hdr);
    }

    format->encoding = MMAL_ENCODING_OPAQUE;
    format->es->video.width = VCOS_ALIGN_UP(state->width, 32);
    format->es->video.height = VCOS_ALIGN_UP(state->height, 16);
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = state->width;
    format->es->video.crop.height = state->height;
    format->es->video.frame_rate.num = state->framerate;
    format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN;

    status = mmal_port_format_commit(video_port);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("camera video format couldn't be set");
        goto error;
    }

    // Ensure there are enough buffers to avoid dropping frames
    if (video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;


    // Set the encode format on the still port

    format = still_port->format;

    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;

    format->es->video.width = VCOS_ALIGN_UP(state->width, 32);
    format->es->video.height = VCOS_ALIGN_UP(state->height, 16);
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = state->width;
    format->es->video.crop.height = state->height;
    format->es->video.frame_rate.num = 0;
    format->es->video.frame_rate.den = 1;

    status = mmal_port_format_commit(still_port);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("camera still format couldn't be set");
        goto error;
    }

    /* Ensure there are enough buffers to avoid dropping frames */
    if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

    /* Enable component */
    status = mmal_component_enable(camera);
    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("camera component couldn't be enabled");
        goto error;
    }

    raspicamcontrol_set_all_parameters(camera, &state->camera_parameters);

    state->camera_component = camera;

    update_annotation_data(state);

    return status;

error:

    if (camera)
        mmal_component_destroy(camera);

    return status;
}

/**
 * Destroy the camera component
 *
 * @param state Pointer to state control struct
 *
 */
void destroy_camera_component(RASPIVID_STATE *state)
{
    if (state->camera_component)
    {
        mmal_component_destroy(state->camera_component);
        state->camera_component = NULL;
    }
}

/**
 * Create the encoder component, set up its ports
 *
 * @param state Pointer to state control struct
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
MMAL_STATUS_T create_encoder_component(RASPIVID_STATE *state)
{
    MMAL_COMPONENT_T *encoder = 0;
    MMAL_PORT_T *encoder_input = NULL, *encoder_output = NULL;
    MMAL_STATUS_T status;
    MMAL_POOL_T *pool;


    // Create encoder component
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER, &encoder);
    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Unable to create video encoder component");
        goto error;
    }

    if (!encoder->input_num || !encoder->output_num)
    {
        status = MMAL_ENOSYS;
        vcos_log_error("Video encoder doesn't have input/output ports");
        goto error;
    }

    encoder_input = encoder->input[0];
    encoder_output = encoder->output[0];

    // We want same format on input and output
    mmal_format_copy(encoder_output->format, encoder_input->format);

    // Only supporting H264 at the moment
    encoder_output->format->encoding = state->encoding;

    encoder_output->format->bitrate = state->bitrate;

    if (state->encoding == MMAL_ENCODING_H264)
        encoder_output->buffer_size = encoder_output->buffer_size_recommended;
    else
        encoder_output->buffer_size = 256<<10;


    if (encoder_output->buffer_size < encoder_output->buffer_size_min)
        encoder_output->buffer_size = encoder_output->buffer_size_min;

    encoder_output->buffer_num = encoder_output->buffer_num_recommended;

    if (encoder_output->buffer_num < encoder_output->buffer_num_min)
        encoder_output->buffer_num = encoder_output->buffer_num_min;

    // We need to set the frame rate on output to 0, to ensure it gets
    // updated correctly from the input framerate when port connected
    encoder_output->format->es->video.frame_rate.num = 0;
    encoder_output->format->es->video.frame_rate.den = 1;

    // Commit the port changes to the output port
    status = mmal_port_format_commit(encoder_output);

    if (status != MMAL_SUCCESS)
    {
        vcos_log_error("Unable to set format on video encoder output port");
        goto error;
    }

    // Set the rate control parameter
    if (state->videoRateControl)
    {
        /***
         * MMAL RATE Control constants
         *
         * MMAL_VIDEO_RATECONTROL_DEFAULT,
         * MMAL_VIDEO_RATECONTROL_VARIABLE,
         * MMAL_VIDEO_RATECONTROL_CONSTANT,
         * MMAL_VIDEO_RATECONTROL_VARIABLE_SKIP_FRAMES,
         * MMAL_VIDEO_RATECONTROL_CONSTANT_SKIP_FRAMES,
         */
        MMAL_PARAMETER_VIDEO_RATECONTROL_T param =
        {{ MMAL_PARAMETER_RATECONTROL, sizeof(param)}, MMAL_VIDEO_RATECONTROL_CONSTANT};
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to set ratecontrol");
            goto error;
        }

        /***
         * MMAL RATE Control Model
         * MMAL_VIDEO_ENCODER_RC_MODEL_DEFAULT    = 0,
         * MMAL_VIDEO_ENCODER_RC_MODEL_JVT = MMAL_VIDEO_ENCODER_RC_MODEL_DEFAULT,
         * MMAL_VIDEO_ENCODER_RC_MODEL_VOWIFI,
         * MMAL_VIDEO_ENCODER_RC_MODEL_CBR,
         * MMAL_VIDEO_ENCODER_RC_MODEL_LAST,
         * MMAL_VIDEO_ENCODER_RC_MODEL_DUMMY      = 0x7FFFFFFF
         */
        MMAL_PARAMETER_VIDEO_ENCODE_RC_MODEL_T param2 =
        {{ MMAL_PARAMETER_VIDEO_ENCODE_RC_MODEL, sizeof(param2)}, MMAL_VIDEO_ENCODER_RC_MODEL_CBR};
        status = mmal_port_parameter_set(encoder_output, &param2.hdr);
        if (status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to set rate control model");
            goto error;
        }

    }

    if (state->encoding == MMAL_ENCODING_H264 &&
            state->intraperiod != -1)
    {
        MMAL_PARAMETER_UINT32_T param = {{ MMAL_PARAMETER_INTRAPERIOD, sizeof(param)}, state->intraperiod};
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to set intraperiod");
            goto error;
        }
    }

    if (state->encoding == MMAL_ENCODING_H264 && state->quantisationParameter)
    {

        MMAL_PARAMETER_UINT32_T param =
        {{ MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT, sizeof(param)}, state->quantisationInitialParameter};
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to set initial QP");
            goto error;
        }

        MMAL_PARAMETER_UINT32_T param2 =
        {{ MMAL_PARAMETER_VIDEO_ENCODE_MIN_QUANT, sizeof(param)}, state->quantisationMinParameter};
        status = mmal_port_parameter_set(encoder_output, &param2.hdr);
        if (status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to set min QP");
            goto error;
        }

        MMAL_PARAMETER_UINT32_T param3 =
        {{ MMAL_PARAMETER_VIDEO_ENCODE_MAX_QUANT, sizeof(param)}, state->quantisationMaxParameter};
        status = mmal_port_parameter_set(encoder_output, &param3.hdr);
        if (status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to set max QP");
            goto error;
        }


    }

    if (state->encoding == MMAL_ENCODING_H264)
    {
        MMAL_PARAMETER_VIDEO_PROFILE_T  param;
        param.hdr.id = MMAL_PARAMETER_PROFILE;
        param.hdr.size = sizeof(param);

        param.profile[0].profile = state->profile;
        param.profile[0].level = MMAL_VIDEO_LEVEL_H264_4; // This is the only value supported

        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS)
        {
            vcos_log_error("Unable to set H264 profile");
            goto error;
        }
    }

    if (mmal_port_parameter_set_boolean(encoder_input,
        MMAL_PARAMETER_VIDEO_IMMUTABLE_INPUT, state->immutableInput) != MMAL_SUCCESS) {
        vcos_log_error("Unable to set immutable input flag");
        // Continue rather than abort..
    }

    //set INLINE HEADER flag to generate SPS and PPS for every IDR if requested
    if (mmal_port_parameter_set_boolean(encoder_output,
        MMAL_PARAMETER_VIDEO_ENCODE_INLINE_HEADER, state->bInlineHeaders) != MMAL_SUCCESS) {
        vcos_log_error("failed to set INLINE HEADER FLAG parameters");
        // Continue rather than abort..
    }

#ifdef __NOT_WORKING__
    // set CABAC disable
    // MMAL_PARAMETER_VIDEO_ENCODE_H264_DISABLE_CABAC false
    if (mmal_port_parameter_set_boolean(encoder_output,
        MMAL_PARAMETER_VIDEO_ENCODE_H264_DISABLE_CABAC, MMAL_TRUE) != MMAL_SUCCESS) {
        vcos_log_error("failed to set CABAC DISABLE parameters");
        // Continue rather than abort..
    }
#endif // __NOT_WORKING__

    //set Encode SPS Timing
    if (mmal_port_parameter_set_boolean(encoder_output,
        MMAL_PARAMETER_VIDEO_ENCODE_SPS_TIMING, MMAL_TRUE) != MMAL_SUCCESS) {
        vcos_log_error("failed to set SPS TIMING HEADER FLAG parameters");
        // Continue rather than abort..
    }

    // set Minimise Fragmentation
    if (mmal_port_parameter_set_boolean(encoder_output,
        MMAL_PARAMETER_MINIMISE_FRAGMENTATION, MMAL_FALSE) != MMAL_SUCCESS) {
        vcos_log_error("failed to set SPS TIMING HEADER FLAG parameters");
        // Continue rather than abort..
    }

    // Adaptive intra refresh settings
    if (state->encoding == MMAL_ENCODING_H264 &&
            state->intra_refresh_type != -1) {
        MMAL_PARAMETER_VIDEO_INTRA_REFRESH_T  param;
        param.hdr.id = MMAL_PARAMETER_VIDEO_INTRA_REFRESH;
        param.hdr.size = sizeof(param);

        // Get first so we don't overwrite anything unexpectedly
        status = mmal_port_parameter_get(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS) {
            vcos_log_warn("Unable to get existing H264 intra-refresh values. Please update your firmware");
            // Set some defaults, don't just pass random stack data
            param.air_mbs = param.air_ref = param.cir_mbs = param.pir_mbs = 0;
        }

        param.refresh_mode = state->intra_refresh_type;

        //if (state->intra_refresh_type == MMAL_VIDEO_INTRA_REFRESH_CYCLIC_MROWS)
        //  param.cir_mbs = 10;

        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS) {
            vcos_log_error("Unable to set H264 intra-refresh values");
            goto error;
        }
    }

    //  Enable component
    status = mmal_component_enable(encoder);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("Unable to enable video encoder component");
        goto error;
    }

    /* Create pool of buffer headers for the output port to consume */
    pool = mmal_port_pool_create(encoder_output, encoder_output->buffer_num, encoder_output->buffer_size);

    if (!pool) {
        vcos_log_error("Failed to create buffer header pool for encoder output port %s", encoder_output->name);
    }

    state->encoder_pool = pool;
    state->encoder_component = encoder;

    return status;

error:
    if (encoder)
        mmal_component_destroy(encoder);

    state->encoder_component = NULL;

    return status;
}

/**
 * Destroy the encoder component
 *
 * @param state Pointer to state control struct
 *
 */
void destroy_encoder_component(RASPIVID_STATE *state)
{
    // Get rid of any port buffers first
    if (state->encoder_pool) {
        mmal_port_pool_destroy(state->encoder_component->output[0], state->encoder_pool);
    }

    if (state->encoder_component) {
        mmal_component_destroy(state->encoder_component);
        state->encoder_component = NULL;
    }
}

/**
 * Connect two specific ports together
 *
 * @param output_port Pointer the output port
 * @param input_port Pointer the input port
 * @param Pointer to a mmal connection pointer, reassigned if function successful
 * @return Returns a MMAL_STATUS_T giving result of operation
 *
 */
MMAL_STATUS_T connect_ports(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port, 
        MMAL_CONNECTION_T **connection) {
    MMAL_STATUS_T status;

    status =  mmal_connection_create(connection, output_port, input_port, 
            MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);

    if (status == MMAL_SUCCESS) {
        status =  mmal_connection_enable(*connection);
        if (status != MMAL_SUCCESS)
            mmal_connection_destroy(*connection);
    }

    return status;
}

/**
 * Checks if specified port is valid and enabled, then disables it
 *
 * @param port  Pointer the port
 *
 */
void check_disable_port(MMAL_PORT_T *port) {
    if (port && port->is_enabled)
        mmal_port_disable(port);
}

