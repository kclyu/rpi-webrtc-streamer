#!/usr/bin/env python
# -*- coding: utf-8 -*-

PROG_LICENSE="""
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
"""

PROG_DESC = "Rpi-WebRTC-Streamer Telegram Bot for sending motion detection message"
PROG_VERSION = '0.72'

import os
import sys
import yaml
import json
import pickle
import argparse
import ast
import subprocess
from telegram.ext import Updater, CommandHandler, MessageHandler, Filters
import logging
import logging.handlers
from functools import wraps
from datetime import datetime
from string import Template
import time

#  constants
PROG_CONSTANT_TEMP_PATH_MP4 = '/tmp'

""" --------------------------------------------------------------------------
configuraiton key and default values
"""

# defaut value 
PROG_FILE_CONFIG = '/opt/rws/etc/telegramBot.conf'
PROG_LOG_FILENAME = '/opt/rws/log/telegramBot.log'

PROG_CHAT_ID_FILENAME = '/opt/rws/etc/run/telegramBotChatId.pickle'
PROG_ADMIN_ID_FILENAME = '/opt/rws/etc/run/telegramBotAdminIdList.pickle'
PROG_NOTI_HOUR_SCHEDULE = '[9,10,11,12,13,14,15,16,17,18]'
PROG_NOTI_DAY_SCHEDULE = '[0,1,2,3,4]'

# configuration keys
PROG_CONFIG_KEY_AUTH_TOKEN = 'auth_token'
PROG_CONFIG_KEY_H264_PATH = 'motion_h264_path'
PROG_CONFIG_KEY_CHECKING_INTERVAL = 'checking_interval'
PROG_CONFIG_KEY_MP4_UPLOAD_ENABLE = 'mp4_upload_enable'
PROG_CONFIG_KEY_NOTI_COOLING_PERIOD = 'noti_cooling_period'
PROG_CONFIG_KEY_UPLOAD_TIMEOUT = 'upload_timeout'

PROG_CONFIG_KEY_NO_NOTI_IN_SCHEDULE = 'no_noti_in_schedule'
PROG_CONFIG_KEY_NOTI_HOUR_SCHEDULE = 'noti_schedule_hour'
PROG_CONFIG_KEY_NOTI_DAY_SCHEDULE = 'noti_schedule_day'

PROG_CONFIG_KEY_CHAT_ID_FILENAME = 'chat_id_filename'
PROG_CONFIG_KEY_ADMINS_LIST_FILENAME = 'admin_list_filename'

PROG_CONFIG_KEY_CMD_CONVERTING = 'converting_command'

PROG_CONFIG_KEY_EXTERNAL_COMMAND_ENABLE = 'external_command_enable'
PROG_CONFIG_KEY_VALIDATE_COMMAND = 'validate_command'
PROG_CONFIG_KEY_VALIDATE_JSON_KEY = 'valid_json_key'
PROG_CONFIG_KEY_VALIDATE_JSON_VALUE = 'valid_json_value'
PROG_CONFIG_KEY_SYNC_COMMAND = 'sync_command'

