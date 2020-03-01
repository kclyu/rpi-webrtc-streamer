/*
 *  Lyu,KeunChang
 *
 * This is a stripped down version of the original RaspiCamControl module from
 * the raspberry pi userland-master branch, Original copyright info below
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
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <ctype.h>
#include <memory.h>
#include <stdio.h>

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/vcos/vcos.h"
#include "interface/vmcs_host/vc_vchi_gencmd.h"
#include "raspicli.h"

// clang-format off
#include "raspicamcontrol.h"
// clang-format on

/// Structure to cross reference exposure strings against the MMAL parameter
/// equivalent
static XREF_T exposure_map[] = {
    {"off", MMAL_PARAM_EXPOSUREMODE_OFF},
    {"auto", MMAL_PARAM_EXPOSUREMODE_AUTO},
    {"night", MMAL_PARAM_EXPOSUREMODE_NIGHT},
    {"nightpreview", MMAL_PARAM_EXPOSUREMODE_NIGHTPREVIEW},
    {"backlight", MMAL_PARAM_EXPOSUREMODE_BACKLIGHT},
    {"spotlight", MMAL_PARAM_EXPOSUREMODE_SPOTLIGHT},
    {"sports", MMAL_PARAM_EXPOSUREMODE_SPORTS},
    {"snow", MMAL_PARAM_EXPOSUREMODE_SNOW},
    {"beach", MMAL_PARAM_EXPOSUREMODE_BEACH},
    {"verylong", MMAL_PARAM_EXPOSUREMODE_VERYLONG},
    {"fixedfps", MMAL_PARAM_EXPOSUREMODE_FIXEDFPS},
    {"antishake", MMAL_PARAM_EXPOSUREMODE_ANTISHAKE},
    {"fireworks", MMAL_PARAM_EXPOSUREMODE_FIREWORKS}};

static const int exposure_map_size =
    sizeof(exposure_map) / sizeof(exposure_map[0]);

/// Structure to cross reference awb strings against the MMAL parameter
/// equivalent
static XREF_T awb_map[] = {{"off", MMAL_PARAM_AWBMODE_OFF},
                           {"auto", MMAL_PARAM_AWBMODE_AUTO},
                           {"sun", MMAL_PARAM_AWBMODE_SUNLIGHT},
                           {"cloud", MMAL_PARAM_AWBMODE_CLOUDY},
                           {"shade", MMAL_PARAM_AWBMODE_SHADE},
                           {"tungsten", MMAL_PARAM_AWBMODE_TUNGSTEN},
                           {"fluorescent", MMAL_PARAM_AWBMODE_FLUORESCENT},
                           {"incandescent", MMAL_PARAM_AWBMODE_INCANDESCENT},
                           {"flash", MMAL_PARAM_AWBMODE_FLASH},
                           {"horizon", MMAL_PARAM_AWBMODE_HORIZON}};

static const int awb_map_size = sizeof(awb_map) / sizeof(awb_map[0]);

/// Structure to cross reference image effect against the MMAL parameter
/// equivalent
static XREF_T imagefx_map[] = {
    {"none", MMAL_PARAM_IMAGEFX_NONE},
    {"negative", MMAL_PARAM_IMAGEFX_NEGATIVE},
    {"solarise", MMAL_PARAM_IMAGEFX_SOLARIZE},
    {"sketch", MMAL_PARAM_IMAGEFX_SKETCH},
    {"denoise", MMAL_PARAM_IMAGEFX_DENOISE},
    {"emboss", MMAL_PARAM_IMAGEFX_EMBOSS},
    {"oilpaint", MMAL_PARAM_IMAGEFX_OILPAINT},
    {"hatch", MMAL_PARAM_IMAGEFX_HATCH},
    {"gpen", MMAL_PARAM_IMAGEFX_GPEN},
    {"pastel", MMAL_PARAM_IMAGEFX_PASTEL},
    {"watercolour", MMAL_PARAM_IMAGEFX_WATERCOLOUR},
    {"film", MMAL_PARAM_IMAGEFX_FILM},
    {"blur", MMAL_PARAM_IMAGEFX_BLUR},
    {"saturation", MMAL_PARAM_IMAGEFX_SATURATION},
    {"colourswap", MMAL_PARAM_IMAGEFX_COLOURSWAP},
    {"washedout", MMAL_PARAM_IMAGEFX_WASHEDOUT},
    {"posterise", MMAL_PARAM_IMAGEFX_POSTERISE},
    {"colourpoint", MMAL_PARAM_IMAGEFX_COLOURPOINT},
    {"colourbalance", MMAL_PARAM_IMAGEFX_COLOURBALANCE},
    {"cartoon", MMAL_PARAM_IMAGEFX_CARTOON}};

static const int imagefx_map_size =
    sizeof(imagefx_map) / sizeof(imagefx_map[0]);

static XREF_T metering_mode_map[] = {
    {"average", MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE},
    {"spot", MMAL_PARAM_EXPOSUREMETERINGMODE_SPOT},
    {"backlit", MMAL_PARAM_EXPOSUREMETERINGMODE_BACKLIT},
    {"matrix", MMAL_PARAM_EXPOSUREMETERINGMODE_MATRIX}};

static const int metering_mode_map_size =
    sizeof(metering_mode_map) / sizeof(metering_mode_map[0]);

/// Structure to cross reference flicker avoid strings against the MMAL
/// parameter equivalent

static XREF_T flicker_avoid_map[] = {{"off", MMAL_PARAM_FLICKERAVOID_OFF},
                                     {"auto", MMAL_PARAM_FLICKERAVOID_AUTO},
                                     {"50hz", MMAL_PARAM_FLICKERAVOID_50HZ},
                                     {"60hz", MMAL_PARAM_FLICKERAVOID_60HZ}};

static const int flicker_avoid_map_size =
    sizeof(flicker_avoid_map) / sizeof(flicker_avoid_map[0]);

static XREF_T drc_mode_map[] = {{"off", MMAL_PARAMETER_DRC_STRENGTH_OFF},
                                {"low", MMAL_PARAMETER_DRC_STRENGTH_LOW},
                                {"med", MMAL_PARAMETER_DRC_STRENGTH_MEDIUM},
                                {"high", MMAL_PARAMETER_DRC_STRENGTH_HIGH}};

static const int drc_mode_map_size =
    sizeof(drc_mode_map) / sizeof(drc_mode_map[0]);

static XREF_T stereo_mode_map[] = {
    {"off", MMAL_STEREOSCOPIC_MODE_NONE},
    {"sbs", MMAL_STEREOSCOPIC_MODE_SIDE_BY_SIDE},
    {"tb", MMAL_STEREOSCOPIC_MODE_TOP_BOTTOM},
};

static const int stereo_mode_map_size =
    sizeof(stereo_mode_map) / sizeof(stereo_mode_map[0]);

#define zoom_full_16P16 ((unsigned int)(65536 * 0.15))
#define zoom_increment_16P16 (65536UL / 10)

/**
 * Convert string to the MMAL parameter for exposure mode
 * @param str Incoming string to match
 * @return MMAL parameter matching the string, or the AUTO option if no match
 * found
 */
