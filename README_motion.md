
# Motion Detection

## About
Although Motion Detection through Pixel Analysis of Video Image can be used in Raspberry PI, it is not suitable to perform in Raspberry PI hardware in high resolution video or Motion Detection for Real-Time because there is a limitation on CPU performance. Fortunately, MMAL API of the Raspberry PI GPU provides H.264 video encoding and an interface to use MV(Motion Vector).

[Motion Vector](https://en.wikipedia.org/wiki/Motion_vector) plays a key role in video encoding and is actually you may decide that you have summary information about the moving subject. By using motion vector with this characteristic, Motion Detection can perform rough motion detection without depending on CPU performance in real time.

RWS is implemented to perform Motion Detection in the period when WebRTC streaming is not performed, which is the main function of RWS, and has the function of storing Motion Detected video as a file.
 
## Motion Detection Enabling 
The default value of Motion Detection feature is disabled. If motion_detection_enable is set to true in motion_config.conf file, Motion Detection function will be activated.
The following is the contents in etc/motion_config.conf template file. Please refer to the [Motion Config section](#motion_config)  below for a description of each config item.

```
motion_detection_enable=true
motion_width=1024
motion_height=768
motion_fps=30
motion_bitrate=3500
motion_clear_percent=5
motion_annotate_text=Raspi Motion %Y-%m-%d.%X
motion_clear_wait_period=5000
motion_directory=/home/pi/Videos
motion_file_prefix=motion
motion_file_size_limit=6000
motion_save_imv_file=false
blob_cancel_threshold=1
blob_tracking_threshold=10
motion_file_total_size_limit=2000
```
## How Motion Detection is working in RWS
The Motion Detection function of RWS uses MV provided by MMAL. Each MV can use one MV frame for each video frame when H.264 video frame is encoded. RWS makes this MV frame a frame-by-frame binary blob and active pixel percent.

 - Active Pixel:  When the motion vector is updated (when the position of x, y is changed), it is determined that the corresponding block is updated, and if a block having an update counter equal to or greater than the threshold value set for a predetermined period is used as the active pixel.  
 - Active Pixel Rate: The ratio of Active Pixels among all blocks
 - Binary Blob (Active Pixel Binary blob): A blob created using Active Pixel that looks for groups of connected pixels
 - Active Blob : If the previous blob and the new blob intersect each other and the number of intersections rises more than a certain number of times, it is regarded as an active blob.   

RWS creates a binary blob for every MV frame and determines that there is an actual motion when there is an active blob among blobs. If the Active Blob disappears further and the Active Pixel Rate falls below the specified value, it is judged that there is no more motion.  The logic for Motion Clear now uses two blends (Active Blob and Active Pixel Rate) because the blobs disappear when the moving object stops for a certain period of time.  

## <a name="motion_config"></a>Motion configuration file

|config|Value|Description|
|----------------|-----------------|--------------|
|motion_detection_enable|boolean| enable/disalbe motion detection feature ( default value is 'false')|
|motion_width|video width| specify the width of motion video resolution|
|motion_height|video height| specify the height of motion video resolution|
|motion_fps|video frame rate| specify the frame rate of motion video|
|motion_bitrate|video bitrate| specify the maximum bitrate of video|
|motion_annotate_text|string| Specifies the text to use for the video annotation. If there is no '%' character in string format, the date and time string will be added in the format of HH:MM:SS DD/MM/YYYY.  If you want to specify a date time of the desired format, you can refer to [strftime](http://www.cplusplus.com/reference/ctime/strftime/). (For example, DoorGate %Y-%m-%d.%X will have 'DoorGate 2017-11-21 21:03:01' video annotation text.)|
|motion_directory|path string| specify the path of video file destination|
|motion_file_prefix|string|specify the prefix of video file name|
|motion_file_size_limit|file size|Specifies the maximum size of video files that can be saved. More than the specified size is no longer stored.|
|motion_save_imv_file|boolean| When set to true, the H.264 Inline Motion Vector is stored as a motion video file.|
|motion_file_total_size_limit|directory size|Specifies the maximum size of motion_directory in which motion video files are stored. If it is larger than the specified size, the oldest file is deleted based on the file name.|
|blob_cancel_threshold|percent|If the blob size is less than the value specified in the motion vector (IMV) blob, it is ignored without being recognized as a blob. The value is percent of the size of the blob versus video resolution.(For example, if you specify 1, blobs less than 1% of the screen will not be recognized as blobs.)|
|blob_tracking_threshold|frame counter|Specifies the number of times the recognized blob will continue to be recognized in successive frames. For example, if you specify 10, motion will be ignored if blobs are not recognized identically in consecutive 10 frames.|
|motion_clear_percent|percent|Once the motion is recognized, it determines that there is no motion if the size of the motion blob falls below the specified percent value.|
|motion_clear_wait_period|miliseconds|Specifies the retention time (miliseconds) after motion is deactivated. When motion is activated again within the retension time, the retension time is restarted after motion deactivated.|

## Motion Detection - Version History

 * 2017/11/28 : Initial Version

 