# configuration dict
_PROG_CONFIG= { PROG_CONFIG_KEY_AUTH_TOKEN : '',
        PROG_CONFIG_KEY_H264_PATH : '/opt/rws/motion_captured',
        PROG_CONFIG_KEY_CHECKING_INTERVAL : 5,
        PROG_CONFIG_KEY_MP4_UPLOAD_ENABLE : True,
        PROG_CONFIG_KEY_UPLOAD_TIMEOUT : 120,
        PROG_CONFIG_KEY_NOTI_COOLING_PERIOD : 300,

        PROG_CONFIG_KEY_CMD_CONVERTING : \
                '/opt/rws/tools/ffmpeg_neon -y -i $input_file $output_file',

        PROG_CONFIG_KEY_NO_NOTI_IN_SCHEDULE : False,
        PROG_CONFIG_KEY_NOTI_DAY_SCHEDULE : PROG_NOTI_DAY_SCHEDULE,
        PROG_CONFIG_KEY_NOTI_HOUR_SCHEDULE : PROG_NOTI_HOUR_SCHEDULE,

        PROG_CONFIG_KEY_CHAT_ID_FILENAME :  PROG_CHAT_ID_FILENAME,
        PROG_CONFIG_KEY_ADMINS_LIST_FILENAME :  PROG_ADMIN_ID_FILENAME,

        PROG_CONFIG_KEY_EXTERNAL_COMMAND_ENABLE : False,
        PROG_CONFIG_KEY_VALIDATE_COMMAND  : '/opt/rws/tools/googleDrive.py about --json',
        PROG_CONFIG_KEY_VALIDATE_JSON_KEY  : 'isAuthenticatedUser',
        PROG_CONFIG_KEY_VALIDATE_JSON_VALUE  : 'True',
        PROG_CONFIG_KEY_SYNC_COMMAND  : '/opt/rws/tools/googleDrive.py sync --json'
        }

""" --------------------------------------------------------------------------
Messages definitons
"""

#  message definitions for bot command message
PROG_BOT_CMD_MSG_HELP="""
/start: Starts a new session with this Bot or \
reset the previous session to the session you are using now.
/disable_sound : the notification of motion detection delivered \
without alert sound
/enable_sound : the notification of motion detection with alert sound
/disable_noti : stop the notification of motion detection.
/enable_noti : start the notification of motion detection.
/settings : dispaly current settings of this bot 
"""

PROG_BOT_CMD_MSG_GREETING_0="Hello!!, it's First time to see you."
PROG_BOT_CMD_MSG_GREETING_1="Hi again!, Discard the old session and \
use the new session you are using now."
PROG_BOT_CMD_MSG_GREETING_2="Hi again!, Now, the chat session is \
the same as the previous session. Resetting to a new session is not necessary."

PROG_BOT_CMD_MSG_ALERT_DISABLE="\
From now, Motion detection message will be delivered \
*WITHOUT* sound alert"
PROG_BOT_CMD_MSG_ALERT_ALREADY_DISABLED="\
Already sound alert disabled, Motion detection message will be delivered \
*WITHOUT* sound alert"
PROG_BOT_CMD_MSG_ALERT_ENABLE="\
From now, Motion detection message will be delivered \
*WITH* sound alert"
PROG_BOT_CMD_MSG_ALERT_ALREADY_ENABLED="\
Already sound alert enabled, Motion detection message will be delivered \
*WITH* sound alert"

PROG_BOT_CMD_MSG_NOTI_DISABLE="\
Motion detection message will *not* be delivered"
PROG_BOT_CMD_MSG_NOTI_ALREADY_DISABLED="\
Already noti disabled, Motion detection message will *not* be delivered"
PROG_BOT_CMD_MSG_NOTI_ENABLE="Motion detection message will be *delivered*"
PROG_BOT_CMD_MSG_NOTI_ALREADY_ENABLED="\
Already noti enabled, Motion detection message will be *delivered*"


# message definition for warning and error
PROG_MSG_EXTERNAL_COMMAND_NOT_VALID="""
External Command is not valid or not configured properly, 
Please check if the External Command you want to use is set up properly.
"""

""" --------------------------------------------------------------------------
global variables
"""
_H264_FILE_LIST=[]
_IS_EXTERNAL_COMMAND_VALID=False
_IS_CHAT_ID_LOADED=False
_CHAT_ID=0

_LIST_OF_ADMINS = []
_IS_ADMINS_LIST_LOADED=False

_LOADED_NOTI_SCHEDULE_DAY=[]
_LOADED_NOTI_SCHEDULE_HOUR=[]

_NOTI_DISABLE=False
_ALERT_DISABLE=False

_COOLING_TIME_LAST_NOTI=time.time()  # dummy initialization


""" Enable logging 
"""
logging.basicConfig(format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
                    level=logging.INFO)