MMAL_PARAM_EXPOSUREMODE_T exposure_mode_from_string(const char *str) {
    int i = raspicli_map_xref(str, exposure_map, exposure_map_size);

    if (i != -1) return (MMAL_PARAM_EXPOSUREMODE_T)i;

    vcos_log_error("Unknown exposure mode: %s", str);
    return MMAL_PARAM_EXPOSUREMODE_AUTO;
}

/**
 * Convert string to the MMAL parameter for flicker avoid mode
 * @param str Incoming string to match
 * @return MMAL parameter matching the string, or the AUTO option if no match
 * found
 */
MMAL_PARAM_FLICKERAVOID_T flicker_avoid_mode_from_string(const char *str) {
    int i = raspicli_map_xref(str, flicker_avoid_map, flicker_avoid_map_size);

    if (i != -1) return (MMAL_PARAM_FLICKERAVOID_T)i;

    vcos_log_error("Unknown flicker avoid mode: %s", str);
    return MMAL_PARAM_FLICKERAVOID_OFF;
}

/**
 * Convert string to the MMAL parameter for AWB mode
 * @param str Incoming string to match
 * @return MMAL parameter matching the string, or the AUTO option if no match
 * found
 */
MMAL_PARAM_AWBMODE_T awb_mode_from_string(const char *str) {
    int i = raspicli_map_xref(str, awb_map, awb_map_size);

    if (i != -1) return (MMAL_PARAM_AWBMODE_T)i;

    vcos_log_error("Unknown awb mode: %s", str);
    return MMAL_PARAM_AWBMODE_AUTO;
}

/**
 * Convert string to the MMAL parameter for image effects mode
 * @param str Incoming string to match
 * @return MMAL parameter matching the strong, or the AUTO option if no match
 * found
 */
MMAL_PARAM_IMAGEFX_T imagefx_mode_from_string(const char *str) {
    int i = raspicli_map_xref(str, imagefx_map, imagefx_map_size);

    if (i != -1) return (MMAL_PARAM_IMAGEFX_T)i;

    vcos_log_error("Unknown image fx: %s", str);
    return MMAL_PARAM_IMAGEFX_NONE;
}

/**
 * Convert string to the MMAL parameter for exposure metering mode
 * @param str Incoming string to match
 * @return MMAL parameter matching the string, or the AUTO option if no match
 * found
 */
MMAL_PARAM_EXPOSUREMETERINGMODE_T metering_mode_from_string(const char *str) {
    int i = raspicli_map_xref(str, metering_mode_map, metering_mode_map_size);

    if (i != -1) return (MMAL_PARAM_EXPOSUREMETERINGMODE_T)i;

    vcos_log_error("Unknown metering mode: %s", str);
    return MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE;
}

/**
 * Convert string to the MMAL parameter for DRC level
 * @param str Incoming string to match
 * @return MMAL parameter matching the string, or the AUTO option if no match
 * found
 */
MMAL_PARAMETER_DRC_STRENGTH_T drc_mode_from_string(const char *str) {
    int i = raspicli_map_xref(str, drc_mode_map, drc_mode_map_size);

    if (i != -1) return (MMAL_PARAMETER_DRC_STRENGTH_T)i;

    vcos_log_error("Unknown DRC level: %s", str);
    return MMAL_PARAMETER_DRC_STRENGTH_OFF;
}

/**
 * Convert string to the MMAL parameter for exposure metering mode
 * @param str Incoming string to match
 * @return MMAL parameter matching the string, or the AUTO option if no match
 * found
 */
MMAL_STEREOSCOPIC_MODE_T stereo_mode_from_string(const char *str) {
    int i = raspicli_map_xref(str, stereo_mode_map, stereo_mode_map_size);

    if (i != -1) return (MMAL_STEREOSCOPIC_MODE_T)i;

    vcos_log_error("Unknown metering mode: %s", str);
    return MMAL_STEREOSCOPIC_MODE_NONE;
}

/**
 * Dump contents of camera parameter structure to stderr for debugging/verbose
 * logging
 *
 * @param params Const pointer to parameters structure to dump
 */
void raspicamcontrol_dump_parameters(const RASPICAM_CAMERA_PARAMETERS *params) {
    const char *exp_mode = raspicli_unmap_xref(params->exposureMode,
                                               exposure_map, exposure_map_size);
    const char *fl_mode = raspicli_unmap_xref(
        params->flickerAvoidMode, flicker_avoid_map, flicker_avoid_map_size);
    const char *awb_mode =
        raspicli_unmap_xref(params->awbMode, awb_map, awb_map_size);
    const char *image_effect =
        raspicli_unmap_xref(params->imageEffect, imagefx_map, imagefx_map_size);
    const char *metering_mode = raspicli_unmap_xref(
        params->exposureMeterMode, metering_mode_map, metering_mode_map_size);

    fprintf(stderr, "Sharpness %d, Contrast %d, Brightness %d\n",
            params->sharpness, params->contrast, params->brightness);
    fprintf(stderr,
            "Saturation %d, ISO %d, Video Stabilisation %s, Exposure "
            "compensation %d\n",
            params->saturation, params->ISO,
            params->videoStabilisation ? "Yes" : "No",
            params->exposureCompensation);
    fprintf(stderr, "Exposure Mode '%s', AWB Mode '%s', Image Effect '%s'\n",
            exp_mode, awb_mode, image_effect);
    fprintf(stderr, "Flicker Avoid Mode '%s'\n", fl_mode);
    fprintf(
        stderr,
        "Metering Mode '%s', Colour Effect Enabled %s with U = %d, V = %d\n",
        metering_mode, params->colourEffects.enable ? "Yes" : "No",
        params->colourEffects.u, params->colourEffects.v);
    fprintf(stderr, "Rotation %d, hflip %s, vflip %s\n", params->rotation,
            params->hflip ? "Yes" : "No", params->vflip ? "Yes" : "No");
    fprintf(stderr, "ROI x %lf, y %f, w %f h %f\n", params->roi.x,
            params->roi.y, params->roi.w, params->roi.h);
}

