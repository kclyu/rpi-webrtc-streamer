
# telegamBot for Rpi-WebRTC-Streamer

## Introduction

Motion Video File is created by Motion Detection function in Rpi-WebRTC-Streamer package. This TelegramBot converts the H.264 video file, which is the Motion Videp Clip created by RWS, into MP4 file and sends it to the Telegram Messenger Client to deliver Motion Notification to the user. telegramBot has the function of attaching converted MP4 file to notifications to be transmitted, Notification Schedule functions, and user can set Notification disable or Silent Notification (without sound) through commands supported by bot commands. 

## Installation

### TelegramBot Token
You need auth_token to configure this telegramBot for Rpi-WebRTC-Streamer. auth_token is the authoriization code for Telegram Bot to work with Telegram Bot Server. You need to create a new bot through Telegram BotFather (sending `/newbot` command to BotFather and a few procedures) , then BotFather will generate and display a Auth Token for the new Bot. The corresponding Auth Token should be copied and added to the configuration file later.

<img src="https://github.com/kclyu/rpi-webrtc-streamer/blob/master/misc/telegram_botfather.png" width="500">

**(The name of the new bot should be different from the example.)**

The following URL is a URL that tells you how to create a new Bot from Telegram BotFather.

[https://core.telegram.org/bots#6-botfather](https://core.telegram.org/bots#6-botfather)

In the above URL, there is some bot command for you to change various settings of the newly created Bot. It is recommended to change the bot icon with '/setuserpic' bot command.  You can change the icon of newly created bot,  after that, you can see the bot image with the changed icon on your phone or other Telegram Messenger Client.



### Required Package
To use this bot, install the Telegram Bot Python API and yaml package with the following command.
(use yaml for the python configuration file, 'sudo pip install pyyaml' may take a long time for Raspberry PI ZeroW hardware)
```
sudo pip install python-telegram-bot --upgrade
sudo apt-get install libyaml-dev libpython2.7-dev
sudo pip install pyyaml
```

### Configuration File 
After copying telegramBot.conf to etc as in the following command, replace auth_token in newly copied telegramBot.conf with auth_token received from BotFather.
```
sudo cp /opt/rws/etc/template/telegramBot.conf /opt/rws/etc
sudo vi /opt/rws/etc/telegramBot.conf
```


In order to check whether it is running properly, Please execute the following command to test newly edited configuration file.  To add a newly created Bot to your Telegram Messenger client, you can connect it by clicking on `t.me/<Your Bot Name>` in BotFather's /newbot's response.

If the connection is successful, 'Saving chat_id' and 'Saving admin list' messages will be displayed as below and you can confirm that Telegram Bot has executed / start command normally.

```
pi@raspberryW:/home/pi $ cd /opt/rws/tools
pi@raspberryW:/opt/rws/tools $ sudo ./telegramBot.py -c ../etc/telegramBot.conf -v
2018-01-14 22:57:22,559 - __main__ - ERROR - Failed to load chat_id pickle , Exception: [Errno 2] No such file or directory: '/opt/rws/etc/run/telegrambot.pickle'
2018-01-14 22:57:22,562 - __main__ - ERROR - Failed to load admins list pickle , Exception: [Errno 2] No such file or directory: '/opt/rws/etc/run/telegrambotadmin.pickle'
2018-01-14 22:57:27,577 - __main__ - DEBUG - First time comparing... so, store current list in FILE_LIST
2018-01-14 22:57:52,757 - __main__ - DEBUG - Saving chat_id : 161561821
2018-01-14 22:57:52,762 - __main__ - DEBUG - Saving admin list : [161561821]
2018-01-14 23:04:26,596 - telegram.ext.updater - ERROR - Error while getting Updates: Timed out
2018-01-14 23:04:26,647 - __main__ - WARNING - Update "None" caused error "Timed out"
2018-01-14 23:07:27,137 - telegram.ext.updater - ERROR - Error while getting Updates: Timed out
2018-01-14 23:07:27,188 - __main__ - WARNING - Update "None" caused error "Timed out"
...
```

In addition, `motion_h264_path` in the conf file should be the same as` motion_h264_path` specified in `/opt/rws/etc/motion_config.conf`, and whenever the H.264 video file is added normally, the Bot will convert the MP4 video file to your Telegram Messenger.

Please refer to the section below for detailed configuration items of telegramBot.

### Security of telegramBot
telegramBot is accessible only to the user who first sent / start command. If you do not have access to the telegramBot, delete the chat_id and admin_list pickle files and make a chat connection from the Messenger client again.

### Systemd Service file registration
You can register telegramBot.py as systemd service by typing the following command.
```
sudo ./telegramBot.py --install-systemd-service
sudo systemctl enable telegramBot
sudo systemctl start telegramBot 
```
Below is an example of executing the above command.

```
pi@raspberryW:/opt/rws/tools $ sudo ./telegramBot.py --install-systemd-service
installing systemd service file : /lib/systemd/system/telegramBot.service
systemd telegramBot.service file installed successfully.
pi@raspberryW:~ $ 
pi@raspberryW:~ $ sudo systemctl enable telegramBot
Created symlink from /etc/systemd/system/multi-user.target.wants/telegramBot.service to /lib/systemd/system/telegramBot.service.
pi@raspberryW:~ $ sudo systemctl start telegramBot
pi@raspberryW:~ $ 
pi@raspberryW:/opt/rws/tools $ sudo systemctl start telegramBot
pi@raspberryW:/opt/rws/tools $ ps -ef | grep telegramBot
root      1973     1 49 00:11 ?        00:00:02 python /opt/rws/tools/telegramBot.py --log /opt/rws/log/telegramBot.log
pi        1987  1013  0 00:12 pts/1    00:00:00 grep --color=auto telegramBot
pi@raspberryW:/opt/rws/tools $ 
```
### Systemd Service file Deregistration
Enter the following command to remove telegramBot.py as systemd service file.
```
sudo systemctl stop telegramBot
sudo systemctl disable telegramBot
sudo ./telegramBot.py --remove-systemd-service
```

### TelegramBot Start/Stop

- Start 
```
sudo systemctl start telegramBot
```
- Stop 
```
sudo systemctl stop telegramBot
```
- Restart
```
sudo systemctl restart telegramBot
``` 
## Configuration Items
|config|Value|Description|
|----------------|-----------------|--------------|
|auth_token|string| Auth_token generated from Telegram BotFather|
|motion_h264_path|directory path|the directory where Rpi-WebRTC-Streamer stores h.264 files|
|checking_interval|seconds|Set the interval to invoke process to check for new files added.|
|chat_id_filename|file path|the file to store the telegram chat_id to use when sending the message.  the below example is default value|
|admin_list_filename|file path|specify the location of the file to store the admin id.|
|mp4_upload_enable|boolean| specifies whether the h.264 file generated by rws is converted to an mp4 file and included in the telegram message. if you set the default value to true and set it to false, only the name of the newly created file will be transferred to the message without the mp4 file.  default value is true|
|converting_command|path|specifies the system command to use when converting to mp4 file.  the specified command string must have $input_file and $output_file,  and it is a template to replace internally. *note1*|
|upload_timeout|seconds|When telegram message upload, specify the value of upload timeout seconds. If network upload is slow, you should increase the timeout value. |
|noti_cooling_period|seconds|Keep the message waiting for a period of time after transmission.  No new messages are sent during this period. When set to 0, new motion detection messages will continue to be transmitted  without waiting time.|
|sound_alerts_in_schedule|boolean|Always disable sound alert when sending messages. The Telegram Client receives new messages but without a sound alert.|
|noti_schedule_hour|python list definition|specifies the hour of day. the following is a example of noti_schedule_hour  : noti_schedule_hour: '[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23]'|
|noti_schedule_day|python list definition|specifies the day of week. 0 is Monday, ... 6 is Sunday. the following is a example of noti_schedule_day||

*note 1* :  There are two ffmpeg_neon and ffmpeg_armv6 files in the tools directory,  and ffmpeg_neon should be used on pi2 or higher hardware. and ffmpeg_armv6 should be used on ZeroW or PI1 

`converting_command: '/opt/rws/tools/ffmpeg_armv6 -y -i $input_file $output_file'`

When you convert h.264 video to mp4, the size will be smaller than h.264 original file and it is advantageous when you send the file to Messenger like Telegram or upload it to Cloud Storage  like GoogleDrive

However, conversion to mp4 is very slow on hardware such as PI 1 or PI Zero W with an ARMv6 family of CPUs, so it can hardly be used. In this case, you should use '-vcodec copy' to convert the file to a level that simply changes the container, as shown below. 

`converting_command: '/opt/rws/tools/ffmpeg_armv6 -y -vcodec copy -i $input_file $output_file'`

## Supported Bot Command
## Configuration Items
|Bot Command|Description|
|---------------|--------------|
|/start|This is the command used when the Client and Bot are connected for the first time. When this command is executed for the first time, the chat_id and admin_id values are saved to a file (chat_id_filename, admin_list_filename), and all other commands are blocked if the admin_id is different.|
|/disable_sound|When delivering a notification to the Messenger Client, the notification will be delivered without sound playback| 
|/enable_sound|When the notification is delivered to the Messenger Client, the client plays the sound|
|/disable_noti|Do not forward notification to the Messenger client even if there is a new video clip|
|/enable_noti| Bot will send notification when new video clip added to Messenger client|
|/help|Show help message|
|/settings|Show current set value|

### Version History

 * 2018/01/13 v0.73 : Initial Version