logger = logging.getLogger(__name__)

""" --------------------------------------------------------------------------
systemd service template
"""

# Ubuntu
# TELEGRAMBOT_SYSTEMD_FILENAME='/etc/systemd/system/telegramBot.service'
TELEGRAMBOT_SYSTEMD_FILENAME='/lib/systemd/system/telegramBot.service'
TELEGRAMBOT_SYSTEMD_SERVICE="""
[Unit]
Description=Rpi-WebRTC-Streamer TelegamBot to send motion detection message
After=syslog.target

[Service]
Type=simple
PIDFile=/var/run/telegramBot.pid
ExecStart=/opt/rws/tools/telegramBot.py --log /opt/rws/log/telegramBot.log
StandardOutput=journal
StandardError=journal
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target

"""
def install_systemd_service_file():
    try:
        file = open(TELEGRAMBOT_SYSTEMD_FILENAME, 'w')
        file.write(TELEGRAMBOT_SYSTEMD_SERVICE)
        file.close()
    except Exception as e:
        print('Error in opening systemd service file : %s\nException: %s' 
                % ( TELEGRAMBOT_SYSTEMD_FILENAME, e ))
    else:
        print('systemd telegramBot.service file installed successfully.')
                
def remove_systemd_service_file():
    if os.path.isfile(TELEGRAMBOT_SYSTEMD_FILENAME):
        try:
            os.remove(TELEGRAMBOT_SYSTEMD_FILENAME)
        except Exception as e:
            print('Error in remove systemd service file : %s\nException: %s' 
                    % ( TELEGRAMBOT_SYSTEMD_FILENAME, e ))
        else:
            print('systemd telegramBot.service file uninstalled successfully.')
                    

""" --------------------------------------------------------------------------
utility helper functions
"""

def setup_logger(args):
    """ Add the log message handler to the logger
    """
    # setting log level 
    if args.verbose:
        logger.setLevel(logging.DEBUG)

    if args.log:
        try:
            # try to open log file
            handler = logging.handlers.RotatingFileHandler(
                    args.log, maxBytes=2000000, backupCount=9)
        except Exception as e:
            logger.error('Error in opening log file handler: %s' % e )
        else:
            formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
            handler.setFormatter(formatter)
            logger.addHandler(handler)

def load_config(config_filename):
    """ If a config file is specified, load the config file.

    Even though the config file is not specified, we actually use 
    the contents of default config for the _PROG_CONFIG global variable.
    """
    global _PROG_CONFIG, _LOADED_NOTI_SCHEDULE_DAY, _LOADED_NOTI_SCHEDULE_HOUR

    with open(config_filename) as fp:
        loaded_config = yaml.load(fp)
        for key, value in loaded_config.items():
            _PROG_CONFIG[key] = value

    _LOADED_NOTI_SCHEDULE_HOUR = get_noti_schedule(
            _PROG_CONFIG[PROG_CONFIG_KEY_NOTI_HOUR_SCHEDULE],
            PROG_NOTI_HOUR_SCHEDULE)
    _LOADED_NOTI_SCHEDULE_DAY = get_noti_schedule(
            _PROG_CONFIG[PROG_CONFIG_KEY_NOTI_DAY_SCHEDULE],
            PROG_NOTI_DAY_SCHEDULE)

    logger.debug("Notification Day Schedule: %s" % _LOADED_NOTI_SCHEDULE_DAY )
    logger.debug("Notification Hour Schedule: %s" % _LOADED_NOTI_SCHEDULE_HOUR )

def get_h264_dir_filelist(h264_file_path):
    """ Create a list of H.264 files created by RWS.

    Create a list of files with the h.264 extension in the H.264 directoy 
    specified in the config file.
    """
    h264_file_list = [w.replace('.h264', '') for w in 
        [f for f in os.listdir(h264_file_path) 
            if os.path.isfile(os.path.join(h264_file_path, f)) \
                    and f.find('.h264') != -1 and f.find('saving') == -1 ]]
    return h264_file_list

