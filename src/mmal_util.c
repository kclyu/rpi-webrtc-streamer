/*
Copyright (c) 2017, rpi-webrtc-streamer Lyu,KeunChang

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

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sysexits.h>

#include "mmal_video.h"

// Original fucntion defined in the RaspiVid.c
void check_camera_model(int cam_num) {
    MMAL_COMPONENT_T *camera_info;
    MMAL_STATUS_T status;

    // Try to get the camera name
    status =
        mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA_INFO, &camera_info);
    if (status == MMAL_SUCCESS) {
        MMAL_PARAMETER_CAMERA_INFO_T param;
        param.hdr.id = MMAL_PARAMETER_CAMERA_INFO;
        param.hdr.size = sizeof(param) -
                         4;  // Deliberately undersize to check firmware version
        status = mmal_port_parameter_get(camera_info->control, &param.hdr);

        if (status != MMAL_SUCCESS) {
            // Running on newer firmware
            param.hdr.size = sizeof(param);
            status = mmal_port_parameter_get(camera_info->control, &param.hdr);
            if (status == MMAL_SUCCESS &&
                param.num_cameras > (unsigned int)cam_num) {
                if (!strncmp(param.cameras[cam_num].camera_name, "toshh2c",
                             7)) {
                    fprintf(stderr,
                            "The driver for the TC358743 HDMI to CSI2 chip you "
                            "are using is NOT supported.\n");
                    fprintf(stderr,
                            "They were written for a demo purposes only, and "
                            "are in the firmware on an as-is\n");
                    fprintf(stderr,
                            "basis and therefore requests for support or "
                            "changes will not be acted on.\n\n");
                }
            }
        }

        mmal_component_destroy(camera_info);
    }
}

MMAL_STATUS_T check_installed_camera(void) {
    MMAL_COMPONENT_T *camera_info;
    MMAL_STATUS_T status;

    // Try to get the camera name
    status =
        mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA_INFO, &camera_info);
    if (status == MMAL_SUCCESS) {
        MMAL_PARAMETER_CAMERA_INFO_T param;
        param.hdr.id = MMAL_PARAMETER_CAMERA_INFO;
        param.hdr.size = sizeof(param);
        status = mmal_port_parameter_get(camera_info->control, &param.hdr);
        if (status != MMAL_SUCCESS) {
            return status;
        }

        if (param.num_cameras == 0) {
            return MMAL_ENXIO;
        };

        for (uint32_t i = 0; i < param.num_cameras; i++) {
            printf("Cameras installed : %d\n", param.num_cameras);
            printf("Camera device name  : %s\n", param.cameras[i].camera_name);
        };

        mmal_component_destroy(camera_info);
        return MMAL_SUCCESS;
    }
    return status;
}

/**
 * Convert a MMAL status return value to a simple boolean of success
 * ALso displays a fault if code is not success
 *
 * @param status The error code to convert
 * @return 0 if status is success, 1 otherwise
 */
int mmal_status_to_int(MMAL_STATUS_T status) {
    if (status == MMAL_SUCCESS)
        return 0;
    else {
        switch (status) {
            case MMAL_ENOMEM:
                vcos_log_error("Out of memory");
                break;
            case MMAL_ENOSPC:
                vcos_log_error("Out of resources (other than memory)");
                break;
            case MMAL_EINVAL:
                vcos_log_error("Argument is invalid");
                break;
            case MMAL_ENOSYS:
                vcos_log_error("Function not implemented");
                break;
            case MMAL_ENOENT:
                vcos_log_error("No such file or directory");
                break;
            case MMAL_ENXIO:
                vcos_log_error("No such device or address");
                break;
            case MMAL_EIO:
                vcos_log_error("I/O error");
                break;
            case MMAL_ESPIPE:
                vcos_log_error("Illegal seek");
                break;
            case MMAL_ECORRUPT:
                vcos_log_error("Data is corrupt \attention FIXME: not POSIX");
                break;
            case MMAL_ENOTREADY:
                vcos_log_error(
                    "Component is not ready \attention FIXME: not POSIX");
                break;
            case MMAL_ECONFIG:
                vcos_log_error(
                    "Component is not configured \attention FIXME: not POSIX");
                break;
            case MMAL_EISCONN:
                vcos_log_error("Port is already connected ");
                break;
            case MMAL_ENOTCONN:
                vcos_log_error("Port is disconnected");
                break;
            case MMAL_EAGAIN:
                vcos_log_error(
                    "Resource temporarily unavailable. Try again later");
                break;
            case MMAL_EFAULT:
                vcos_log_error("Bad address");
                break;
            default:
                vcos_log_error("Unknown status error");
                break;
        }

        return 1;
    }
}

/*****************************************************************************
 *
 * Helper functions to print component/format/port/buffer to console
 *
 ****************************************************************************/
