
# Setup Rpi-WebRTC-Streamer 

Once you have successfully installed Rpi-WebRTC-Streamer(RWS), It can be used in the following configuration.

- RWS Standalone
- HTTP server with Reverse Proxy + RWS
- HTTP server with Reverse proxy  + Client Certs + RWS

RWS has a simple HTTP and WebSocket interface that can be used in a local/private network that does not require security.  You can use RWS standalone configuration at this situation only it is  protected behind F/W or NAT. However, If you need to access directly from public internet, it is necessary to implement a function that can restrict the other user from RWS.  In this case, you must use security features by using the VPN or client certs or the ability to authenticate users.

## Required Hardware
### Raspberry PI 
- Raspberry PI 2 (or Upper version)
- Raspberry Zero W  
### Video Camera
- RPI Camera board V1/V2
### Audio hardware (optional)
Please refer to the  [README_audio.md](https://github.com/kclyu/rpi-webrtc-streamer/blob/master/README_audio.md)


## Setup configuration file for RWS 
The directories where RWS is installed include the following directories: There is a log directory where RWS log files are stored, an etc directory with RWS configuration files, and a tools directory with other tools that can be used with RWS. There is a web-root that contains html and javascript files.

If you installed RWS with a deb file, the default conf files are installed in the etc directory. Please refer to 'Configuration Files' section below for configuration file settings and description of each setting item.

If you want to build and install RWS, modify the necessary files using the configuration file template included in etc/template as shown in the example below.

```
cd $(WHERE_RWS_DIRECTORY)/etc
cp template/webrtc_streamer.conf webrtc_streamer.conf
cp template/media_config.conf media_config.conf
cp template/motion_config.conf motion_config.conf
``` 

##  Process Management 
RWS is only compatible with systemd. To execute the process and check the status, use the following command.

### Start
```
sudo systemctl start rws
```
### Stop
```
sudo systemctl stop rws
```
### Status/Monitoring
`sudo systemctl status rws` is used for process status checking.
`sudo journalctl -u rws` for  log message for process start/stop/standard error log checking.
```
sudo systemctl status rws
● rws.service - Rpi WebRTC Streamer
   Loaded: loaded (/etc/systemd/system/rws.service; enabled)
   Active: active (running) since Thu 2017-06-08 21:08:45 KST; 2h 47min ago
 Main PID: 14968 (webrtc-streamer)
   CGroup: /system.slice/rws.service
           └─14968 /opt/rws/webrtc-streamer --log /opt/rws/log

Jun 08 22:04:44 raspberrypi webrtc-streamer[14968]: ALSA lib control.c:953:(snd_ctl_open_noupdate) Invalid CTL
Jun 08 22:04:44 raspberrypi webrtc-streamer[14968]: ALSA lib control.c:953:(snd_ctl_open_noupdate) Invalid CTL
Jun 08 22:04:44 raspberrypi webrtc-streamer[14968]: ALSA lib pcm.c:2239:(snd_pcm_open_noupdate) Unknown PCM
Jun 08 22:04:44 raspberrypi webrtc-streamer[14968]: ALSA lib control.c:953:(snd_ctl_open_noupdate) Invalid CTL
Jun 08 22:04:44 raspberrypi webrtc-streamer[14968]: ALSA lib control.c:953:(snd_ctl_open_noupdate) Invalid CTL
Jun 08 22:04:44 raspberrypi webrtc-streamer[14968]: ALSA lib pcm.c:2239:(snd_pcm_open_noupdate) Unknown PCM
Jun 08 22:04:44 raspberrypi webrtc-streamer[14968]: ALSA lib control.c:953:(snd_ctl_open_noupdate) Invalid CTL
Jun 08 22:04:44 raspberrypi webrtc-streamer[14968]: ALSA lib pcm.c:2239:(snd_pcm_open_noupdate) Unknown PCM
Jun 08 22:04:44 raspberrypi webrtc-streamer[14968]: ALSA lib control.c:953:(snd_ctl_open_noupdate) Invalid CTL
Jun 08 22:04:44 raspberrypi webrtc-streamer[14968]: ALSA lib pcm.c:2239:(snd_pcm_open_noupdate) Unknown PCM
Hint: Some lines were ellipsized, use -l to show in full.

sudo journalctl -u rws
Jun 08 19:56:05 raspberrypi systemd[1]: Stopped Rpi WebRTC Streamer.
Jun 08 19:56:05 raspberrypi systemd[1]: Stopped Rpi WebRTC Streamer.
Jun 08 21:08:45 raspberrypi systemd[1]: Starting Rpi WebRTC Streamer...
Jun 08 21:08:45 raspberrypi systemd[1]: Started Rpi WebRTC Streamer.
Jun 08 21:08:45 raspberrypi systemd[1]: Started Rpi WebRTC Streamer.
Jun 08 21:09:18 raspberrypi webrtc-streamer[14968]: ALSA lib control.c:953:(snd_ctl_open_noupdate) Invalid CTL
Jun 08 21:09:18 raspberrypi webrtc-streamer[14968]: ALSA lib control.c:953:(snd_ctl_open_noupdate) Invalid CTL
Jun 08 21:09:18 raspberrypi webrtc-streamer[14968]: ALSA lib control.c:953:(snd_ctl_open_noupdate) Invalid CTL
Jun 08 21:09:18 raspberrypi webrtc-streamer[14968]: ALSA lib pcm.c:2239:(snd_pcm_open_noupdate) Unknown PCM
Jun 08 21:09:18 raspberrypi webrtc-streamer[14968]: ALSA lib control.c:953:(snd_ctl_open_noupdate) Invalid CTL
Jun 08 21:09:18 raspberrypi webrtc-streamer[14968]: ALSA lib pcm.c:2239:(snd_pcm_open_noupdate) Unknown PCM
Jun 08 21:09:18 raspberrypi webrtc-streamer[14968]: ALSA lib control.c:953:(snd_ctl_open_noupdate) Invalid CTL
Jun 08 21:09:18 raspberrypi webrtc-streamer[14968]: ALSA lib control.c:953:(snd_ctl_open_noupdate) Invalid CTL
Jun 08 21:09:18 raspberrypi webrtc-streamer[14968]: ALSA lib pcm.c:2239:(snd_pcm_open_noupdate) Unknown PCM
```
## RWS Standalone configuration 
It is a configuration that can be used in a local/private network using one of the default http server port 8889 without having to depend on another Http server like Nginx.

If you installed RWS using deb file, configuration files based on port 8889 for HTTP and WebSocket server will be installed as default conf file when installing. Once the installation is complete, you can use the browser to open the URL as shown below.

### Open Testing URL in Chrome Browser
Open the following URL in the browser will display the native-peerconnection testing page.

```
http://your-private-ip-address:8889/native-peerconnection/
```
*`8889` port number is the websocket_port specified in the config file, and the URL protocol uses http instead of https.*


### Using VPN
If you can use a VPN on your router, you can use it on the public internet as if you are using a local/private network without any additional https or client certs file configuration. It is the recommended way to use RWS on public internet.

The method for configuring the VPN will be included in the NAT or F/W product manual you are using.

Android and iOS have different types of VPN protocol stack that they support. In general, the basic VPN protocol supported by Android is PPTP/L2TP/IPSec, though it depends on the phone vendor. IOS supports L2TP / IPSec / IKEV2.

If the same protocol is supported by Phone and Router, you can use VPN service by configuring VPN on Router and Phone. You can find out how to set detailed VPN settings for Phone and Router by searching the Internet.

If you have successfully set up the VPN and can use the network through the VPN, you can use it in your browser using http instead of https as you used the RWS testing page on the local network above.
	

``` 
http://your-private-ip-address:8889/native-peerconnection/
```
*`8889` port number is the websocket_port specified in the config file, and the URL protocol uses http instead of https.*
### Open in Android AppRTC app
Android AppRTCMobile is a WebRTC app for Android designed to work with Google's https://appr.tc. One of the unique features of the Android App is that it provides a function to directly link without the signaling server (appr.tc) using a TCP socket called Direct RTC Client.

See below for how to install AppRTCMobile on android devices.
```
cd ~/Workspace/rpi-webrtc-streamer/misc
adb install -r AppRTCMobile.apk
```

Please change the following in AppRTC Settings menu of App.
- Setup AppRTCMobile app
   - 'Enter ApRTC local video resolution' --- select the lowest resolution
   - 'Select default video codec' -- select one of H264 baseline or H264 High

To work with RWS, enter the IP address of RWS is installed in the room number input field.

## Running RWS with Nginx
The HTTP function of RWS has no room for adding user-friendly HTTP return code and other functions. Therefore, it is recommended to use reverse proxy in front of RWS to add user authentication or other logging function in addition to using RWS in local network.
####  Nginx  as a Reverse Proxy
Below is an example of using reverse proxy with nginx.
```
server {
        listen 8080;
        server_name example.server.com;
        root /home/pi/Workspace/client/web-root;
        index index.html index.htm;

        location / {
                allow 10.0.0.0/24;
                allow 192.168.0.0/24;
                allow 127.0.0.1;
                deny all;
                try_files $uri $uri/ =404;
        }

        location /rws/ws {
                proxy_pass http://localhost:8889;
                proxy_http_version 1.1;
                proxy_set_header Upgrade $http_upgrade;
                proxy_set_header Connection "upgrade";
        }
}
```


####  Nginx with Client Side Certificate Auth as a Reverse Proxy
Below is an example of how RWS works with Nginx. It shows how to use Nginx as HTTPS and Secured WebScoket Reverse Proxy and cover the security part with client side certs. Other configurations or other requirements may require Googling, web server configuration, or security expert advice. (It may not be enough to emphasize as much, I recommend that you do not leave the camera feed open to the public internet without any security measures.)

```
###
### Nginx with SSL as a Reverse Proxy
###
server {
    listen 80;
	server_name example.server.com; # change your domain name
    rewrite ^(.*) https://$server_name$1 permanent;
}

server {
	listen 443 ssl;

	server_name example.server.com; # change your domain name

	ssl on;
	ssl_certificate /etc/ssl/webrtc_ca/certs/server.crt;
	ssl_certificate_key /etc/ssl/webrtc_ca/private/server.key;
	ssl_client_certificate /etc/ssl/webrtc_ca/certs/ca.crt;
	ssl_crl /etc/ssl/webrtc_ca/private/ca.crl;
	ssl_verify_client on;

	ssl_session_timeout 5m;

	ssl_protocols SSLv3 TLSv1;
	ssl_ciphers ALL:!ADH:!EXPORT56:RC4+RSA:+HIGH:+MEDIUM:+LOW:+SSLv3:+EXP;
	ssl_prefer_server_ciphers on;

    ### For WebSocket Reverse proxy
    location /rws/ws {
            proxy_pass http://localhost:8889;
            proxy_http_version 1.1;
            proxy_set_header Upgrade $http_upgrade;
            proxy_set_header Connection "upgrade";
     }
    ### For HTTP Reverse proxy
	location /rws {
		  ## proxy setting for RWS(Rpi-WebRTC-Streamer)
		  ## it will bind 8889 port on localhost
          proxy_pass          http://localhost:8889
          proxy_set_header    Host             $host;
          proxy_set_header    X-Real-IP        $remote_addr;
          proxy_set_header    X-Forwarded-For  $proxy_add_x_forwarded_for;
          proxy_set_header    X-Client-Verify  SUCCESS;
          proxy_set_header    X-Client-DN      $ssl_client_s_dn;
          proxy_set_header    X-SSL-Subject    $ssl_client_s_dn;
          proxy_set_header    X-SSL-Issuer     $ssl_client_i_dn;
          proxy_read_timeout 1800;
          proxy_connect_timeout 1800;
	}
}

```
#### installing html example for testing
You need to copy the html and javascript files to nginx html directory. 
```
cd $(WHERE_RWS_DIRECTORY)/web-root
sudo cp -r native-peerconnection /usr/share/nginx/html
``` 

#### Testing RWS in browser
To test RWS WebRTC streaming in browser, Open the following URL in the latest chrome or firefox browser.
```
https://server.example.com/native-connection/index.html
```

### Configuration Files

#### Server Main Configuration


|config|Value|Description|
|----------------|-----------------|--------------|
|websocket_enable|boolean|enable/disable the websocket server|
|websocket_port|integer|RWS websocket server port number|
|direct_socket_enable|boolean|enable/disable the direct socket server|
|turn_server|URL|specify the turn server URL(not implemented yet)|
|sturn_server|URL|specify the sturn server (using google stun server as default)|
|media_config|file path|specify the WebRTC media parameters|
|motion_config|file path|specify the RWS motion configuration parameter|
|web_root|path|specify the internal HTTP server web root|
|libwebsocket_debug|boolean|enable/disable libwebsockets debug message printing|
|wss_url|string|setting for WebSocket URL (By default, wss_url is set to 'wss_url=__ WS_SERVER __/rws/ws'. The value of '/rws/ws' is used by default in html examples such as native-peerconnection. If you change /rws/ws, you also have to modify the contents of each html file and so on.)


#### WebRTC Media Config file

|config|Value|Description|
|----------------|-----------------|--------------|
|max_bitrate|integer|specify the maximum bit rate for audio/video (default value is 350000(3.5M) bps.)|
|resolution_4_3_enable|boolean|specify screen resolution ratio ( true: using 4:3, false using 16:9)|
|use_dynamic_video_resolution|boolean|specify using dynamic resolution changing based on the bandwidth estimation (If set to true, dynamic resolution feature is enabled; if set to false, fixed_resolution will be used.)|
|use_dynamic_video_fps|boolean|specify using dynamic fps changing based on the bandwidth estimation (If set to true, dynamic fps feature is enabled; if set to false, fixed_video_fps will be used.)|
|video_resolution_list_4_3|video resolution list|list of 4:3 ratio screen resolution|
|video_resolution_list_16_9|video resolution list|list of 16:9 ratio screen resolution |
|use_initial_video_resolution|boolean|use or not use initial video resolution specified by initial_video_resolution|
|fixed_video_resolution|video resolution|The specified video resolution will be used from startup, and video resolution will not be changed dynamically *Note 1*|
|fixed_video_fps|integer|The specified video fps will be used from startup, and video fps will not be changed dynamically|
|audio_processing|boolean|enable/disable below audio processing feature|
|audio_echo_cancellation|boolean|enable/disable google echo cancellation feature|
|auido_gain_control|boolean|enable/disable google gain control feature|
|audio_high_passfilter|boolean|enable/disable google high pass filter feature|
|auido_noise_suppression|boolean|enable/disable google noise suppression feature|
|audio_level_control|boolean|enable/disable audio level control feature (this feature does not depend on the audio_processing config)||
|video_sharpness |integer|set image sharpness (-100 to 100), 0 is the default.|
|video_contrast |integer|set image contrast (-100 to 100), 0 is the default|
|video_brightness |integer|set image brightness (0 to 100), 50 is the default. 0 is black, 100 is white.|
|video_saturation |integer|set image saturation (-100 to 100), 0 is the default.|
|video_ev |integer|set the EV compensation. Range is -10 to +10, default is 0.|
|video_exposure_mode|exposure mode |set exposure mode, auto is the default. possible exposure mode: off, auto, night, nightpreview, snow, beach, verylong, fixedfps, antishake, fireworks |
|video_flicker_mode |flicker mode|set flicker avoid mode, off is the default, possible flicker mode : off, auto, 50hz, 60hz|
|video_awb_mode |awb mode|set Automatic White Balance (AWB) mode, auto is the default. possible awb_mode: off, auto, sun, cloud, shade, tungsten, fluorescent, incandescent, flash, horizon |
|video_drc_mode |drc mode|set DRC Level, off is the default. possible DRC mode: off,low,med,high|

*Note1 : the configuration name is changed from initial_video_resolution*

#### Motion Config file
Please refer to [README_motion.md](https://github.com/kclyu/rpi-webrtc-streamer/blob/master/README_motion.md) document for setting of Motion conf file.