def get_newly_added_filelist():
    global  _H264_FILE_LIST, _PROG_CONFIG

    """ Getting the list of local video files in directory """
    video_file_list = get_h264_dir_filelist(_PROG_CONFIG[PROG_CONFIG_KEY_H264_PATH])
    if len(_H264_FILE_LIST) == 0:
        logger.debug("First time comparing... so, store current list in FILE_LIST")
        _H264_FILE_LIST  = video_file_list
        return []

    previous_filelist_set = set( _H264_FILE_LIST )
    file_list = [x for x in video_file_list if x not in previous_filelist_set]

    if len(file_list):
        logger.debug("Newly added H264 video file(s) : %s" % file_list)
    _H264_FILE_LIST =  video_file_list 
    return file_list

def get_noti_schedule(noti_config,noti_config_default):
    try:
        noti_schedule = ast.literal_eval(noti_config)
    except Exception as e:
        logger.error(
                "Failed to load noti schedule config: %s, Exception: %s"  
                % ( noti_config, e ))
        logger.warning(
                "Using the default config of noti config: %s"  
                % noti_config_default)
        noti_schedule = ast.literal_eval(noti_config_default)
    return noti_schedule

def is_in_noti_schedule():
    global _LOADED_NOTI_SCHEDULE_DAY, _LOADED_NOTI_SCHEDULE_HOUR,\
            _ALERT_DISABLE

    # check whether user disable the notification of motion detection
    if _ALERT_DISABLE == True:
        return False

    # 0 is Monday, ..., 6 is Sunday
    day_of_week =  datetime.today().weekday()
    hour_of_day =  datetime.today().hour
    if day_of_week in _LOADED_NOTI_SCHEDULE_DAY:
        if int(hour_of_day) in _LOADED_NOTI_SCHEDULE_HOUR:
            return True
    return False

def validate_external_command():
    global  _PROG_CONFIG

    try:
        validate_command = _PROG_CONFIG[PROG_CONFIG_KEY_VALIDATE_COMMAND]
        logger.debug('Validate CMD : %s' % validate_command)
        validate_result = subprocess.check_output(validate_command,shell=True)
    except Exception as e:
        logger.error("Failed to validate external command, Exception: %s"  % e)
        return False

    try:
        json_loaded = json.loads(validate_result)
    except Exception as e:
        logger.error("Failed to load json result: %s, Exception: %s"  
                % ( validate_result, e ))
        return False

    cmd_key = _PROG_CONFIG[PROG_CONFIG_KEY_VALIDATE_JSON_KEY] 
    if cmd_key not in json_loaded:
        logger.debug('Not found key in json_loaded: %s' % cmd_key )
        return False

    if str(json_loaded[cmd_key]) == _PROG_CONFIG[PROG_CONFIG_KEY_VALIDATE_JSON_VALUE]:
        logger.debug('Validation Key : %s, Value: %s is valid.' 
                % (cmd_key, _PROG_CONFIG[PROG_CONFIG_KEY_VALIDATE_JSON_VALUE]))
        return True

    logger.debug('Validation Key : %s, Value: %s is %s *not* valid' 
        % (cmd_key, _PROG_CONFIG[PROG_CONFIG_KEY_VALIDATE_JSON_VALUE], 
            json_loaded[cmd_key]))
    return False

def load_chat_id(chat_id_filename):
    global _CHAT_ID, _IS_CHAT_ID_LOADED
    try:
        with open(chat_id_filename, 'rb') as fp:
            _CHAT_ID = pickle.load(fp)
            _IS_CHAT_ID_LOADED = True
    except Exception as e:
        logger.error("Failed to load chat_id pickle , Exception: %s"  % e)
    except FileNotFoundError:
        pass
    else:
        logger.debug("Loaded chat_id : %s" % _CHAT_ID )