/**
 * Give the supplied parameter block a set of default values
 * @params Pointer to parameter block
 */
void raspicamcontrol_set_defaults(RASPICAM_CAMERA_PARAMETERS *params) {
    vcos_assert(params);

    params->sharpness = 0;
    params->contrast = 0;
    params->brightness = 50;
    params->saturation = 0;
    params->ISO = 0;  // 0 = auto
    params->videoStabilisation = 0;
    params->exposureCompensation = 0;
    params->flickerAvoidMode = MMAL_PARAM_FLICKERAVOID_OFF;
    params->exposureMode = MMAL_PARAM_EXPOSUREMODE_AUTO;
    params->exposureMeterMode = MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE;
    params->awbMode = MMAL_PARAM_AWBMODE_AUTO;
    params->imageEffect = MMAL_PARAM_IMAGEFX_NONE;
    params->colourEffects.enable = 0;
    params->colourEffects.u = 128;
    params->colourEffects.v = 128;
    params->rotation = 0;
    params->hflip = params->vflip = 0;
    params->roi.x = params->roi.y = 0.0;
    params->roi.w = params->roi.h = 1.0;
    params->shutter_speed = 0;  // 0 = auto
    params->awb_gains_r = 0;    // Only have any function if AWB OFF is used.
    params->awb_gains_b = 0;
    params->drc_level = MMAL_PARAMETER_DRC_STRENGTH_OFF;
    params->stats_pass = MMAL_FALSE;
    params->enable_annotate = 0;
    params->annotate_string[0] = '\0';
    params->annotate_text_size = 0;        // Use firmware default
    params->annotate_text_size_ratio = 0;  // disable annotation text ratio
    params->annotate_text_colour = -1;     // Use firmware default
    params->annotate_bg_colour = -1;       // Use firmware default
    params->stereo_mode.mode = MMAL_STEREOSCOPIC_MODE_NONE;
    params->stereo_mode.decimate = MMAL_FALSE;
    params->stereo_mode.swap_eyes = MMAL_FALSE;
}

/**
 * Get all the current camera parameters from specified camera component
 * @param camera Pointer to camera component
 * @param params Pointer to parameter block to accept settings
 * @return 0 if successful, non-zero if unsuccessful
 */
int raspicamcontrol_get_all_parameters(MMAL_COMPONENT_T *camera,
                                       RASPICAM_CAMERA_PARAMETERS *params) {
    vcos_assert(camera);
    vcos_assert(params);

    if (!camera || !params) return 1;

    /* TODO : Write these get functions
       params->sharpness = raspicamcontrol_get_sharpness(camera);
       params->contrast = raspicamcontrol_get_contrast(camera);
       params->brightness = raspicamcontrol_get_brightness(camera);
       params->saturation = raspicamcontrol_get_saturation(camera);
       params->ISO = raspicamcontrol_get_ISO(camera);
       params->videoStabilisation =
       raspicamcontrol_get_video_stabilisation(camera);
       params->exposureCompensation =
       raspicamcontrol_get_exposure_compensation(camera); params->exposureMode =
       raspicamcontrol_get_exposure_mode(camera); params->flickerAvoidMode =
       raspicamcontrol_get_flicker_avoid_mode(camera); params->awbMode =
       raspicamcontrol_get_awb_mode(camera); params->imageEffect =
       raspicamcontrol_get_image_effect(camera); params->colourEffects =
       raspicamcontrol_get_colour_effect(camera); params->thumbnailConfig =
       raspicamcontrol_get_thumbnail_config(camera);
    */
    return 0;
}

/**
 * Set the specified camera to all the specified settings
 * @param camera Pointer to camera component
 * @param params Pointer to parameter block containing parameters
 * @return 0 if successful, none-zero if unsuccessful.
 */
int raspicamcontrol_set_all_parameters(
    MMAL_COMPONENT_T *camera, const RASPICAM_CAMERA_PARAMETERS *params) {
    int result;

    result = raspicamcontrol_set_saturation(camera, params->saturation);
    result += raspicamcontrol_set_sharpness(camera, params->sharpness);
    result += raspicamcontrol_set_contrast(camera, params->contrast);
    result += raspicamcontrol_set_brightness(camera, params->brightness);
    result += raspicamcontrol_set_ISO(camera, params->ISO);
    result += raspicamcontrol_set_video_stabilisation(
        camera, params->videoStabilisation);
    result += raspicamcontrol_set_exposure_compensation(
        camera, params->exposureCompensation);
    result += raspicamcontrol_set_exposure_mode(camera, params->exposureMode);
    result += raspicamcontrol_set_flicker_avoid_mode(camera,
                                                     params->flickerAvoidMode);
    result +=
        raspicamcontrol_set_metering_mode(camera, params->exposureMeterMode);
    result += raspicamcontrol_set_awb_mode(camera, params->awbMode);
    result += raspicamcontrol_set_awb_gains(camera, params->awb_gains_r,
                                            params->awb_gains_b);
    result += raspicamcontrol_set_imageFX(camera, params->imageEffect);
    result += raspicamcontrol_set_colourFX(camera, &params->colourEffects);
    // result += raspicamcontrol_set_thumbnail_parameters(camera,
    // &params->thumbnailConfig);  TODO Not working for some reason
    result += raspicamcontrol_set_rotation(camera, params->rotation);
    result += raspicamcontrol_set_flips(camera, params->hflip, params->vflip);
    result += raspicamcontrol_set_ROI(camera, params->roi);
    result += raspicamcontrol_set_shutter_speed(camera, params->shutter_speed);
    result += raspicamcontrol_set_DRC(camera, params->drc_level);
    result += raspicamcontrol_set_stats_pass(camera, params->stats_pass);
    result += raspicamcontrol_set_annotate(
        camera, params->enable_annotate, params->annotate_string,
        params->annotate_text_size, params->annotate_text_colour,
        params->annotate_bg_colour, params->annotate_justify,
        params->annotate_x, params->annotate_y);

    return result;
}

/**
 * Adjust the saturation level for images
 * @param camera Pointer to camera component
 * @param saturation Value to adjust, -100 to 100
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_saturation(MMAL_COMPONENT_T *camera, int saturation) {
    int ret = 0;

    if (!camera) return 1;

    if (saturation >= -100 && saturation <= 100) {
        MMAL_RATIONAL_T value = {saturation, 100};
        ret = mmal_status_to_int(mmal_port_parameter_set_rational(
            camera->control, MMAL_PARAMETER_SATURATION, value));
    } else {
        vcos_log_error("Invalid saturation value");
        ret = 1;
    }

    return ret;
}

/**
 * Set the sharpness of the image
 * @param camera Pointer to camera component
 * @param sharpness Sharpness adjustment -100 to 100
 */
