
# Configure RPi-WebRTC-Streamer 
RWS is a program that streaming the h.264 video from a Raspberry PI camera. RWS uses WebRTC protocol as a streaming protocol and it uses HTTP (WebRTC peerconnection example) protocol as signaling.

Once you have successfully built the RWS, We need to install NGINX as the Web Server and SSL setup. Also user authentication thing is required if there is other thing in addition to SSL. This document will do not cover the related SSL details and User Authentication method.

However, SSL and reverse proxy settings required in nginx, please refer to the following section.

If you are planning to set up User Authentication, 'Nginx with SSL as a Reverse Proxy' is enough to see details of the settings in nginx, or You need to check 'Nginx with Client Side Certificate Auth as a Reverse Proxy' section if you want to use client authentication certs.

## Nginx Configuration Example
####  Nginx with SSL as a Reverse Proxy

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
	ssl_session_cache  builtin:1000  shared:SSL:10m;
	ssl_protocols  TLSv1 TLSv1.1 TLSv1.2;
	ssl_ciphers HIGH:!aNULL:!eNULL:!EXPORT:!CAMELLIA:!DES:!MD5:!PSK:!RC4;
	ssl_prefer_server_ciphers on;

	ssl_certificate /etc/nginx/ssl/nginx.crt;
        ssl_certificate_key /etc/nginx/ssl/nginx.key;

	root /usr/share/nginx/html;
	index index.html index.htm;

	# Make site accessible from http://localhost/
	server_name localhost;

	location / {
		# First attempt to serve request as file, then
		# as directory, then fall back to displaying a 404.
		try_files $uri $uri/ =404;
		# Uncomment to enable naxsi on this location
		# include /etc/nginx/naxsi.rules
	}
}