def save_chat_id(chat_id_filename, chat_id):
    global _CHAT_ID, _IS_CHAT_ID_LOADED
    logger.debug("Saving chat_id : %s" % chat_id )
    try:
        with open(chat_id_filename, 'wb') as fp:
            pickle.dump(chat_id, fp)
    except Exception as e:
        logger.error("Failed to save chat_id pickle , Exception: %s"  % e)
    else:
        _CHAT_ID = chat_id
        _IS_CHAT_ID_LOADED = True

def load_admins_list(admin_list_filename):
    global _LIST_OF_ADMINS, _IS_ADMINS_LIST_LOADED
    try:
        with open(admin_list_filename, 'rb') as fp:
            _LIST_OF_ADMINS = pickle.load(fp)
            _IS_ADMINS_LIST_LOADED = True
    except Exception as e:
        logger.error("Failed to load admins list pickle , Exception: %s"  % e)
    except FileNotFoundError:
        pass
    else:
        logger.debug("Loaded admins list : %s" % _LIST_OF_ADMINS )

def save_admins_list(admin_list_filename):
    global _LIST_OF_ADMINS, _IS_ADMINS_LIST_LOADED
    logger.debug("Saving admin list : %s" % _LIST_OF_ADMINS )
    try:
        with open(admin_list_filename, 'wb') as fp:
            pickle.dump(_LIST_OF_ADMINS, fp)
    except Exception as e:
        logger.error("Failed to save admins list pickle , Exception: %s"  % e)
    else:
        _IS_ADMINS_LIST_LOADED = True

def convert_to_mp4( path, filename, dump_output=False ):
    """ Upload the video file to Google Drive.

    First, convert h.264 video file to Mp4 video file using external command,
    then upload the converted mp4 file to Google Drive.
    """
    try:
        mp4_output_file = "{0}/{1}.mp4".format(
                PROG_CONSTANT_TEMP_PATH_MP4, filename)
        h264_input_file = "{0}/{1}.h264".format(
                path, filename)
        template = Template(_PROG_CONFIG[PROG_CONFIG_KEY_CMD_CONVERTING])
        video_converter_command = template.substitute(
                input_file=h264_input_file,output_file=mp4_output_file )
        logger.debug('Video Converint CMD : %s' % video_converter_command)
        if dump_output:
            convert_result = subprocess.check_call(video_converter_command, 
                    shell=True)
        else:
            FNULL = open(os.devnull, 'w')
            convert_result = subprocess.check_call(video_converter_command,
                    stdout=FNULL, stderr=FNULL, shell=True)
    except Exception as e:
        logger.error("Failed to conver Video, Exception: %s"  % e)
        return False, ''

    else:
        if convert_result is not 0:
            logger.error("Failed to convert %s to MP4" % filename )
            return False, ''

    return True,mp4_output_file

def remove_mp4( filename ):
    """ remove video file which is created temporarily for uploading """
    try:
        os.remove(filename)
    except OSError:
        logger.error("Failed to remove temp .mp4 file {}" .format(filename))
        return False

    logger.debug("Removing tempoary mp4 file {}" .format(filename))
    return True


""" 
bot command and jobqueue callback handlers
"""
def restrected_access_for_bot_command(func):
    """ Decorater for Restricted access for bot command
    """
    @wraps(func)
    def wrapped(bot, update, *args, **kwargs):
        global _IS_CHAT_ID_LOADED, _IS_ADMINS_LIST_LOADED
        # The first /start user becomes the actual admin. After that, 
        # users other than admin are restricted from using the bot command.
        if _IS_CHAT_ID_LOADED == False and _IS_ADMINS_LIST_LOADED == False:
            return func(bot, update, *args, **kwargs)

        user_id = update.effective_user.id
        if user_id not in _LIST_OF_ADMINS:
            logger.error("Unauthorized access denied for {}.".format(user_id))
            return
        return func(bot, update, *args, **kwargs)
    return wrapped