int raspicamcontrol_set_sharpness(MMAL_COMPONENT_T *camera, int sharpness) {
    int ret = 0;

    if (!camera) return 1;

    if (sharpness >= -100 && sharpness <= 100) {
        MMAL_RATIONAL_T value = {sharpness, 100};
        ret = mmal_status_to_int(mmal_port_parameter_set_rational(
            camera->control, MMAL_PARAMETER_SHARPNESS, value));
    } else {
        vcos_log_error("Invalid sharpness value");
        ret = 1;
    }

    return ret;
}

/**
 * Set the contrast adjustment for the image
 * @param camera Pointer to camera component
 * @param contrast Contrast adjustment -100 to  100
 * @return
 */
int raspicamcontrol_set_contrast(MMAL_COMPONENT_T *camera, int contrast) {
    int ret = 0;

    if (!camera) return 1;

    if (contrast >= -100 && contrast <= 100) {
        MMAL_RATIONAL_T value = {contrast, 100};
        ret = mmal_status_to_int(mmal_port_parameter_set_rational(
            camera->control, MMAL_PARAMETER_CONTRAST, value));
    } else {
        vcos_log_error("Invalid contrast value");
        ret = 1;
    }

    return ret;
}

/**
 * Adjust the brightness level for images
 * @param camera Pointer to camera component
 * @param brightness Value to adjust, 0 to 100
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_brightness(MMAL_COMPONENT_T *camera, int brightness) {
    int ret = 0;

    if (!camera) return 1;

    if (brightness >= 0 && brightness <= 100) {
        MMAL_RATIONAL_T value = {brightness, 100};
        ret = mmal_status_to_int(mmal_port_parameter_set_rational(
            camera->control, MMAL_PARAMETER_BRIGHTNESS, value));
    } else {
        vcos_log_error("Invalid brightness value");
        ret = 1;
    }

    return ret;
}

/**
 * Adjust the ISO used for images
 * @param camera Pointer to camera component
 * @param ISO Value to set TODO :
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_ISO(MMAL_COMPONENT_T *camera, int ISO) {
    if (!camera) return 1;

    return mmal_status_to_int(mmal_port_parameter_set_uint32(
        camera->control, MMAL_PARAMETER_ISO, ISO));
}

/**
 * Adjust the metering mode for images
 * @param camera Pointer to camera component
 * @param saturation Value from following
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE,
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_SPOT,
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_BACKLIT,
 *   - MMAL_PARAM_EXPOSUREMETERINGMODE_MATRIX
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_metering_mode(
    MMAL_COMPONENT_T *camera, MMAL_PARAM_EXPOSUREMETERINGMODE_T m_mode) {
    MMAL_PARAMETER_EXPOSUREMETERINGMODE_T meter_mode = {
        {MMAL_PARAMETER_EXP_METERING_MODE, sizeof(meter_mode)}, m_mode};
    if (!camera) return 1;

    return mmal_status_to_int(
        mmal_port_parameter_set(camera->control, &meter_mode.hdr));
}

/**
 * Set the video stabilisation flag. Only used in video mode
 * @param camera Pointer to camera component
 * @param saturation Flag 0 off 1 on
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_video_stabilisation(MMAL_COMPONENT_T *camera,
                                            int vstabilisation) {
    if (!camera) return 1;

    return mmal_status_to_int(mmal_port_parameter_set_boolean(
        camera->control, MMAL_PARAMETER_VIDEO_STABILISATION, vstabilisation));
}

/**
 * Adjust the exposure compensation for images (EV)
 * @param camera Pointer to camera component
 * @param exp_comp Value to adjust, -10 to +10
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_exposure_compensation(MMAL_COMPONENT_T *camera,
                                              int exp_comp) {
    if (!camera) return 1;

    return mmal_status_to_int(mmal_port_parameter_set_int32(
        camera->control, MMAL_PARAMETER_EXPOSURE_COMP, exp_comp));
}

/**
 * Set exposure mode for images
 * @param camera Pointer to camera component
 * @param mode Exposure mode to set from
 *   - MMAL_PARAM_EXPOSUREMODE_OFF,
 *   - MMAL_PARAM_EXPOSUREMODE_AUTO,
 *   - MMAL_PARAM_EXPOSUREMODE_NIGHT,
 *   - MMAL_PARAM_EXPOSUREMODE_NIGHTPREVIEW,
 *   - MMAL_PARAM_EXPOSUREMODE_BACKLIGHT,
 *   - MMAL_PARAM_EXPOSUREMODE_SPOTLIGHT,
 *   - MMAL_PARAM_EXPOSUREMODE_SPORTS,
 *   - MMAL_PARAM_EXPOSUREMODE_SNOW,
 *   - MMAL_PARAM_EXPOSUREMODE_BEACH,
 *   - MMAL_PARAM_EXPOSUREMODE_VERYLONG,
 *   - MMAL_PARAM_EXPOSUREMODE_FIXEDFPS,
 *   - MMAL_PARAM_EXPOSUREMODE_ANTISHAKE,
 *   - MMAL_PARAM_EXPOSUREMODE_FIREWORKS,
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_exposure_mode(MMAL_COMPONENT_T *camera,
                                      MMAL_PARAM_EXPOSUREMODE_T mode) {
    MMAL_PARAMETER_EXPOSUREMODE_T exp_mode = {
        {MMAL_PARAMETER_EXPOSURE_MODE, sizeof(exp_mode)}, mode};

    if (!camera) return 1;

    return mmal_status_to_int(
        mmal_port_parameter_set(camera->control, &exp_mode.hdr));
}

/**
 * Set flicker avoid mode for images
 * @param camera Pointer to camera component
 * @param mode Exposure mode to set from
 *   - MMAL_PARAM_FLICKERAVOID_OFF,
 *   - MMAL_PARAM_FLICKERAVOID_AUTO,
 *   - MMAL_PARAM_FLICKERAVOID_50HZ,
 *   - MMAL_PARAM_FLICKERAVOID_60HZ,
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_flicker_avoid_mode(MMAL_COMPONENT_T *camera,
                                           MMAL_PARAM_FLICKERAVOID_T mode) {
    MMAL_PARAMETER_FLICKERAVOID_T fl_mode = {
        {MMAL_PARAMETER_FLICKER_AVOID, sizeof(fl_mode)}, mode};

    if (!camera) return 1;

    return mmal_status_to_int(
        mmal_port_parameter_set(camera->control, &fl_mode.hdr));
}

/**
 * Set the aWB (auto white balance) mode for images
 * @param camera Pointer to camera component
 * @param awb_mode Value to set from
 *   - MMAL_PARAM_AWBMODE_OFF,
 *   - MMAL_PARAM_AWBMODE_AUTO,
 *   - MMAL_PARAM_AWBMODE_SUNLIGHT,
 *   - MMAL_PARAM_AWBMODE_CLOUDY,
 *   - MMAL_PARAM_AWBMODE_SHADE,
 *   - MMAL_PARAM_AWBMODE_TUNGSTEN,
 *   - MMAL_PARAM_AWBMODE_FLUORESCENT,
 *   - MMAL_PARAM_AWBMODE_INCANDESCENT,
 *   - MMAL_PARAM_AWBMODE_FLASH,
 *   - MMAL_PARAM_AWBMODE_HORIZON,
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_awb_mode(MMAL_COMPONENT_T *camera,
                                 MMAL_PARAM_AWBMODE_T awb_mode) {
    MMAL_PARAMETER_AWBMODE_T param = {{MMAL_PARAMETER_AWB_MODE, sizeof(param)},
                                      awb_mode};

    if (!camera) return 1;

    return mmal_status_to_int(
        mmal_port_parameter_set(camera->control, &param.hdr));
}

int raspicamcontrol_set_awb_gains(MMAL_COMPONENT_T *camera, float r_gain,
                                  float b_gain) {
    MMAL_PARAMETER_AWB_GAINS_T param = {
        {MMAL_PARAMETER_CUSTOM_AWB_GAINS, sizeof(param)}, {0, 0}, {0, 0}};

    if (!camera) return 1;

    if (!r_gain || !b_gain) return 0;

    param.r_gain.num = (unsigned int)(r_gain * 65536);
    param.b_gain.num = (unsigned int)(b_gain * 65536);
    param.r_gain.den = param.b_gain.den = 65536;
    return mmal_status_to_int(
        mmal_port_parameter_set(camera->control, &param.hdr));
}

/**
 * Set the image effect for the images
 * @param camera Pointer to camera component
 * @param imageFX Value from
 *   - MMAL_PARAM_IMAGEFX_NONE,
 *   - MMAL_PARAM_IMAGEFX_NEGATIVE,
 *   - MMAL_PARAM_IMAGEFX_SOLARIZE,
 *   - MMAL_PARAM_IMAGEFX_POSTERIZE,
 *   - MMAL_PARAM_IMAGEFX_WHITEBOARD,
 *   - MMAL_PARAM_IMAGEFX_BLACKBOARD,
 *   - MMAL_PARAM_IMAGEFX_SKETCH,
 *   - MMAL_PARAM_IMAGEFX_DENOISE,
 *   - MMAL_PARAM_IMAGEFX_EMBOSS,
 *   - MMAL_PARAM_IMAGEFX_OILPAINT,
 *   - MMAL_PARAM_IMAGEFX_HATCH,
 *   - MMAL_PARAM_IMAGEFX_GPEN,
 *   - MMAL_PARAM_IMAGEFX_PASTEL,
 *   - MMAL_PARAM_IMAGEFX_WATERCOLOUR,
 *   - MMAL_PARAM_IMAGEFX_FILM,
 *   - MMAL_PARAM_IMAGEFX_BLUR,
 *   - MMAL_PARAM_IMAGEFX_SATURATION,
 *   - MMAL_PARAM_IMAGEFX_COLOURSWAP,
 *   - MMAL_PARAM_IMAGEFX_WASHEDOUT,
 *   - MMAL_PARAM_IMAGEFX_POSTERISE,
 *   - MMAL_PARAM_IMAGEFX_COLOURPOINT,
 *   - MMAL_PARAM_IMAGEFX_COLOURBALANCE,
 *   - MMAL_PARAM_IMAGEFX_CARTOON,
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_imageFX(MMAL_COMPONENT_T *camera,
                                MMAL_PARAM_IMAGEFX_T imageFX) {
    MMAL_PARAMETER_IMAGEFX_T imgFX = {
        {MMAL_PARAMETER_IMAGE_EFFECT, sizeof(imgFX)}, imageFX};

    if (!camera) return 1;

    return mmal_status_to_int(
        mmal_port_parameter_set(camera->control, &imgFX.hdr));
}

/* TODO :what to do with the image effects parameters?
   MMAL_PARAMETER_IMAGEFX_PARAMETERS_T imfx_param =
{{MMAL_PARAMETER_IMAGE_EFFECT_PARAMETERS,sizeof(imfx_param)}, imageFX, 0, {0}};
mmal_port_parameter_set(camera->control, &imfx_param.hdr);
                             */

