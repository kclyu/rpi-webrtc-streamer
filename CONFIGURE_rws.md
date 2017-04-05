
# Configure RPi-WebRTC-Streamer 

Once you have successfully built the RWS, You need to install NGINX as the Web Server and SSL setup. Also user authentication thing is required if there is other thing in addition to SSL. This document will do not cover the related SSL details and User Authentication method.

Below is an example of how RWS works with Nginx. It shows how to use Nginx as HTTP and WebScoket Reverse Proxy and cover the security part with client side certs. Other configurations or other requirements may require Googling, web server configuration, or security expert advice. (It may not be enough to emphasize as much, I recommend that you do not leave the camera feed open to the public internet without any security measures.)


## Nginx Configuration Example
####  Nginx with Client Side Certificate Auth as a Reverse Proxy

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
cd ~/Workspace/rpi-webrtc-streamer/web-root
sudo cp -r native-peerconnection /usr/share/nginx/html
``` 
#### Setup configuration file for RWS 

```
cd ~/Workspace/rpi-webrtc-streamer/etc
cp app_channel.conf.template app_channel.conf 
cp webrtc-streamer.conf.template webrtc-streamer.conf
vi app_channel.conf  # change the 'your.dns_hostname.here' to your domain name. 
``` 

## Running RWS
RWS has not been created the scripts for a process monitoring and start/restart. You can simply enter the following command to start RWS.

```
cd ~/Workspace/rpi-webrtc-streamer
webrtc-streamer >& /dev/null &
```

This command will start listening at 8889 port number.


## Testing RWS in browser
To test RWS WebRTC streaming in browser, Open the following URL in the latest chrome or firefox browser.
```
https://server.example.com/native-connection/index.html
```

To use the h.264 webrtc streaming in chrome or firefox, browser settings are required for each browser.
For the chrome browser case: you need to to enable the '#enable-webrtc-h264-with-openh264-ffmpeg' entry in the 'chrome//flags' URL. For more information, please refer to the googling or [PSA](https://groups.google.com/forum/#!topic/discuss-webrtc/8ov4YW6HLgo).

For the firefox browser case: Please refer to [Checking H264 Capability on Firefox](https://github.com/EricssonResearch/openwebrtc/issues/425).
* Note: RWS h.264 stream does not work properly due to an unknown reason in firefox. If the cause is known and it works normally, this note part will be removed.

## Testing RWS with Android AppRTCMobile App
#### Android AppRTCMobile APP 
Android AppRTCMobile is a WebRTC app for android designed to work with Google's https://appr.tc. One of the unique features of the Android App is that it provides a function to directly link without the signaling server (appr.tc) using a TCP socket called Direct RTC Client.

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

#### Building Android AppRTCMobile App

This document does not detail the procedure for Android App Building, please refer to https://webrtc.org/native-code/android/ for the compilation environment or other tool installation.

Please refer to the example below for gn file settings.

```
cd ~/Worksapce/webrtc/src
mkdir android_build
cp ~/Workspace/rpi-webrtc-streamer/misc/webrtc_android_build_args.gn android_build/args.gn
gn gen android_build
ninja -C android_build
# After build is complete
cd android_build/apks
adb install -r AppRTCMobile.apk
```