@restrected_access_for_bot_command
def bot_command_start(bot, update):
    """Send a message when the command /start is issued.
    """
    global _PROG_CONFIG, _IS_CHAT_ID_LOADED, _IS_ADMINS_LIST_LOADED, \
            _CHAT_ID, _LIST_OF_ADMINS

    if _IS_CHAT_ID_LOADED == False and _IS_ADMINS_LIST_LOADED == False:
        # When the first /start command is called, save the chat_id 
        # and admin id as files.
        update.message.reply_text(PROG_BOT_CMD_MSG_GREETING_0)
        save_chat_id(_PROG_CONFIG[PROG_CONFIG_KEY_CHAT_ID_FILENAME], 
                update.message.chat_id)
        # Save the admin id
        _LIST_OF_ADMINS.append(update.effective_user.id)
        save_admins_list(_PROG_CONFIG[PROG_CONFIG_KEY_ADMINS_LIST_FILENAME])

    elif _CHAT_ID != update.message.chat_id:
        # need to save new chat_id
        update.message.reply_text(PROG_BOT_CMD_MSG_GREETING_1)
        save_chat_id(_PROG_CONFIG[PROG_CONFIG_KEY_CHAT_ID_FILENAME], 
                update.message.chat_id)
    else:
        # using same chat_id, no need to save chat_id
        update.message.reply_text(PROG_BOT_CMD_MSG_GREETING_2)

@restrected_access_for_bot_command
def bot_command_help(bot, update):
    """Send a message when the command /help is issued.
    """
    update.message.reply_text(PROG_BOT_CMD_MSG_HELP)

@restrected_access_for_bot_command
def bot_command_disable_sound(bot, update):
    """Send a message when the command /disable_sound is issued.
    """
    global _ALERT_DISABLE

    if _ALERT_DISABLE == True:
        update.message.reply_text(PROG_BOT_CMD_MSG_ALERT_ALREADY_DISABLED)
        return
    _ALERT_DISABLE = True   # disable the sound alert flag
    logger.info("disable_sound issued by %s and disabled" % 
            update.effective_user.id )
    update.message.reply_text(PROG_BOT_CMD_MSG_ALERT_DISABLE)

@restrected_access_for_bot_command
def bot_command_enable_sound(bot, update):
    """Send a message when the command /enable_sound is issued.
    """
    global _ALERT_DISABLE

    if _ALERT_DISABLE == True:
        _ALERT_DISABLE = False   # eable the alert flag
        update.message.reply_text(PROG_BOT_CMD_MSG_ALERT_ENABLE)
        logger.info("enable_sound issued by %s and enabled" % 
                update.effective_user.id )
        return
    update.message.reply_text(PROG_BOT_CMD_MSG_ALERT_ALREADY_ENABLED)

@restrected_access_for_bot_command
def bot_command_disable_noti(bot, update):
    """Send a message when the command /disable_noti is issued.
    """
    global _NOTI_DISABLE

    if _NOTI_DISABLE == True:
        update.message.reply_text(PROG_BOT_CMD_MSG_NOTI_ALREADY_DISABLED)
        return
    _NOTI_DISABLE = True   # disable the alert flag
    logger.info("disable_noti issued by %s and disabled" % 
            update.effective_user.id )
    update.message.reply_text(PROG_BOT_CMD_MSG_NOTI_DISABLE)

@restrected_access_for_bot_command
def bot_command_enable_noti(bot, update):
    """Send a message when the command /enable_noti is issued.
    """
    global _NOTI_DISABLE

    if _NOTI_DISABLE == True:
        _NOTI_DISABLE = False   # eable the alert flag
        update.message.reply_text(PROG_BOT_CMD_MSG_NOTI_ENABLE)
        logger.info("enable_noti issued by %s and enabled" % 
                update.effective_user.id )
        return
    update.message.reply_text(PROG_BOT_CMD_MSG_NOTI_ALREADY_ENABLED)