/**
 * Set the colour effect  for images (Set UV component)
 * @param camera Pointer to camera component
 * @param colourFX  Contains enable state and U and V numbers to set (e.g.
 * 128,128 = Black and white)
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_colourFX(MMAL_COMPONENT_T *camera,
                                 const MMAL_PARAM_COLOURFX_T *colourFX) {
    MMAL_PARAMETER_COLOURFX_T colfx = {
        {MMAL_PARAMETER_COLOUR_EFFECT, sizeof(colfx)}, 0, 0, 0};

    if (!camera) return 1;

    colfx.enable = colourFX->enable;
    colfx.u = colourFX->u;
    colfx.v = colourFX->v;

    return mmal_status_to_int(
        mmal_port_parameter_set(camera->control, &colfx.hdr));
}

/**
 * Set the rotation of the image
 * @param camera Pointer to camera component
 * @param rotation Degree of rotation (any number, but will be converted to
 * 0,90,180 or 270 only)
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_rotation(MMAL_COMPONENT_T *camera, int rotation) {
    int ret;
    int my_rotation = ((rotation % 360) / 90) * 90;

    ret = mmal_port_parameter_set_int32(camera->output[0],
                                        MMAL_PARAMETER_ROTATION, my_rotation);
    mmal_port_parameter_set_int32(camera->output[1], MMAL_PARAMETER_ROTATION,
                                  my_rotation);
    mmal_port_parameter_set_int32(camera->output[2], MMAL_PARAMETER_ROTATION,
                                  my_rotation);

    return ret;
}

/**
 * Set the flips state of the image
 * @param camera Pointer to camera component
 * @param hflip If true, horizontally flip the image
 * @param vflip If true, vertically flip the image
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_flips(MMAL_COMPONENT_T *camera, int hflip, int vflip) {
    MMAL_PARAMETER_MIRROR_T mirror = {
        {MMAL_PARAMETER_MIRROR, sizeof(MMAL_PARAMETER_MIRROR_T)},
        MMAL_PARAM_MIRROR_NONE};

    if (hflip && vflip)
        mirror.value = MMAL_PARAM_MIRROR_BOTH;
    else if (hflip)
        mirror.value = MMAL_PARAM_MIRROR_HORIZONTAL;
    else if (vflip)
        mirror.value = MMAL_PARAM_MIRROR_VERTICAL;

    mmal_port_parameter_set(camera->output[0], &mirror.hdr);
    mmal_port_parameter_set(camera->output[1], &mirror.hdr);
    return mmal_port_parameter_set(camera->output[2], &mirror.hdr);
}

/**
 * Set the ROI of the sensor to use for captures/preview
 * @param camera Pointer to camera component
 * @param rect   Normalised coordinates of ROI rectangle
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_ROI(MMAL_COMPONENT_T *camera, PARAM_FLOAT_RECT_T rect) {
    MMAL_PARAMETER_INPUT_CROP_T crop = {
        {MMAL_PARAMETER_INPUT_CROP, sizeof(MMAL_PARAMETER_INPUT_CROP_T)}};

    crop.rect.x = (65536 * rect.x);
    crop.rect.y = (65536 * rect.y);
    crop.rect.width = (65536 * rect.w);
    crop.rect.height = (65536 * rect.h);

    return mmal_port_parameter_set(camera->control, &crop.hdr);
}

/**
 * Zoom in and Zoom out by changing ROI
 * @param camera Pointer to camera component
 * @param zoom_command zoom command enum
 * @return 0 if successful, non-zero otherwise
 */
