/*
Copyright (c) 2019, rpi-webrtc-streamer Lyu,KeunChang

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

import ConfigParams from './config_params.mjs';
import {kTypeIntegerRange, kTypeIntegerItem, kTypeBoolean, kTypeString,
    kTypeStringItem, kTypeVideoResolution, kTypeVideoResolutionList,
    kType, kDefaultValue, kValue, kMin, kMax, kValidValueList } from './config_params.mjs';

// Each config item key for the media configuration
// and the default value of the item
const config_media_params =  {
    // max_bitrate: { type: kTypeIntegerRange, default_value: 3500000, max: 17000000, min: 200 },
    // resolution_4_3_enable: { type: kTypeBoolean, default_value: true},
    video_rotation: {type: kTypeIntegerItem, default_value: 0, valid_value_list: [0, 90, 180, 270] },
    video_hflip: { type: kTypeBoolean, default_value: false},
    video_vflip: { type: kTypeBoolean, default_value: false},
    // use_dynamic_video_resolution: { type: kTypeBoolean, default_value: true},
    // use_dynamic_video_fps: { type: kTypeBoolean, default_value: true},
    // fixed_video_resolution: { type: kTypeVideoResolution, default_value: '640x480'},
    // fixed_video_fps: { type: kTypeIntegerRange, default_value: 30 , max: 30, min: 5 },
    // video_resolution_list_4_3: { type: kTypeVideoResolutionList,
    //     default_value: '320x240,400x300,512x384,640x480,1024x768,1152x864,1296x972,1640x1232' },
    // video_resolution_list_16_9: { type: kTypeVideoResolutionList,
    //     default_value: '384x216,512x288,640x360,768x432,896x504,1024x576,1152x648,1280x720,1408x864,1920x1080' },
    audio_processing_enable: { type: kTypeBoolean, default_value: false},
    audio_echo_cancel: { type: kTypeBoolean, default_value: true},
    audio_gain_control: { type: kTypeBoolean, default_value: true},
    audio_highpass_filter: { type: kTypeBoolean, default_value: true},
    audio_level_control: { type: kTypeBoolean, default_value: true},
    audio_noise_suppression: { type: kTypeBoolean, default_value: true},
    video_sharpness: { type: kTypeIntegerRange, default_value: 0, max: 100, min: -100 },
    video_contrast: { type: kTypeIntegerRange, default_value: 0, max: 100, min: -100 },
    video_brightness: { type: kTypeIntegerRange, default_value: 50, max: 100, min: 0 },
    video_saturation: { type: kTypeIntegerRange, default_value: 0, max: 100, min: -100 },
    video_ev: { type: kTypeIntegerRange, default_value: 0, max: 10, min: -10 },
    video_exposure_mode: {type: kTypeStringItem,
        default_value: 'auto', valid_value_list: [ 'off', 'auto', 'night',
        'nightpreview', 'backlight', 'spotlight', 'sports', 'snow', 'beach',
        'verylong', 'fixedfps', 'antishake', 'fireworks' ] },
    video_flicker_mode: {type: kTypeStringItem,
        default_value: 'auto', valid_value_list: [ 'off', 'auto', '50hz', '60hz' ] },
    video_awb_mode: {type: kTypeStringItem,
        default_value: 'auto', valid_value_list: [ 'off', 'auto', 'sun',
        'cloud', 'shade', 'tungsten', 'fluorescent', 'incandescent',
        'flash', 'horizon' ] },
    video_drc_mode: {type: kTypeStringItem,
        default_value: 'high', valid_value_list: [ 'off', 'low', 'med', 'high' ] },
    // video_stabilisation: { type: kTypeBoolean, default_value: false},
    video_enable_annotate_text: { type: kTypeBoolean, default_value: false},
    video_annotate_text_size_ratio: { type: kTypeIntegerRange,
        default_value: 3, max: 10, min: 2 },
    video_annotate_text: { type: kTypeString ,
        default_value: '%Y-%m-%d.%X', min: 10, max:128}
};


////////////////////////////////////////////////////////////////////////////////
//
// media configuration helper class
//
////////////////////////////////////////////////////////////////////////////////
class ConfigMedia extends ConfigParams {
    constructor(){
        super(config_media_params);
        this._element_container = {};
    }

    dumpConfigParams () {
        console.log(config_media_params);
    }

    configHandler () {
        for (let elem_key of Object.keys(this._element_container)) {
            // the type of element value is always one type of string
            if( this._element_container[elem_key].value !=
                    this.get(elem_key).toString()) {
        console.log('Element value changed: ' + elem_key +
                '( ' + this._element_container[elem_key].value + ', ' +
                    this.get(elem_key) + ')');
                this.setWithTypeConvert( elem_key,
                    this._element_container[elem_key].value );
                console.log('JSON: ' + JSON.stringify(this.getJsonConfig()));
            }
        }
    }

    resetToDefault () {
        for (let elem_key of Object.keys(this._element_container)) {
            this._element_container[elem_key].value
                = this.reset(elem_key);
            // setting element display value
            let element_disp = document.getElementById(elem_key + '_disp');
            if( element_disp )  {
                element_disp.value  = this._element_container[elem_key].value;
            }
        }
    }

    reloadConfigData () {
        for (let elem_key of Object.keys(this._element_container)) {
            this._element_container[elem_key].value
                = this.get(elem_key);
            // setting element display value
            let element_disp = document.getElementById(elem_key + '_disp');
            if( element_disp )  {
                element_disp.value  = this._element_container[elem_key].value;
            }
        }
    }

    getJsonConfigElement () {
        let json_config = {};
        for (let param_key in this._element_container ) {
            json_config[param_key] = this._config_params[param_key][kValue];
        };
        return json_config;
    }

    hookConfigElement () {
        let element;
        for (let param_key in this._config_params ) {
            element = document.getElementById(param_key);
            if( element ) {
                this._element_container[param_key] = element;

                // updating range type of input node
                if( element.nodeName === 'INPUT' &&
                        element.type === 'range' ){
                    // update the input type element attributes
                    // from config_media
                    this._updateInputRangeElement(element,
                            this._config_params[param_key] );
                }
                else if( element.nodeName === 'SELECT' ) {
                    // update the select element attributes from config_media
                    this._updateSelectElement(element,
                            this._config_params[param_key] );
                }
                // update onchange event handler
                element.onchange =  this.configHandler.bind(this);
            }
        };
        console.log('Config Elements ' +
                Object.keys(this._element_container).length
                + ' added from HTML document : ', this._element_container );
    }

    _updateInputRangeElement(element, params) {
        if( element.nodeName === 'INPUT' &&
                element.type === 'range' &&
                element.configMedia === undefined ) {
            // create update mark attribute
            element.configMedia = true;

            // updating node attributes
            element.min = params[kMin];
            element.max = params[kMax];
            element.value = params[kValue];

            // setting element display value
            let element_disp = document.getElementById(element.id + '_disp');
            if( element_disp )  {
                element_disp.value  = params[kValue];
            }
        }
        else {
            console.error('object is not range type of INPUT node');
        }
    }

    _updateSelectElement(element, params) {
        if( element.nodeName === 'SELECT' &&
                element.configMedia === undefined ) {

            // create update mark attribute
            element.configMedia = true;
            // updating node attributes
            element.value = params[kValue];

            // search first option node in element
            let child = element.firstChild;
            let clonedChild;
            while( child !== null )  {
                if( child.nodeName === 'OPTION' ) {
                    clonedChild = child.cloneNode(true);
                    break;
                }
                child = child.nextSibling;
            }

            // replace select options with params options
            if( params.type === kTypeBoolean ) {
                // remove select options
                while( element.length) {
                    element.remove(0);
                }

                // adding enable_option
                let enable_option = clonedChild.cloneNode(true);
                enable_option.value = true;
                enable_option.innerText = 'Enable';
                element.add( enable_option);

                // adding disable_option
                let disable_option = clonedChild.cloneNode(true);
                disable_option.value = false;
                disable_option.innerText = 'Disable';
                element.add(disable_option);
            }
            else if( params.type === kTypeIntegerItem ) {
                // remove all of select options
                while( element.length) {
                    element.remove(0);
                }
                params[kValidValueList].forEach((item) => {
                    let option_item = clonedChild.cloneNode(true);
                    option_item.value = item;
                    option_item.innerText = item.toString();
                    element.add(option_item);
                });
            }
            else if( params.type === kTypeStringItem ) {
                // remove all of select options
                while( element.length) {
                    element.remove(0);
                }
                params[kValidValueList].forEach((item) => {
                    let option_item = clonedChild.cloneNode(true);
                    option_item.value = item;
                    option_item.innerText = item;
                    element.add(option_item);
                });
            }
            element.value = params[kValue];
        }
        else {
            console.error('object is not SELECT node');
        }
    }

    print() {
        console.log(this._config_params );
    }
}

// module.exports = ConfigMedia;
export default ConfigMedia;