@restrected_access_for_bot_command
def bot_command_settings(bot, update):
    """Send a message when the command /settings is issued.
    """
    global _NOTI_DISABLE, _ALERT_DISABLE, \
            _LOADED_NOTI_SCHEDULE_DAY, _LOADED_NOTI_SCHEDULE_HOUR

    settings_message = ''

    if _NOTI_DISABLE == True:
        settings_message += 'notice message disabled : /enable_noti to enable'
    else:
        settings_message += 'notice message enabled : /disable_noti to disable'

    if _ALERT_DISABLE == True:
        settings_message += '\nsound alert disabled : /enable_sound to enable'
    else:
        settings_message += '\nsound alert enabled : /disable_sound to disable'

    settings_message += '\nAlert day of week: {}'.format(
            _LOADED_NOTI_SCHEDULE_DAY)
    settings_message += '\nAlert hour of day: {}'.format(
            _LOADED_NOTI_SCHEDULE_HOUR)

    settings_message += '\n\nAdditional explanation -\n\
Day of week : 0 is Monday, 1 is Tuesday ... 6 is Sunday.'
    update.message.reply_text(settings_message)

def bot_error_handler(bot, update, error):
    """Log Errors caused by Updates.
    """
    logger.warning('Update "%s" caused error "%s"', update, error)

def jobqueue_callback_video_file(bot, job):
    """ the main jobqueue callback funcion that converts 
    the newly added H264 file to mp4 video file and sends 
    the telegram noti message
    """
    global _PROG_CONFIG, _CHAT_ID, _COOLING_TIME_LAST_NOTI

    file_list = get_newly_added_filelist()
    disable_noti = not is_in_noti_schedule() # need reverse boolean
    if len(file_list):
        logger.info('newly added file list: %s' % file_list )

        # get the time difference and compare whether difference is greater then
        # config value of NOTI_COOLING_PERIOD 
        time_difference = int(time.time() - _COOLING_TIME_LAST_NOTI)
        if _PROG_CONFIG[PROG_CONFIG_KEY_NOTI_COOLING_PERIOD] > time_difference:
            logger.debug(
                    "Skip sending the noti message, within the COOLING PERIOD {0}/{1}"
                    .format(time_difference,
                        _PROG_CONFIG[PROG_CONFIG_KEY_NOTI_COOLING_PERIOD]  ))
            return

        # Do not send message when in schedule
        # if NO_NOTI_IN_SCHEDULE is true, send the noti message to user 
        # without sound alert
        if disable_noti and _PROG_CONFIG[PROG_CONFIG_KEY_NO_NOTI_IN_SCHEDULE]:
            logger.debug( "Not send the noti message, within in schedule")
            return


    for media_filename in file_list:
        # mark new noti sending time
        # the new value will be used to compute NOTI_COOLING_PERIOD
        _COOLING_TIME_LAST_NOTI = time.time()

        if _PROG_CONFIG[PROG_CONFIG_KEY_MP4_UPLOAD_ENABLE] == True:
            try:
                # If MP4 Upload is enabled, H264 file will be converted to MP4 
                # video which is temporarily converted attached 
                # to telegram message
                result, temp_filename = convert_to_mp4(
                        _PROG_CONFIG[PROG_CONFIG_KEY_H264_PATH], media_filename )
                logger.info(
                        'chat id %s, noti disable: %s, trying to upload video file %s' 
                        % (_CHAT_ID, disable_noti, media_filename) )
                with open(temp_filename, 'rb') as video_media:
                    video_caption='Motion Detected : {}'.format(
                            media_filename )
                    bot.sendVideo(chat_id=_CHAT_ID, 
                            video=video_media,
                            caption=video_caption,
                            disable_notification=disable_noti,
                            timeout=_PROG_CONFIG[PROG_CONFIG_KEY_UPLOAD_TIMEOUT])
            except Exception as e:
                logger.error("Failed to upload video file, Exception: %s"  % e)
            else:
                logger.debug(
                        "File uploaded successfully, file: {}".format(media_filename))
                remove_mp4(temp_filename)
        else:
            # MP4 Upload is not enabled, so, sending the only telegram message 
            # of H264 file name 
            bot.send_message(_CHAT_ID, 
                    text='Motion Detected : {}'.format(media_filename))