static void dump_mmal_format(MMAL_ES_FORMAT_T *format) {
    const char *name_type;

    if (!format) return;

    switch (format->type) {
        case MMAL_ES_TYPE_AUDIO:
            name_type = "audio";
            break;
        case MMAL_ES_TYPE_VIDEO:
            name_type = "video";
            break;
        case MMAL_ES_TYPE_SUBPICTURE:
            name_type = "subpicture";
            break;
        default:
            name_type = "unknown";
            break;
    }

    DLOG_FORMAT("    type: %s, fourcc: %4.4s", name_type,
                (char *)&format->encoding);
    DLOG_FORMAT("      bitrate: %i, framed: %i", format->bitrate,
                !!(format->flags & MMAL_ES_FORMAT_FLAG_FRAMED));
    DLOG_FORMAT("      extra data: %i, %p", format->extradata_size,
                format->extradata);
    switch (format->type) {
        case MMAL_ES_TYPE_AUDIO:
            DLOG_FORMAT(
                "     samplerate: %i, channels: %i, bps: %i, block align: %i",
                format->es->audio.sample_rate, format->es->audio.channels,
                format->es->audio.bits_per_sample,
                format->es->audio.block_align);
            break;

        case MMAL_ES_TYPE_VIDEO:
            DLOG_FORMAT("      width: %i, height: %i, (%i,%i,%i,%i)",
                        format->es->video.width, format->es->video.height,
                        format->es->video.crop.x, format->es->video.crop.y,
                        format->es->video.crop.width,
                        format->es->video.crop.height);
            DLOG_FORMAT("      color_space : %4.4s",
                        format->es->video.color_space);
            DLOG_FORMAT("      pixel aspect ratio: %i/%i, frame rate: %i/%i",
                        format->es->video.par.num, format->es->video.par.den,
                        format->es->video.frame_rate.num,
                        format->es->video.frame_rate.den);
            break;

        case MMAL_ES_TYPE_SUBPICTURE:
            break;

        default:
            break;
    }
}

static void dump_mmal_port(MMAL_PORT_T *port) {
    MMAL_ES_FORMAT_T *format;
    if (!port) return;

    DLOG_FORMAT("%s(%p)", port->name, port);
    DLOG_FORMAT(
        "  buffers num: %i(opt %i, min %i), size: %i(opt %i, min: %i), align: "
        "%i",
        port->buffer_num, port->buffer_num_recommended, port->buffer_num_min,
        port->buffer_size, port->buffer_size_recommended, port->buffer_size_min,
        port->buffer_alignment_min);
    if (port->format) {
        format = port->format;
        dump_mmal_format(format);
    }
}

void dump_component(char *prefix, MMAL_COMPONENT_T *component) {
    MMAL_PORT_T *port_ptr = NULL;

    if (component == NULL) {
        DLOG_FORMAT("Component %s: (nil)", prefix);
    } else {
        uint32_t i;
        DLOG_FORMAT("Component %s: (%s) %s", prefix,
                    component->is_enabled ? "enabled" : "disabled",
                    component->name);

        for (i = 0; i < component->input_num; i++) {
            if (component->input) {
                port_ptr = component->input[i];
                dump_mmal_port(port_ptr);
            }
        }

        for (i = 0; i < component->output_num; i++) {
            if (component->output) {
                port_ptr = component->output[i];
                dump_mmal_port(port_ptr);
            }
        }
    }
}

void dump_all_mmal_component(RASPIVID_STATE *state) {
    dump_component((char *)"Camera : ", state->camera_component);
    dump_component((char *)"Encoder: ", state->encoder_component);
    // dump_component( "Preview: ", state->preview_parameters->preview_component
    // );
}

void dump_buffer_flag(char *buf, int buflen, int flags) {
    snprintf(
        buf, buflen, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
        (flags & MMAL_BUFFER_HEADER_FLAG_EOS) ? "EOS" : "",
        (flags & MMAL_BUFFER_HEADER_FLAG_FRAME_START) ? ",START" : "",
        (flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END) ? ",END" : "",
        (flags & MMAL_BUFFER_HEADER_FLAG_FRAME) ? ",FRAME" : "",
        (flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME) ? ",KEY" : "",

        (flags & MMAL_BUFFER_HEADER_FLAG_DISCONTINUITY) ? ",DIS" : "",
        (flags & MMAL_BUFFER_HEADER_FLAG_CONFIG) ? ",CFG" : "",
        (flags & MMAL_BUFFER_HEADER_FLAG_ENCRYPTED) ? ",ENC" : "",
        (flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO) ? ",CODIDE" : "",
        (flags & MMAL_BUFFER_HEADER_FLAGS_SNAPSHOT) ? ",SNAP" : "",

        (flags & MMAL_BUFFER_HEADER_FLAG_CORRUPTED) ? ",CORR" : "",
        (flags & MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED) ? ",FAIL" : "",
        (flags & MMAL_BUFFER_HEADER_FLAG_DECODEONLY) ? ",DECODE" : "",
        (flags & MMAL_BUFFER_HEADER_FLAG_USER0) ? ",USR0" : "",
        (flags & MMAL_BUFFER_HEADER_FLAG_USER1) ? ",USR0" : "",
        (flags & MMAL_BUFFER_HEADER_FLAG_USER2) ? ",USR0" : "");
}

void dump_mmal_buffer(int id, const MMAL_BUFFER_HEADER_T *buf) {
    char strbuf[512];
    strbuf[0] = 0;

    dump_buffer_flag(strbuf, sizeof(strbuf), buf->flags);

    DLOG_FORMAT("%d: FRAME (%s:%d)(%s): size %d,off:%d", id, strbuf, buf->flags,
                (buf->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END) ? "" : "SEG",
                buf->length, buf->offset);
    fflush(stdout);
}
