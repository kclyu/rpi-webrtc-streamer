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

// config type string constant
export const kTypeIntegerRange = 'Integer_Range';
export const kTypeIntegerItem= 'Integer_Item';
export const kTypeBoolean= 'Boolean';
export const kTypeString= 'String';
export const kTypeStringItem= 'String_Item';
export const kTypeVideoResolution= 'VideoResolution';
export const kTypeVideoResolutionList= 'VideoResolutionList';

export const kType='type';
export const kDefaultValue='default_value';
export const kValue='current_value';
export const kMin='min';
export const kMax='max';
export const kValidValueList='valid_value_list';

class ConfigParams {
    constructor (config_params) {
        // copy configuration params
        this._config_params = Object.assign({}, config_params);
        for (let key in this._config_params) {
            if( this._config_params.hasOwnProperty(key)) {
                let value = this._config_params[key];
                // console.log('Key : ', key, value );
                if( value !== undefined &&
                        value.hasOwnProperty(kDefaultValue) &&
                        value.hasOwnProperty(kType) ) {
                    value[kValue] = value.default_value;
                }
                else
                    throw new Error('Required key does not exist');
            } else {
                throw new Error('Parameter value does not exist');
            }
        }
    }

    _validateBoolean(params, key, value) {
        if( typeof value !== 'boolean' )
            throw new Error('Parameter value is not boolean');
        return true;
    }

    _validateIntegerRange(params, key, value) {
        if( typeof value !== 'number' )
            throw new Error('Parameter value is not number');
        if( params[kMin] === false || params[kMax] === false )
            throw new Error('Required parameter key(min/max) does not exist');
        if( params[kMin] > value || params[kMax] < value )
            return false;
        return true;
    }

    _validateIntegerItem(params, key, value) {
        if( typeof value !== 'number' )
            throw new Error('Parameter value is not number');
        if( params[kValidValueList] === false ||
                Array.isArray(params[kValidValueList]) === false )
            throw new Error('Required parameter key(valid_value_list) is invalid');
        // if( params[kValidValueList].indexOf(value) === -1) {
        if( params[kValidValueList].indexOf(value) === -1) {
            return false;
        }
        return true;
    }

    _validateString(params, key, value) {
        if( typeof value !== 'string' )
            throw new Error('Parameter value is not string');
        if( params[kMin] === false || params[kMax] === false )
            return new Error('Required parameter key(min/max) does not exist');
        if( params[kMin] > value.length || params[kMax] < value.length )
            return false;
        return true;
    }

    _validateStringItem(params, key, value) {
        if( typeof value !== 'string' )
            throw new Error('Parameter value is not string');
        if( params[kValidValueList] === false )
            return new Error('Required parameter key(valid_value_list) does not exist');
        if( params[kValidValueList].indexOf(value) === -1)
            return false;
        return true;
    }

    _validateVideoResolution(value) {
        if( typeof value !== 'string' )
            throw new Error('Parameter value is not string');
        let resolution = value.split('x');
        if( resolution.length !== 2 )
            throw new Error('Parameter value is not Video Resolution format: ' + value);
        if( isNaN(Number(resolution[0])) || isNaN(Number(resolution[1])) )
            return false;
        return true;
    }

    _validateVideoResolutionList(value) {
        if( typeof value !== 'string' )
            throw new Error('Parameter value is not string');
        let resolution_list = value.split(',');
        for(let res of resolution_list ) {
            if( this._validateVideoResolution(res) !== true )
                throw new Error('Parameter value is not Video Resolution list format: ' + value);
        }
        return true;
    }