server {
	listen 8080 ssl;

	server_name example.server.com; # change your domain name

	ssl on;
	ssl_session_cache  builtin:1000  shared:SSL:10m;
	ssl_protocols  TLSv1 TLSv1.1 TLSv1.2;
	ssl_ciphers HIGH:!aNULL:!eNULL:!EXPORT:!CAMELLIA:!DES:!MD5:!PSK:!RC4;
	ssl_prefer_server_ciphers on;

	ssl_certificate /etc/nginx/ssl/nginx.crt;
	ssl_certificate_key /etc/nginx/ssl/nginx.key;

	location / {
		  ## proxy setting for RWS(Rpi-WebRTC-Streamer)
		  ## it will bind 8888 port on localhost
          proxy_pass          http://localhost:8888
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
#### Nginx with Client Side Certificate Auth as a Reverse Proxy

```
###
### Nginx with Client Side Certificate Auth as a Reverse Proxy
###
server {
    listen 80;
    server_name example.server.com;
    rewrite ^(.*) https://$server_name$1 permanent;
}

server {
	#listen 80 default_server;
	#listen [::]:80 default_server ipv6only=on;
	listen 443 ssl default_server;

	server_name example.server.com;

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

	root /usr/share/nginx/html;
	index index.html index.htm;

	# Make site accessible from http://localhost/
	server_name localhost;

	location / {
		# First attempt to serve request as file, then
		# as directory, then fall back to displaying a 404.
		try_files $uri $uri/ =404;
		# Uncomment to enable naxsi on this location
		# include /etc/nginx/naxsi.rules
	}
}

server {
	listen 8080 ssl;

	server_name example.server.com;

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

	location / {
		  ## proxy setting for RWS(Rpi-WebRTC-Streamer)
		  ## it will bind 8888 port on localhost
		  proxy_pass          http://localhost:8888;
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

## Running RWS
RWS has not been created the scripts for a process monitoring and start/restart. You can simply enter the following command to start RWS.

```
webrtc-streamer >& /dev/null &
```
This command will start listening at localhost: 8888.

#### installing example html for testing
In order to test the RMS, we need to copy the html file to nginx html directory.
```
cd ~/Workspace/rpi-webrtc-streamer/misc
sudo mkdir /usr/share/nginx/html/rms
sudo cp native-to-browser-test.html /usr/share/nginx/html/rms
```

#### Testing RMS in browser
To test RMS WebRTC streaming , Open the following URL in the latest chrome or firefox browser.
```
https://server.example.com/rms/native-to-browser-test.html
```

To use the h.264 webrtc streaming in chrome or firefox, browser settings are required for each browser.
For the chrome browser case: you need to to enable the '#enable-webrtc-h264-with-openh264-ffmpeg' entry in the 'chrome//flags' URL. For more information, please refer to the googling or [PSA](https://groups.google.com/forum/#!topic/discuss-webrtc/8ov4YW6HLgo).

For the firefox browser case: Please refer to [Checking H264 Capability on Firefox](https://github.com/EricssonResearch/openwebrtc/issues/425).

#### RMS testing web log
The following log example is the log message of the native-to-browser-test.html between chrome browser and RMS.

```
My id: 2
Peer 1: RaspiStreamer,1,1

Message from 'RaspiStreamer:{
   "sdp" : "v=0\r\no=- 8242741994528445919 2 IN IP4 127.0.0.1\r\ns=-\r\nt=0 0\r\na=group:BUNDLE audio video\r\na=msid-semantic: WMS stream_label\r\nm=audio 9 UDP/TLS/RTP/SAVPF 111 103 9 102 0 8 105 13 126\r\nc=IN IP4 0.0.0.0\r\na=rtcp:9 IN IP4 0.0.0.0\r\na=ice-ufrag:PWR5\r\na=ice-pwd:fVPLNmnOc8srJfaZP1xO6Rlx\r\na=fingerprint:sha-256 AD:7B:05:AB:E6:2E:61:0B:C1:FF:F6:5F:8A:29:DC:05:A1:2F:FA:9C:BF:23:8A:AC:90:5F:42:3C:0D:A0:87:F1\r\na=setup:actpass\r\na=mid:audio\r\na=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\na=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\na=sendrecv\r\na=rtcp-mux\r\na=rtpmap:111 opus/48000/2\r\na=rtcp-fb:111 transport-cc\r\na=fmtp:111 minptime=10;useinbandfec=1\r\na=rtpmap:103 ISAC/16000\r\na=rtpmap:9 G722/8000\r\na=rtpmap:102 ILBC/8000\r\na=rtpmap:0 PCMU/8000\r\na=rtpmap:8 PCMA/8000\r\na=rtpmap:105 CN/16000\r\na=rtpmap:13 CN/8000\r\na=rtpmap:126 telephone-event/8000\r\na=ssrc:1933314036 cname:uvBZPWChpTJkNsiQ\r\na=ssrc:1933314036 msid:stream_label audio_label\r\na=ssrc:1933314036 mslabel:stream_label\r\na=ssrc:1933314036 label:audio_label\r\nm=video 9 UDP/TLS/RTP/SAVPF 100 101 107 116 117 96 97 99 98\r\nc=IN IP4 0.0.0.0\r\na=rtcp:9 IN IP4 0.0.0.0\r\na=ice-ufrag:PWR5\r\na=ice-pwd:fVPLNmnOc8srJfaZP1xO6Rlx\r\na=fingerprint:sha-256 AD:7B:05:AB:E6:2E:61:0B:C1:FF:F6:5F:8A:29:DC:05:A1:2F:FA:9C:BF:23:8A:AC:90:5F:42:3C:0D:A0:87:F1\r\na=setup:actpass\r\na=mid:video\r\na=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\na=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\na=extmap:4 urn:3gpp:video-orientation\r\na=extmap:5 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\r\na=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay\r\na=sendrecv\r\na=rtcp-mux\r\na=rtcp-rsize\r\na=rtpmap:100 VP8/90000\r\na=rtcp-fb:100 ccm fir\r\na=rtcp-fb:100 nack\r\na=rtcp-fb:100 nack pli\r\na=rtcp-fb:100 goog-remb\r\na=rtcp-fb:100 transport-cc\r\na=rtpmap:101 VP9/90000\r\na=rtcp-fb:101 ccm fir\r\na=rtcp-fb:101 nack\r\na=rtcp-fb:101 nack pli\r\na=rtcp-fb:101 goog-remb\r\na=rtcp-fb:101 transport-cc\r\na=rtpmap:107 H264/90000\r\na=rtcp-fb:107 ccm fir\r\na=rtcp-fb:107 nack\r\na=rtcp-fb:107 nack pli\r\na=rtcp-fb:107 goog-remb\r\na=rtcp-fb:107 transport-cc\r\na=fmtp:107 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\r\na=rtpmap:116 red/90000\r\na=rtpmap:117 ulpfec/90000\r\na=rtpmap:96 rtx/90000\r\na=fmtp:96 apt=100\r\na=rtpmap:97 rtx/90000\r\na=fmtp:97 apt=101\r\na=rtpmap:99 rtx/90000\r\na=fmtp:99 apt=107\r\na=rtpmap:98 rtx/90000\r\na=fmtp:98 apt=116\r\na=ssrc-group:FID 2784292059 2686641453\r\na=ssrc:2784292059 cname:uvBZPWChpTJkNsiQ\r\na=ssrc:2784292059 msid:stream_label video_label\r\na=ssrc:2784292059 mslabel:stream_label\r\na=ssrc:2784292059 label:video_label\r\na=ssrc:2686641453 cname:uvBZPWChpTJkNsiQ\r\na=ssrc:2686641453 msid:stream_label video_label\r\na=ssrc:2686641453 mslabel:stream_label\r\na=ssrc:2686641453 label:video_label\r\n",
   "type" : "offer"
}

Prefer video send codec: H264/90000
mLineIndex Line: m=video 9 UDP/TLS/RTP/SAVPF 100 101 107 116 117 96 97 99 98
found Prefered Codec in : 61: a=rtpmap:107 H264/90000
** Modified LineIndex Line: m=video 9 UDP/TLS/RTP/SAVPF 107 100 101 116 117 96 97 99 98
Passing Offer SDP: v=0
o=- 8242741994528445919 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE audio video
a=msid-semantic: WMS stream_label
m=audio 9 UDP/TLS/RTP/SAVPF 111 103 9 102 0 8 105 13 126
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:PWR5
a=ice-pwd:fVPLNmnOc8srJfaZP1xO6Rlx
a=fingerprint:sha-256 AD:7B:05:AB:E6:2E:61:0B:C1:FF:F6:5F:8A:29:DC:05:A1:2F:FA:9C:BF:23:8A:AC:90:5F:42:3C:0D:A0:87:F1
a=setup:actpass
a=mid:audio
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=sendrecv
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=10;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:9 G722/8000
a=rtpmap:102 ILBC/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:126 telephone-event/8000
a=ssrc:1933314036 cname:uvBZPWChpTJkNsiQ
a=ssrc:1933314036 msid:stream_label audio_label
a=ssrc:1933314036 mslabel:stream_label
a=ssrc:1933314036 label:audio_label
m=video 9 UDP/TLS/RTP/SAVPF 107 100 101 116 117 96 97 99 98
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:PWR5
a=ice-pwd:fVPLNmnOc8srJfaZP1xO6Rlx
a=fingerprint:sha-256 AD:7B:05:AB:E6:2E:61:0B:C1:FF:F6:5F:8A:29:DC:05:A1:2F:FA:9C:BF:23:8A:AC:90:5F:42:3C:0D:A0:87:F1
a=setup:actpass
a=mid:video
a=extmap:2 urn:ietf:params:rtp-hdrext:toffset
a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:4 urn:3gpp:video-orientation
a=extmap:5 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=sendrecv
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:100 VP8/90000
a=rtcp-fb:100 ccm fir
a=rtcp-fb:100 nack
a=rtcp-fb:100 nack pli
a=rtcp-fb:100 goog-remb
a=rtcp-fb:100 transport-cc
a=rtpmap:101 VP9/90000
a=rtcp-fb:101 ccm fir
a=rtcp-fb:101 nack
a=rtcp-fb:101 nack pli
a=rtcp-fb:101 goog-remb
a=rtcp-fb:101 transport-cc
a=rtpmap:107 H264/90000
a=rtcp-fb:107 ccm fir
a=rtcp-fb:107 nack
a=rtcp-fb:107 nack pli
a=rtcp-fb:107 goog-remb
a=rtcp-fb:107 transport-cc
a=fmtp:107 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
a=rtpmap:116 red/90000
a=rtpmap:117 ulpfec/90000
a=rtpmap:96 rtx/90000
a=fmtp:96 apt=100
a=rtpmap:97 rtx/90000
a=fmtp:97 apt=101
a=rtpmap:99 rtx/90000
a=fmtp:99 apt=107
a=rtpmap:98 rtx/90000
a=fmtp:98 apt=116
a=ssrc-group:FID 2784292059 2686641453
a=ssrc:2784292059 cname:uvBZPWChpTJkNsiQ
a=ssrc:2784292059 msid:stream_label video_label
a=ssrc:2784292059 mslabel:stream_label
a=ssrc:2784292059 label:video_label
a=ssrc:2686641453 cname:uvBZPWChpTJkNsiQ
a=ssrc:2686641453 msid:stream_label video_label
a=ssrc:2686641453 mslabel:stream_label
a=ssrc:2686641453 label:video_label

After setLocal Answer SDP: v=0
o=- 7993802214311095253 2 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE audio video
a=msid-semantic: WMS
m=audio 9 UDP/TLS/RTP/SAVPF 111 103 9 0 8 105 13 126
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:OSRp
a=ice-pwd:pbaYa8n2Nrjqgtl1UrRsLPxX
a=fingerprint:sha-256 23:B2:D7:37:20:5E:71:4F:75:E1:1E:24:D5:A0:5C:FA:20:51:08:E1:83:2C:79:F2:F8:4C:C9:04:C5:9B:8E:83
a=setup:active
a=mid:audio
a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=recvonly
a=rtcp-mux
a=rtpmap:111 opus/48000/2
a=rtcp-fb:111 transport-cc
a=fmtp:111 minptime=10;useinbandfec=1
a=rtpmap:103 ISAC/16000
a=rtpmap:9 G722/8000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:105 CN/16000
a=rtpmap:13 CN/8000
a=rtpmap:126 telephone-event/8000
m=video 9 UDP/TLS/RTP/SAVPF 107 100 101 116 117 96 97 99 98
c=IN IP4 0.0.0.0
a=rtcp:9 IN IP4 0.0.0.0
a=ice-ufrag:OSRp
a=ice-pwd:pbaYa8n2Nrjqgtl1UrRsLPxX
a=fingerprint:sha-256 23:B2:D7:37:20:5E:71:4F:75:E1:1E:24:D5:A0:5C:FA:20:51:08:E1:83:2C:79:F2:F8:4C:C9:04:C5:9B:8E:83
a=setup:active
a=mid:video
a=extmap:2 urn:ietf:params:rtp-hdrext:toffset
a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
a=extmap:4 urn:3gpp:video-orientation
a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
a=recvonly
a=rtcp-mux
a=rtcp-rsize
a=rtpmap:107 H264/90000
a=rtcp-fb:107 ccm fir
a=rtcp-fb:107 nack
a=rtcp-fb:107 nack pli
a=rtcp-fb:107 goog-remb
a=rtcp-fb:107 transport-cc
a=fmtp:107 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
a=rtpmap:100 VP8/90000
a=rtcp-fb:100 ccm fir
a=rtcp-fb:100 nack
a=rtcp-fb:100 nack pli
a=rtcp-fb:100 goog-remb
a=rtcp-fb:100 transport-cc
a=rtpmap:101 VP9/90000
a=rtcp-fb:101 ccm fir
a=rtcp-fb:101 nack
a=rtcp-fb:101 nack pli
a=rtcp-fb:101 goog-remb
a=rtcp-fb:101 transport-cc
a=rtpmap:116 red/90000
a=rtpmap:117 ulpfec/90000
a=rtpmap:96 rtx/90000
a=fmtp:96 apt=100
a=rtpmap:97 rtx/90000
a=fmtp:97 apt=101
a=rtpmap:99 rtx/90000
a=fmtp:99 apt=107
a=rtpmap:98 rtx/90000
a=fmtp:98 apt=116

Message from 'RaspiStreamer:{
   "candidate" : "candidate:946015839 1 udp 2122260223 10.0.0.11 42349 typ host generation 0 ufrag PWR5 network-id 1 network-cost 50",
   "sdpMLineIndex" : 0,
   "sdpMid" : "audio"
}

