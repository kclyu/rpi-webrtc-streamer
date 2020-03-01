
// We use some GNU extensions (basename)
#define _GNU_SOURCE

#include <arpa/inet.h>
#include <memory.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sysexits.h>

#include "mmal_video.h"

/**
 * Reset the camera component, set up its ports
 *
 * @param state Pointer to state control struct
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
MMAL_STATUS_T reset_camera_component(RASPIVID_STATE *state) {
    MMAL_COMPONENT_T *camera = 0;
    MMAL_ES_FORMAT_T *format;
    MMAL_PORT_T *preview_port = NULL, *video_port = NULL, *still_port = NULL;
    MMAL_STATUS_T status;

    /* Get the camera component */
    if (state->camera_component == NULL) {
        vcos_log_error("camera component does not exist");
    } else {
        camera = state->camera_component;
    }

#ifdef __NOT_REQUIRED_IN_RESET__
    status = raspicamcontrol_set_stereo_mode(
        camera->output[0], &state->camera_parameters.stereo_mode);
    status += raspicamcontrol_set_stereo_mode(
        camera->output[1], &state->camera_parameters.stereo_mode);
    status += raspicamcontrol_set_stereo_mode(
        camera->output[2], &state->camera_parameters.stereo_mode);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("Could not set stereo mode : error %d", status);
        goto error;
    }

    MMAL_PARAMETER_INT32_T camera_num = {
        {MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)}, state->cameraNum};

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

    status = mmal_port_parameter_set_uint32(
        camera->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG,
        state->sensor_mode);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("Could not set sensor mode : error %d", status);
        goto error;
    }

#endif /* __NOT_REQUIRED_IN_RESET__ */
    preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
    video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
    still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];

#ifdef __NOT_REQUIRED_IN_RESET__
    if (state->settings) {
        MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T change_event_request = {
            {MMAL_PARAMETER_CHANGE_EVENT_REQUEST,
             sizeof(MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T)},
            MMAL_PARAMETER_CAMERA_SETTINGS,
            1};

        status =
            mmal_port_parameter_set(camera->control, &change_event_request.hdr);
        if (status != MMAL_SUCCESS) {
            vcos_log_error("No camera settings events");
        }
    }