int raspicamcontrol_zoom_in_zoom_out(MMAL_COMPONENT_T *camera,
                                     ZOOM_COMMAND_T zoom_command,
                                     PARAM_FLOAT_RECT_T *roi) {
    MMAL_PARAMETER_INPUT_CROP_T crop;
    crop.hdr.id = MMAL_PARAMETER_INPUT_CROP;
    crop.hdr.size = sizeof(crop);

    if (mmal_port_parameter_get(camera->control, &crop.hdr) != MMAL_SUCCESS) {
        vcos_log_error(
            "mmal_port_parameter_get(camera->control, &crop.hdr) failed, skip "
            "it");
        return 0;
    }

    if (zoom_command == ZOOM_IN) {
        if ((unsigned int)crop.rect.width <=
            (zoom_full_16P16 + zoom_increment_16P16)) {
            crop.rect.width = zoom_full_16P16;
            crop.rect.height = zoom_full_16P16;
        } else {
            crop.rect.width -= zoom_increment_16P16;
            crop.rect.height -= zoom_increment_16P16;
        }
    } else if (zoom_command == ZOOM_OUT) {
        unsigned int increased_size = crop.rect.width + zoom_increment_16P16;
        if (increased_size < (unsigned int)crop.rect.width)  // overflow
        {
            crop.rect.width = 65536;
            crop.rect.height = 65536;
        } else {
            crop.rect.width = increased_size;
            crop.rect.height = increased_size;
        }
    }

    if (zoom_command == ZOOM_RESET) {
        crop.rect.x = 0;
        crop.rect.y = 0;
        crop.rect.width = 65536;
        crop.rect.height = 65536;
    } else {
        unsigned int centered_top_coordinate = (65536 - crop.rect.width) / 2;
        crop.rect.x = centered_top_coordinate;
        crop.rect.y = centered_top_coordinate;
    }

    int ret =
        mmal_status_to_int(mmal_port_parameter_set(camera->control, &crop.hdr));

    if (ret == 0) {
        roi->x = roi->y = (double)crop.rect.x / 65536;
        roi->w = roi->h = (double)crop.rect.width / 65536;
    } else {
        vcos_log_error("Failed to set crop values, x/y: %u, w/h: %u",
                       crop.rect.x, crop.rect.width);
        ret = 1;
    }

    return ret;
}

/**
 * Zoom in and Zoom out by changing ROI with centered coordination
 * @param camera Pointer to camera component
 * @param zoom_command zoom command enum
 * @return 0 if successful, non-zero otherwise
 */
int raspicamcontrol_zoom_with_coordination(MMAL_COMPONENT_T *camera,
                                           ZOOM_COMMAND_T zoom_command,
                                           double cx, double cy,
                                           PARAM_FLOAT_RECT_T *roi) {
    int dx = 0, dy = 0;
    MMAL_PARAMETER_INPUT_CROP_T crop;
    crop.hdr.id = MMAL_PARAMETER_INPUT_CROP;
    crop.hdr.size = sizeof(crop);

    if (mmal_port_parameter_get(camera->control, &crop.hdr) != MMAL_SUCCESS) {
        vcos_log_error(
            "mmal_port_parameter_get(camera->control, &crop.hdr) failed, skip "
            "it");
        return 0;
    }

    if (zoom_command == ZOOM_IN) {
        if ((unsigned int)crop.rect.width <=
            (zoom_full_16P16 + zoom_increment_16P16)) {
            crop.rect.width = zoom_full_16P16;
            crop.rect.height = zoom_full_16P16;
        } else {
            if (cx > 1.0 || cy > 1.0) {
                vcos_log_error("invalid center x/y value: %f, %f", cx, cy);
                return 1;
            }
            dx = (unsigned int)zoom_increment_16P16 * cx;
            dy = (unsigned int)zoom_increment_16P16 * cy;
            crop.rect.x += dx;
            crop.rect.y += dy;
            crop.rect.width -= zoom_increment_16P16;
            crop.rect.height -= zoom_increment_16P16;
        }
    } else if (zoom_command == ZOOM_OUT) {
        unsigned int increased_size = crop.rect.width + zoom_increment_16P16;
        if (increased_size < (unsigned int)crop.rect.width) {
            // overflow
            crop.rect.width = 65536;
            crop.rect.height = 65536;
        } else {
            dx = (unsigned int)zoom_increment_16P16 * 0.5; /* cx not used */
            dy = (unsigned int)zoom_increment_16P16 * 0.5; /* cy not used */
            crop.rect.width = increased_size;
            crop.rect.height = increased_size;
            if (crop.rect.x - dx < 0)
                crop.rect.x = 0;
            else
                crop.rect.x -= dx;
            if (crop.rect.y - dy < 0)
                crop.rect.y = 0;
            else
                crop.rect.y -= dy;
        }
    } else if (zoom_command == ZOOM_RESET) {
        crop.rect.x = 0;
        crop.rect.y = 0;
        crop.rect.width = 65536;
        crop.rect.height = 65536;
    } else if (zoom_command == ZOOM_MOVE) {
        // relative drag x, y
        dx = (unsigned int)crop.rect.width * cx;
        dy = (unsigned int)crop.rect.height * cy;

        if (crop.rect.x + dx < 0) crop.rect.x = 0;
        if ((unsigned int)crop.rect.x + crop.rect.width + dx > 65536)
            crop.rect.x = 65536 - crop.rect.width;
        else
            crop.rect.x = crop.rect.x + dx;

        if (crop.rect.y + dy < 0) crop.rect.y = 0;
        if ((unsigned int)crop.rect.y + crop.rect.height + dy > 65536)
            crop.rect.y = 65536 - crop.rect.height;
        else
            crop.rect.y = crop.rect.y + dy;
    } else {
        vcos_log_error("Unknown Zoom command type");
        return 1;
    }

    int ret =
        mmal_status_to_int(mmal_port_parameter_set(camera->control, &crop.hdr));
    if (ret == 0) {
        roi->x = (double)crop.rect.x / 65536;
        roi->y = (double)crop.rect.y / 65536;
        roi->w = (double)crop.rect.width / 65536;
        roi->h = (double)crop.rect.height / 65536;
    } else {
        vcos_log_error("Failed to set crop values, x/y: %u, w/h: %u",
                       crop.rect.x, crop.rect.width);
        ret = 1;
    }
    return ret;
}