def main(argv):
    global _COOLING_TIME_LAST_NOTI

    # argparser initialization
    parser = argparse.ArgumentParser(description
            ='{0} V{1}'.format(PROG_DESC,PROG_VERSION))
    parser.add_argument('--config','-c', default=PROG_FILE_CONFIG,
            help='specify the configuration file')
    parser.add_argument('--log','-l', 
            help='specifies log file name to save log')
    parser.add_argument('--verbose','-v', default=False,action='store_true',
            help='enable verbose log mode, default is false')
    parser.add_argument('--install-systemd-service', default=False,
            action='store_true', help='install systemd service of telegramBot')
    parser.add_argument('--remove-systemd-service', 
            default=False,action='store_true',
            help='install systemd service of telegramBot')
    # parse program arguments 
    args = parser.parse_args()
    if args.install_systemd_service:
        print('installing systemd service file : %s' %  
                TELEGRAMBOT_SYSTEMD_FILENAME)
        install_systemd_service_file()
        return 

    if args.remove_systemd_service:
        print('removing systemd service file : %s' %  
                TELEGRAMBOT_SYSTEMD_FILENAME)

        remove_systemd_service_file()
        return 0

    # loading the configuration from file
    load_config(args.config)
    setup_logger(args)

    # initialize the current time as LAST_NOTI mark
    _COOLING_TIME_LAST_NOTI -= _PROG_CONFIG[PROG_CONFIG_KEY_NOTI_COOLING_PERIOD]

    # the chat_id and admin list will be initialized with the first call of 
    # '/start' command.
    load_chat_id(_PROG_CONFIG[PROG_CONFIG_KEY_CHAT_ID_FILENAME])
    load_admins_list(_PROG_CONFIG[PROG_CONFIG_KEY_ADMINS_LIST_FILENAME])

    # Checking external command is enabled 
    if _PROG_CONFIG[PROG_CONFIG_KEY_EXTERNAL_COMMAND_ENABLE] == True:
        if validate_external_command() == False:
            _IS_EXTERNAL_COMMAND_VALID=False
            logger.error(PROG_MSG_EXTERNAL_COMMAND_NOT_VALID)
        else:
            _IS_EXTERNAL_COMMAND_VALID=True

    # Create the EventHandler and pass it your bot's token.
    updater = Updater(_PROG_CONFIG[PROG_CONFIG_KEY_AUTH_TOKEN])
    jobqueue = updater.job_queue

    # Get the dispatcher to register handlers
    dp = updater.dispatcher

    # on different commands - answer in Telegram
    dp.add_handler(CommandHandler("start", bot_command_start))
    dp.add_handler(CommandHandler("help", bot_command_help))
    dp.add_handler(CommandHandler("disable_sound", bot_command_disable_sound))
    dp.add_handler(CommandHandler("enable_sound", bot_command_enable_sound))
    dp.add_handler(CommandHandler("disable_noti", bot_command_disable_noti))
    dp.add_handler(CommandHandler("enable_noti", bot_command_enable_noti))
    dp.add_handler(CommandHandler("settings", bot_command_settings))

    # setting the jobqueue handler to check whether the video file list 
    # has been changed
    jobqueue.run_repeating (jobqueue_callback_video_file,
            _PROG_CONFIG[PROG_CONFIG_KEY_CHECKING_INTERVAL])

    # log all errors
    dp.add_error_handler(bot_error_handler)

    # Start the Bot
    updater.start_polling()

    # Run the bot until you press Ctrl-C or the process receives SIGINT,
    # SIGTERM or SIGABRT. This should be used most of the time, since
    # start_polling() is non-blocking and will stop the bot gracefully.
    updater.idle()


if __name__ == '__main__':
    sys.exit(main(sys.argv))