#endif /* __NOT_REQUIRED_IN_RESET__ */

    // Enable the camera, and tell it its control callback function
    status = mmal_port_enable(camera->control, default_camera_control_callback);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("Unable to enable control port : error %d", status);
        goto error;
    }

    //  set up the camera configuration
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T cam_config = {
            {MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config)},
            .max_stills_w = state->width,
            .max_stills_h = state->height,
            .stills_yuv422 = 0,
            .one_shot_stills = 0,
            .max_preview_video_w = state->width,
            .max_preview_video_h = state->height,
            .num_preview_video_frames = 3,
            .stills_capture_circular_buffer_height = 0,
            .fast_preview_resume = 0,
            .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RAW_STC};
        mmal_port_parameter_set(camera->control, &cam_config.hdr);
    }

    // Now set up the port formats

    // Set the encode format on the Preview port
    // HW limitations mean we need the preview to be the same size as the
    // required recorded output

    format = preview_port->format;

    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;

    if (state->camera_parameters.shutter_speed > 6000000) {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {
            {MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
            {50, 1000},
            {166, 1000}};
        mmal_port_parameter_set(preview_port, &fps_range.hdr);
    } else if (state->camera_parameters.shutter_speed > 1000000) {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {
            {MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
            {166, 1000},
            {999, 1000}};
        mmal_port_parameter_set(preview_port, &fps_range.hdr);
    }

    // enable dynamic framerate if necessary
    if (state->camera_parameters.shutter_speed) {
        if (state->framerate >
            1000000. / state->camera_parameters.shutter_speed) {
            state->framerate = 0;
            if (state->verbose)
                DLOG(
                    "Enable dynamic frame rate to fulfil shutter speed "
                    "requirement");
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

    if (status != MMAL_SUCCESS) {
        vcos_log_error("camera viewfinder format couldn't be set");
        goto error;
    }

    // Set the encode format on the video  port

    format = video_port->format;
    format->encoding_variant = MMAL_ENCODING_I420;

    if (state->camera_parameters.shutter_speed > 6000000) {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {
            {MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
            {50, 1000},
            {166, 1000}};
        mmal_port_parameter_set(video_port, &fps_range.hdr);
    } else if (state->camera_parameters.shutter_speed > 1000000) {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {
            {MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
            {167, 1000},
            {999, 1000}};
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

    if (status != MMAL_SUCCESS) {
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

    if (status != MMAL_SUCCESS) {
        vcos_log_error("camera still format couldn't be set");
        goto error;
    }

    /* Ensure there are enough buffers to avoid dropping frames */
    if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

    /* Enable component */
    status = mmal_component_enable(camera);
    if (status != MMAL_SUCCESS) {
        vcos_log_error("camera component couldn't be enabled");
        goto error;
    }

    raspicamcontrol_set_all_parameters(camera, &state->camera_parameters);

    state->camera_component = camera;

    update_annotation_data(state);

    return status;

error:

    if (camera) mmal_component_destroy(camera);

    return status;
}

/**
 * Reset the encoder component, set up its ports
 *
 * @param state Pointer to state control struct
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
MMAL_STATUS_T reset_encoder_component(RASPIVID_STATE *state) {
    MMAL_COMPONENT_T *encoder = 0;
    MMAL_STATUS_T status;
#ifdef __NOT_REQUIRED_IN_RESET__
    MMAL_PORT_T *encoder_input = NULL, *encoder_output = NULL;
    MMAL_POOL_T *pool;
#endif /* __NOT_REQUIRED_IN_RESET__ */

    // Get the encoder component
    if (state->encoder_component == NULL) {
        vcos_log_error("Unable to get video encoder component");
        goto error;
    } else {
        encoder = state->encoder_component;
    }

#ifdef __NOT_REQUIRED_IN_RESET__
    if (!encoder->input_num || !encoder->output_num) {
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
        encoder_output->buffer_size = 256 << 10;

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

    if (status != MMAL_SUCCESS) {
        vcos_log_error("Unable to set format on video encoder output port");
        goto error;
    }

    if (state->encoding == MMAL_ENCODING_H264 && state->intraperiod != -1) {
        MMAL_PARAMETER_UINT32_T param = {
            {MMAL_PARAMETER_INTRAPERIOD, sizeof(param)}, state->intraperiod};
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS) {
            vcos_log_error("Unable to set intraperiod");
            goto error;
        }
    }

    if (state->encoding == MMAL_ENCODING_H264 && state->quantisationParameter) {
        MMAL_PARAMETER_UINT32_T param = {
            {MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT, sizeof(param)},
            state->quantisationInitialParameter};
        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS) {
            vcos_log_error("Unable to set initial QP");
            goto error;
        }

        MMAL_PARAMETER_UINT32_T param2 = {
            {MMAL_PARAMETER_VIDEO_ENCODE_MIN_QUANT, sizeof(param)},
            state->quantisationMinParameter};
        status = mmal_port_parameter_set(encoder_output, &param2.hdr);
        if (status != MMAL_SUCCESS) {
            vcos_log_error("Unable to set min QP");
            goto error;
        }

        MMAL_PARAMETER_UINT32_T param3 = {
            {MMAL_PARAMETER_VIDEO_ENCODE_MAX_QUANT, sizeof(param)},
            state->quantisationMaxParameter};
        status = mmal_port_parameter_set(encoder_output, &param3.hdr);
        if (status != MMAL_SUCCESS) {
            vcos_log_error("Unable to set max QP");
            goto error;
        }
    }

    if (state->encoding == MMAL_ENCODING_H264) {
        MMAL_PARAMETER_VIDEO_PROFILE_T param;
        param.hdr.id = MMAL_PARAMETER_PROFILE;
        param.hdr.size = sizeof(param);

        param.profile[0].profile = state->profile;
        param.profile[0].level =
            MMAL_VIDEO_LEVEL_H264_4;  // This is the only value supported

        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS) {
            vcos_log_error("Unable to set H264 profile");
            goto error;
        }
    }

    if (mmal_port_parameter_set_boolean(
            encoder_input, MMAL_PARAMETER_VIDEO_IMMUTABLE_INPUT,
            state->immutableInput) != MMAL_SUCCESS) {
        vcos_log_error("Unable to set immutable input flag");
        // Continue rather than abort..
    }

    // set INLINE HEADER flag to generate SPS and PPS for every IDR if requested
    if (mmal_port_parameter_set_boolean(
            encoder_output, MMAL_PARAMETER_VIDEO_ENCODE_INLINE_HEADER,
            state->bInlineHeaders) != MMAL_SUCCESS) {
        vcos_log_error("failed to set INLINE HEADER FLAG parameters");
        // Continue rather than abort..
    }

    // set Encode SPS Timing
    if (mmal_port_parameter_set_boolean(encoder_output,
                                        MMAL_PARAMETER_VIDEO_ENCODE_SPS_TIMING,
                                        MMAL_TRUE) != MMAL_SUCCESS) {
        vcos_log_error("failed to set SPS TIMING HEADER FLAG parameters");
        // Continue rather than abort..
    }

    // set Minimise Fragmentation
    if (mmal_port_parameter_set_boolean(encoder_output,
                                        MMAL_PARAMETER_MINIMISE_FRAGMENTATION,
                                        MMAL_FALSE) != MMAL_SUCCESS) {
        vcos_log_error("failed to set SPS TIMING HEADER FLAG parameters");
        // Continue rather than abort..
    }

    // Adaptive intra refresh settings
    if (state->encoding == MMAL_ENCODING_H264 &&
        state->intra_refresh_type != -1) {
        MMAL_PARAMETER_VIDEO_INTRA_REFRESH_T param;
        param.hdr.id = MMAL_PARAMETER_VIDEO_INTRA_REFRESH;
        param.hdr.size = sizeof(param);

        // Get first so we don't overwrite anything unexpectedly
        status = mmal_port_parameter_get(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS) {
            vcos_log_warn(
                "Unable to get existing H264 intra-refresh values. Please "
                "update your firmware");
            // Set some defaults, don't just pass random stack data
            param.air_mbs = param.air_ref = param.cir_mbs = param.pir_mbs = 0;
        }

        param.refresh_mode = state->intra_refresh_type;

        // if (state->intra_refresh_type ==
        // MMAL_VIDEO_INTRA_REFRESH_CYCLIC_MROWS)
        //   param.cir_mbs = 10;

        status = mmal_port_parameter_set(encoder_output, &param.hdr);
        if (status != MMAL_SUCCESS) {
            vcos_log_error("Unable to set H264 intra-refresh values");
            goto error;
        }
    }
#endif /* __NOT_REQUIRED_IN_RESET__ */

    //  Enable component
    status = mmal_component_enable(encoder);

    if (status != MMAL_SUCCESS) {
        vcos_log_error("Unable to enable video encoder component");
        goto error;
    }

#ifdef __NOT_REQUIRED_IN_RESET__
    /* Create pool of buffer headers for the output port to consume */
    pool = mmal_port_pool_create(encoder_output, encoder_output->buffer_num,
                                 encoder_output->buffer_size);

    if (!pool) {
        vcos_log_error(
            "Failed to create buffer header pool for encoder output port %s",
            encoder_output->name);
    }

    state->encoder_pool = pool;
    state->encoder_component = encoder;
#endif /* __NOT_REQUIRED_IN_RESET__ */

    return status;

error:
    if (encoder) mmal_component_destroy(encoder);

    state->encoder_component = NULL;

    return status;
}