    _setConfigItem (key, value) {
        //  Make sure the config item dictionary has the same key first.
        if( this._config_params.hasOwnProperty(key) === false ) { // true
            console.log('Key: ', key, ' is undefined');
            return false;
        }

        switch( this._config_params[key][kType] ) {
            case kTypeIntegerRange:
                if( this._validateIntegerRange(this._config_params[key],
                            key, value ) !== true ) {
                    return new Error(`Invalid IntegerRange ${key} value ${value}`);
                }
                break;
            case kTypeIntegerItem:
                if( this._validateIntegerItem(this._config_params[key],
                            key, value ) !== true ) {
                    return new Error(`Invalid IntegerItem ${key} value ${value}`);
                }
                break;
            case kTypeBoolean:
                if( this._validateBoolean(this._config_params[key],
                            key, value ) !== true ) {
                    return new Error(`Invalid Boolean ${key} value ${value}`);
                }
                break;
            case kTypeString:
                if( this._validateString(this._config_params[key],
                            key, value ) !== true ) {
                    return new Error(`Invalid String ${key} value ${value}`);
                }
                break;
            case kTypeStringItem:
                if( this._validateStringItem(this._config_params[key],
                            key, value ) !== true ) {
                    return new Error(`Invalid StringItem ${key} value ${value}`);
                }
                break;
            case kTypeVideoResolution:
                if( this._validateVideoResolution( value ) !== true ) {
                    return new Error(`Invalid VideoResolution ${key} value ${value}`);
                }
                break;
            case kTypeVideoResolutionList:
                if( this._validateVideoResolutionList( value ) !== true ) {
                    return new Error(`Invalid VideoResolutionList ${key} value ${value}`);
                }
                break;
            default:
                throw new Error(`Unknown config ${key} type ${kType}`);
        }

        this._config_params[key][kValue] = value;
    }

    setJsonConfig (json_config) {
        for (let item_key of Object.keys(json_config)) {
            let set_config_result;
            if( this._config_params[item_key] === undefined )
                console.log('Key: ', item_key, ' is undefined');

            if( this._config_params[item_key] === undefined ||
                    this._config_params[item_key] === false  )
                return new Error(`Config Key ${item_key} does not exist`);

            if( this._config_params[item_key][kType] === undefined )  {
                console.log(`Invalid ${item_key} value ${json_config[item_key]}`);
            }
            set_config_result = this._setConfigItem( item_key, json_config[item_key] );
            if( set_config_result instanceof Error )
                return set_config_result;
        }
        return true;
    }

    get (key) {
        if( this.isKeyExist(key))
            return this._config_params[key][kValue];
        else
            throw new Error(`Unknown config ${item_key} `);
    }

    reset(key) {
        if( this.isKeyExist(key)) {
            this._config_params[key][kValue] = 
                this._config_params[key][kDefaultValue];
            return this._config_params[key][kDefaultValue];
        }
        else
            throw new Error(`Unknown config ${item_key} `);
    }

    set (key, value) {
        return this._setConfigItem (key, value);
    }

    setWithTypeConvert (key, value) {
        let value_with_type;
        switch( this._config_params[key][kType] ) {
            case kTypeIntegerRange:
                value_with_type = Number(value);
                break;
            case kTypeIntegerItem:
                value_with_type = Number(value);
                break;
            case kTypeBoolean:
                value_with_type = (value === 'true');
                break;
            case kTypeString:
            case kTypeStringItem:
            case kTypeVideoResolution:
            case kTypeVideoResolutionList:
                value_with_type = String(value);
                break;
            default:
                throw new Error(`Unknown config ${key} type ${kType}`);
        }
        return this._setConfigItem (key, value_with_type);
    }

    isKeyExist (key) {
        return this._config_params.hasOwnProperty(key);
    }

    isKeyIntegerType (key) {
        return (this._config_params[key][kType] === kTypeIntegerRange )
    }

    getJsonConfig (include_all=false) {
        let json_config = {};
        for (let param_key in this._config_params ) {
            // If the default value differs from the current value,
            // or if the entire config dump option is given,
            // a json dump is performed.
            if( include_all === true ||
                    this._config_params[param_key][kDefaultValue] !==
                        this._config_params[param_key][kValue] ){
                json_config[param_key] = this._config_params[param_key][kValue];
            }
        };
        return json_config;
    }

    getJsonDefault (dump_all=false) {
        let json_config = {};
        for (let param_key in this._config_params ) {
            json_config[param_key] = this._config_params[param_key][kDefaultValue];
        };
        return json_config;
    }

};

export default ConfigParams;



