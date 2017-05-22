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
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <sysexits.h>

#include <sys/types.h>

#include "mmal_encoder.h"


// static variable for holding camera settings
static MMAL_PARAMETER_CAMERA_SETTINGS_T camera_settings;
static uint16_t camera_setting_updated = 0;

void mmal_encoder_copy_camera_settings( MMAL_PARAMETER_CAMERA_SETTINGS_T *settings )
{
    camera_setting_updated = MMAL_TRUE;
    memcpy( &camera_settings, settings, sizeof(MMAL_PARAMETER_CAMERA_SETTINGS_T));
}

void mmal_encoder_print_camera_settings( void )
{
    DLOG_FORMAT("Exposure now %u, analog gain %u/%u, digital gain %u/%u",
                camera_settings.exposure,
                camera_settings.analog_gain.num, camera_settings.analog_gain.den,
                camera_settings.digital_gain.num, camera_settings.digital_gain.den);
    DLOG_FORMAT("AWB R=%u/%u, B=%u/%u",
                camera_settings.awb_red_gain.num, camera_settings.awb_red_gain.den,
                camera_settings.awb_blue_gain.num, camera_settings.awb_blue_gain.den
               );
}


/*****************************************************************************/
int mmal_encoder_set_camera_settings(MMAL_COMPONENT_T *camera )
{
    MMAL_PARAMETER_CAMERA_SETTINGS_T setting;
    memcpy(&setting, &camera_settings, sizeof( camera_settings));
    setting.hdr.id = MMAL_PARAMETER_CAMERA_SETTINGS;
    setting.hdr.size = sizeof(setting);

    if (!camera) return 1;
    if( camera_setting_updated == 0 ) return 1;

    DLOG("Resetting camera setting with previous one");
    mmal_encoder_print_camera_settings();

    return mmal_status_to_int(mmal_port_parameter_set(camera->control, &setting.hdr));
}

/*****************************************************************************/
static void dump_mmal_format(MMAL_ES_FORMAT_T *format)
{
    const char *name_type;

    if (!format)
        return;

    switch(format->type)
    {
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

    DLOG_FORMAT("    type: %s, fourcc: %4.4s", name_type, (char *)&format->encoding);
    DLOG_FORMAT("      bitrate: %i, framed: %i", format->bitrate,
                !!(format->flags & MMAL_ES_FORMAT_FLAG_FRAMED));
    DLOG_FORMAT("      extra data: %i, %p", format->extradata_size, format->extradata);
    switch(format->type)
    {
    case MMAL_ES_TYPE_AUDIO:
        DLOG_FORMAT("     samplerate: %i, channels: %i, bps: %i, block align: %i",
                    format->es->audio.sample_rate, format->es->audio.channels,
                    format->es->audio.bits_per_sample, format->es->audio.block_align);
        break;

    case MMAL_ES_TYPE_VIDEO:
        DLOG_FORMAT("      width: %i, height: %i, (%i,%i,%i,%i)",
                    format->es->video.width, format->es->video.height,
                    format->es->video.crop.x, format->es->video.crop.y,
                    format->es->video.crop.width, format->es->video.crop.height);
        DLOG_FORMAT("      pixel aspect ratio: %i/%i, frame rate: %i/%i",
                    format->es->video.par.num, format->es->video.par.den,
                    format->es->video.frame_rate.num, format->es->video.frame_rate.den);
        break;

    case MMAL_ES_TYPE_SUBPICTURE:
        break;

    default:
        break;
    }
}


/*****************************************************************************/
static void dump_mmal_port(MMAL_PORT_T *port)
{
    MMAL_ES_FORMAT_T *format;
    if (!port) return;

    DLOG_FORMAT("%s(%p)", port->name, port);
    DLOG_FORMAT("  buffers num: %i(opt %i, min %i), size: %i(opt %i, min: %i), align: %i",
                port->buffer_num, port->buffer_num_recommended, port->buffer_num_min,
                port->buffer_size, port->buffer_size_recommended, port->buffer_size_min,
                port->buffer_alignment_min);
    if( port->format ) {
        format = port->format;
        dump_mmal_format(format);
    }
}

/*****************************************************************************/
void dump_component(char *prefix, MMAL_COMPONENT_T *component)
{
    MMAL_PORT_T *port_ptr=NULL;

    if( component == NULL ) {
        DLOG_FORMAT("Component %s: (nil)", prefix);
    }
    else {
        uint32_t	i;
        DLOG_FORMAT("Component %s: (%s) %s",
                    prefix,
                    component->is_enabled?"enabled":"disabled",
                    component->name );

        for( i = 0; i < component->input_num ; i++ ) {
            if( component->input ) {
                port_ptr = component->input[i];
                dump_mmal_port( port_ptr );
            }
        }

        for( i = 0; i < component->output_num ; i++ ) {
            if( component->output ) {
                port_ptr = component->output[i];
                dump_mmal_port( port_ptr );
            }
        }
    }
}

void dump_all_mmal_component(RASPIVID_STATE *state)
{
    dump_component( (char *)"Camera : ", state->camera_component );
    dump_component( (char *)"Encoder: ", state->encoder_component );
    // dump_component( "Preview: ", state->preview_parameters->preview_component );
}


void dump_buffer_flag(char *buf, int buflen, int flags )
{
    snprintf(buf, buflen, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
             ( flags & MMAL_BUFFER_HEADER_FLAG_EOS) ? "EOS":"",
             ( flags & MMAL_BUFFER_HEADER_FLAG_FRAME_START) ? ",START":"",
             ( flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END) ? ",END":"",
             ( flags & MMAL_BUFFER_HEADER_FLAG_FRAME) ? ",FRAME":"",
             ( flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME) ? ",KEY":"",

             ( flags & MMAL_BUFFER_HEADER_FLAG_DISCONTINUITY) ? ",DIS":"",
             ( flags & MMAL_BUFFER_HEADER_FLAG_CONFIG) ? ",CFG":"",
             ( flags & MMAL_BUFFER_HEADER_FLAG_ENCRYPTED) ? ",ENC":"",
             ( flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO) ? ",CODIDE":"",
             ( flags & MMAL_BUFFER_HEADER_FLAGS_SNAPSHOT) ? ",SNAP":"",

             ( flags & MMAL_BUFFER_HEADER_FLAG_CORRUPTED) ? ",CORR":"",
             ( flags & MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED) ? ",FAIL":"",
             ( flags & MMAL_BUFFER_HEADER_FLAG_DECODEONLY) ? ",DECODE":"",
             ( flags & MMAL_BUFFER_HEADER_FLAG_USER0) ? ",USR0":"",
             ( flags & MMAL_BUFFER_HEADER_FLAG_USER1) ? ",USR0":"",
             ( flags & MMAL_BUFFER_HEADER_FLAG_USER2) ? ",USR0":"" );
}


void dump_mmal_buffer(int id, const MMAL_BUFFER_HEADER_T *buf )
{
    char strbuf[512];
    strbuf[0] = 0;

    dump_buffer_flag( strbuf, sizeof(strbuf),buf->flags);

    DLOG_FORMAT("%d: FRAME (%s:%d)(%s): size %d,off:%d",
                id, strbuf, buf->flags,
                (buf->flags&MMAL_BUFFER_HEADER_FLAG_FRAME_END)?"":"SEG",
                buf->length, buf->offset );
    fflush(stdout);
}