/**
 * Adjust the exposure time used for images
 * @param camera Pointer to camera component
 * @param shutter speed in microseconds
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_shutter_speed(MMAL_COMPONENT_T *camera, int speed) {
    if (!camera) return 1;

    return mmal_status_to_int(mmal_port_parameter_set_uint32(
        camera->control, MMAL_PARAMETER_SHUTTER_SPEED, speed));
}

/**
 * Adjust the Dynamic range compression level
 * @param camera Pointer to camera component
 * @param strength Strength of DRC to apply
 *        MMAL_PARAMETER_DRC_STRENGTH_OFF
 *        MMAL_PARAMETER_DRC_STRENGTH_LOW
 *        MMAL_PARAMETER_DRC_STRENGTH_MEDIUM
 *        MMAL_PARAMETER_DRC_STRENGTH_HIGH
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_DRC(MMAL_COMPONENT_T *camera,
                            MMAL_PARAMETER_DRC_STRENGTH_T strength) {
    MMAL_PARAMETER_DRC_T drc = {{MMAL_PARAMETER_DYNAMIC_RANGE_COMPRESSION,
                                 sizeof(MMAL_PARAMETER_DRC_T)},
                                strength};

    if (!camera) return 1;

    return mmal_status_to_int(
        mmal_port_parameter_set(camera->control, &drc.hdr));
}

int raspicamcontrol_set_stats_pass(MMAL_COMPONENT_T *camera, int stats_pass) {
    if (!camera) return 1;

    return mmal_status_to_int(mmal_port_parameter_set_boolean(
        camera->control, MMAL_PARAMETER_CAPTURE_STATS_PASS, stats_pass));
}

/**
 * Set the annotate data
 * @param camera Pointer to camera component
 * @param Bitmask of required annotation data. 0 for off.
 * @param If set, a pointer to text string to use instead of bitmask, max length
 * 32 characters
 *
 * @return 0 if successful, non-zero if any parameters out of range
 */
int raspicamcontrol_set_annotate(MMAL_COMPONENT_T *camera, const int settings,
                                 const char *string, const int text_size,
                                 const int text_colour, const int bg_colour,
                                 const unsigned int justify,
                                 const unsigned int x, const unsigned int y) {
    MMAL_PARAMETER_CAMERA_ANNOTATE_V3_T annotate = {
        {MMAL_PARAMETER_ANNOTATE, sizeof(MMAL_PARAMETER_CAMERA_ANNOTATE_V3_T)}};

    if (settings) {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        char tmp[MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3];
        int process_datetime = 1;

        annotate.enable = 1;

        if (settings & (ANNOTATE_APP_TEXT | ANNOTATE_USER_TEXT)) {
            if ((settings & (ANNOTATE_TIME_TEXT | ANNOTATE_DATE_TEXT)) &&
                strchr(string, '%') !=
                    NULL) {  // string contains strftime parameter?
                strftime(annotate.text, MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3,
                         string, &tm);
                process_datetime = 0;
            } else {
                strncpy(annotate.text, string,
                        MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3);
            }
            annotate.text[MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3 - 1] = '\0';
        }

        if (process_datetime && (settings & ANNOTATE_TIME_TEXT)) {
            if (strlen(annotate.text)) {
                strftime(tmp, 32, " %X", &tm);
            } else {
                strftime(tmp, 32, "%X", &tm);
            }
            strncat(annotate.text, tmp,
                    MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3 -
                        strlen(annotate.text) - 1);
        }

        if (process_datetime && (settings & ANNOTATE_DATE_TEXT)) {
            if (strlen(annotate.text)) {
                strftime(tmp, 32, " %x", &tm);
            } else {
                strftime(tmp, 32, "%x", &tm);
            }
            strncat(annotate.text, tmp,
                    MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3 -
                        strlen(annotate.text) - 1);
        }

        if (settings & ANNOTATE_SHUTTER_SETTINGS)
            annotate.show_shutter = MMAL_TRUE;

        if (settings & ANNOTATE_GAIN_SETTINGS)
            annotate.show_analog_gain = MMAL_TRUE;

        if (settings & ANNOTATE_LENS_SETTINGS) annotate.show_lens = MMAL_TRUE;

        if (settings & ANNOTATE_CAF_SETTINGS) annotate.show_caf = MMAL_TRUE;

        if (settings & ANNOTATE_MOTION_SETTINGS)
            annotate.show_motion = MMAL_TRUE;

        if (settings & ANNOTATE_FRAME_NUMBER)
            annotate.show_frame_num = MMAL_TRUE;

        if (settings & ANNOTATE_BLACK_BACKGROUND)
            annotate.enable_text_background = MMAL_TRUE;

        annotate.text_size = text_size;

        if (text_colour != -1) {
            annotate.custom_text_colour = MMAL_TRUE;
            annotate.custom_text_Y = text_colour & 0xff;
            annotate.custom_text_U = (text_colour >> 8) & 0xff;
            annotate.custom_text_V = (text_colour >> 16) & 0xff;
        } else
            annotate.custom_text_colour = MMAL_FALSE;

        if (bg_colour != -1) {
            annotate.custom_background_colour = MMAL_TRUE;
            annotate.custom_background_Y = bg_colour & 0xff;
            annotate.custom_background_U = (bg_colour >> 8) & 0xff;
            annotate.custom_background_V = (bg_colour >> 16) & 0xff;
        } else
            annotate.custom_background_colour = MMAL_FALSE;
    } else
        annotate.enable = 0;

    return mmal_status_to_int(
        mmal_port_parameter_set(camera->control, &annotate.hdr));
}

