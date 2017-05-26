
# Setup Rpi-WebRTC-Streamer 

Once you have successfully built the RWS, It can be used in the following configuration.

- RWS Standalone
- HTTP server with Reverse Proxy + RWS
- HTTP server with Reverse proxy  + Client Certs + RWS

RWS has a simple HTTP and WebSocket function that can be used in a local/private network that does not require security.  You can use RWS standalone configuration at this situation only it is  protected behind F/W or NAT. However, if it is possible to access from the Internet directly, it is necessary to implement a function that can restrict the user with the HTTP server.  In this case, you must use security features by using reverse proxying with user authentification or client certs through HTTP server.


## RWS running environment
The RWS executable directory should contain the following directories: The "etc" directory contains the configuration file, and the "log" directory contains the RWS log file. The "web-root" must have html and javascript files for use by the RWS HTTP server.

### Required Hardware 
- Raspberry PI 2 (or Upper version)
- Raspberry Zero W  (Currently under examination)
#### Video Camera
- RPI Camera board V1/V2
#### Audio hardware
Please refer to the  [README_audio.md](https://github.com/kclyu/rpi-webrtc-streamer/blob/master/README_audio.md)


### Configuration Files

#### Server Main Configuration


|config|Value|Description|
|----------------|-----------------|--------------|
|websocket_enable|boolean|enable/disable the websocket server|
|websocket_port|integer|RWS websocket server port number|
|direct_socket_enable|boolean|enable/disable the direct socket server|
|turn_server|URL|specify the turn server URL(not implemented yet)|
|sturn_server|URL|specify the sturn server (using google stun server as default)|
|app_channel_config|file path|specify the App Channel config (not implemented yet)|
|media_config|file path|specify the WebRTC media parameters|

#### WebRTC Media Config file

|config|Value|Description|
|----------------|-----------------|--------------|
|max_bitrate|integer|specify the maximum bit rate for audio/video (default value is 350000(3.5M) bps.)|
|use_4_3_video_resolution|boolean|specify screen resolution ratio ( true: using 4:3, false using 16:9)|
|use_dynamic_video_resolution|boolean|specify using dynamic resolution changing based on the bandwidth estimation|
|video_resolution_list_4_3|resolution list|list of 4:3 ratio screen resolution|
|video_resolution_list_16_9|resolution list|list of 16:9 ratio screen resolution |
|use_initial_video_resolution|boolean|use or not use initial video resolution specified by initial_video_resolution|
|initial_video_resolution|screen resolution|screen resolution to use at startup|
|audio_processing|boolean|enable/disable below audio processing feature|
|audio_echo_cancellation|boolean|enable/disable google echo cancellation feature|
|auido_gain_control|boolean|enable/disable google gain control feature|
|audio_high_passfilter|boolean|enable/disable google high pass filter feature|
|auido_noise_suppression|boolean|enable/disable google noise suppression feature|
|audio_level_control_enable|boolean|enable/disable audio level control feature (this feature does not depend on the audio_processing config)||

#### Channel config file

|config|Value|Description|
|----------------|-----------------|--------------|
|web_root|path|specify the internal HTTP server web root|
|libwebsocket_debug|boolean|enable/disable libwebsockets debug message printing|


## Running RWS in Standalone Configuration
### Setup configuration file for RWS 

```
cd $(WHERE_RWS_DIRECTORY)/etc
cp app_channel.conf.template app_channel.conf 
cp webrtc_streamer.conf.template webrtc_streamer.conf
cp media_config.conf.template media_config.conf
vi app_channel.conf  # change the 'your.dns_hostname.here' to your domain name. 
``` 

### Running RWS
RWS has not been created the scripts for a process monitoring and start/restart. You can simply enter the following command to start RWS.

```
cd $(WHERE_RWS_DIRECTORY)
webrtc-streamer >& /dev/null &
```

This command will start listening at 8889 port number.


### Open in Browser
As mentioned above, Standalone Configuration only need RWS running environment.

Open the following URL in the chrome browser will display the native-peerconnection testing page.

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
Below is an example of how RWS works with Nginx. It shows how to use Nginx as HTTP and WebScoket Reverse Proxy and cover the security part with client side certs. Other configurations or other requirements may require Googling, web server configuration, or security expert advice. (It may not be enough to emphasize as much, I recommend that you do not leave the camera feed open to the public internet without any security measures.)

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

To use the h.264 webrtc streaming in chrome or firefox, browser settings are required for each browser.
For the chrome browser case: you need to to enable the '#enable-webrtc-h264-with-openh264-ffmpeg' entry in the 'chrome//flags' URL. For more information, please refer to the googling or [PSA](https://groups.google.com/forum/#!topic/discuss-webrtc/8ov4YW6HLgo).

For the firefox browser case: Please refer to [Checking H264 Capability on Firefox](https://github.com/EricssonResearch/openwebrtc/issues/425).