Message from 'RaspiStreamer:{
   "candidate" : "candidate:946015839 2 udp 2122260222 10.0.0.11 36718 typ host generation 0 ufrag PWR5 network-id 1 network-cost 50",
   "sdpMLineIndex" : 0,
   "sdpMid" : "audio"
}

Message from 'RaspiStreamer:{
   "candidate" : "candidate:946015839 1 udp 2122260223 10.0.0.11 40132 typ host generation 0 ufrag PWR5 network-id 1 network-cost 50",
   "sdpMLineIndex" : 1,
   "sdpMid" : "video"
}

Message from 'RaspiStreamer:{
   "candidate" : "candidate:946015839 2 udp 2122260222 10.0.0.11 41408 typ host generation 0 ufrag PWR5 network-id 1 network-cost 50",
   "sdpMLineIndex" : 1,
   "sdpMid" : "video"
}

Message from 'RaspiStreamer:{
   "candidate" : "candidate:2062479575 2 udp 1686052606 xxx.xxx.xxx.xxx 41408 typ srflx raddr 10.0.0.11 rport 41408 generation 0 ufrag PWR5 network-id 1 network-cost 50",
   "sdpMLineIndex" : 1,
   "sdpMid" : "video"
}

Message from 'RaspiStreamer:{
   "candidate" : "candidate:2062479575 2 udp 1686052606 xxx.xxx.xxx.xxx 36718 typ srflx raddr 10.0.0.11 rport 36718 generation 0 ufrag PWR5 network-id 1 network-cost 50",
   "sdpMLineIndex" : 0,
   "sdpMid" : "audio"
}

Message from 'RaspiStreamer:{
   "candidate" : "candidate:2062479575 1 udp 1686052607 xxx.xxx.xxx.xxx 42349 typ srflx raddr 10.0.0.11 rport 42349 generation 0 ufrag PWR5 network-id 1 network-cost 50",
   "sdpMLineIndex" : 0,
   "sdpMid" : "audio"
}

Message from 'RaspiStreamer:{
   "candidate" : "candidate:2062479575 1 udp 1686052607 xxx.xxx.xxx.xxx 40132 typ srflx raddr 10.0.0.11 rport 40132 generation 0 ufrag PWR5 network-id 1 network-cost 50",
   "sdpMLineIndex" : 1,
   "sdpMid" : "video"
}
```

## Version History
 * 2016/09/20 v0.56 : Initial Version