int raspicamcontrol_set_stereo_mode(
    MMAL_PORT_T *port, MMAL_PARAMETER_STEREOSCOPIC_MODE_T *stereo_mode) {
    MMAL_PARAMETER_STEREOSCOPIC_MODE_T stereo = {
        {MMAL_PARAMETER_STEREOSCOPIC_MODE, sizeof(stereo)},
        MMAL_STEREOSCOPIC_MODE_NONE,
        MMAL_FALSE,
        MMAL_FALSE};
    if (stereo_mode->mode != MMAL_STEREOSCOPIC_MODE_NONE) {
        stereo.mode = stereo_mode->mode;
        stereo.decimate = stereo_mode->decimate;
        stereo.swap_eyes = stereo_mode->swap_eyes;
    }
    return mmal_status_to_int(mmal_port_parameter_set(port, &stereo.hdr));
}

int raspicamcontrol_set_gains(MMAL_COMPONENT_T *camera, float analog,
                              float digital) {
    MMAL_RATIONAL_T rational = {0, 65536};
    MMAL_STATUS_T status;

    if (!camera) return 1;

    rational.num = (unsigned int)(analog * 65536);
    status = mmal_port_parameter_set_rational(
        camera->control, MMAL_PARAMETER_ANALOG_GAIN, rational);
    if (status != MMAL_SUCCESS) return mmal_status_to_int(status);

    rational.num = (unsigned int)(digital * 65536);
    status = mmal_port_parameter_set_rational(
        camera->control, MMAL_PARAMETER_DIGITAL_GAIN, rational);
    return mmal_status_to_int(status);
}

/**
 * Asked GPU how much memory it has allocated
 *
 * @return amount of memory in MB
 */
int raspicamcontrol_get_mem_gpu(void) {
    char response[80] = "";
    int gpu_mem = 0;
    if (vc_gencmd(response, sizeof response, "get_mem gpu") == 0)
        vc_gencmd_number_property(response, "gpu", &gpu_mem);
    return gpu_mem;
}

/**
 * Ask GPU about its camera abilities
 * @param supported None-zero if software supports the camera
 * @param detected  None-zero if a camera has been detected
 */
void raspicamcontrol_get_camera(int *supported, int *detected) {
    char response[80] = "";
    if (vc_gencmd(response, sizeof response, "get_camera") == 0) {
        if (supported)
            vc_gencmd_number_property(response, "supported", supported);
        if (detected) vc_gencmd_number_property(response, "detected", detected);
    }
}

/**
 * Check to see if camera is supported, and we have allocated enough meooryAsk
 * GPU about its camera abilities
 * @param supported None-zero if software supports the camera
 * @param detected  None-zero if a camera has been detected
 */
void raspicamcontrol_check_configuration(int min_gpu_mem) {
    int gpu_mem = raspicamcontrol_get_mem_gpu();
    int supported = 0, detected = 0;
    raspicamcontrol_get_camera(&supported, &detected);
    if (!supported)
        vcos_log_error(
            "Camera is not enabled in this build. Try running \"sudo "
            "raspi-config\" and ensure that \"camera\" has been enabled\n");
    else if (gpu_mem < min_gpu_mem)
        vcos_log_error(
            "Only %dM of gpu_mem is configured. Try running \"sudo "
            "raspi-config\" and ensure that \"memory_split\" has a value of %d "
            "or greater\n",
            gpu_mem, min_gpu_mem);
    else if (!detected)
        vcos_log_error(
            "Camera is not detected. Please check carefully the camera module "
            "is installed correctly\n");
    else
        vcos_log_error(
            "Failed to run camera app. Please check for firmware updates\n");
}

/** Default camera callback function
 * Handles the --settings
 * @param port
 * @param Callback data
 */
void default_camera_control_callback(MMAL_PORT_T *port,
                                     MMAL_BUFFER_HEADER_T *buffer) {
    fprintf(stderr, "Camera control callback  cmd=0x%08x", buffer->cmd);

    if (buffer->cmd == MMAL_EVENT_PARAMETER_CHANGED) {
        MMAL_EVENT_PARAMETER_CHANGED_T *param =
            (MMAL_EVENT_PARAMETER_CHANGED_T *)buffer->data;
        switch (param->hdr.id) {
            case MMAL_PARAMETER_CAMERA_SETTINGS: {
                MMAL_PARAMETER_CAMERA_SETTINGS_T *settings =
                    (MMAL_PARAMETER_CAMERA_SETTINGS_T *)param;
                vcos_log_error(
                    "Exposure now %u, analog gain %u/%u, digital gain %u/%u",
                    settings->exposure, settings->analog_gain.num,
                    settings->analog_gain.den, settings->digital_gain.num,
                    settings->digital_gain.den);
                vcos_log_error(
                    "AWB R=%u/%u, B=%u/%u", settings->awb_red_gain.num,
                    settings->awb_red_gain.den, settings->awb_blue_gain.num,
                    settings->awb_blue_gain.den);
            } break;
        }
    } else if (buffer->cmd == MMAL_EVENT_ERROR) {
        vcos_log_error(
            "No data received from sensor. Check all connections, including "
            "the Sunny one on the camera board");
    } else {
        vcos_log_error(
            "Received unexpected camera control callback event, 0x%08x",
            buffer->cmd);
    }

    mmal_buffer_header_release(buffer);
}

/******************************************************************************
 *
 * Helper functions to validate camera setting value
 *
 * Ensure that the config value has the same value as the already defined
 * option mode.
 *
 ******************************************************************************/
int check_optionvalue_exposure_mode(const char *str) {
    int i = raspicli_map_xref(str, exposure_map, exposure_map_size);
    if (i != -1) return 1;
    return 0;
}

int check_optionvalue_flicker_mode(const char *str) {
    int i = raspicli_map_xref(str, flicker_avoid_map, flicker_avoid_map_size);
    if (i != -1) return 1;
    return 0;
}

int check_optionvalue_awb_mode(const char *str) {
    int i = raspicli_map_xref(str, awb_map, awb_map_size);
    if (i != -1) return 1;
    return 0;
}

int check_optionvalue_drc_mode(const char *str) {
    int i = raspicli_map_xref(str, drc_mode_map, drc_mode_map_size);
    if (i != -1) return 1;
    return 0;
}
