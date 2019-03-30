FROM ubuntu:18.04
ENV DEBIAN_FRONTEND=noninteractive
ENV HOME /root/

RUN apt update
RUN apt install -y git \
   wget \ 
   xz-utils \
   python \ 
   openssh-client \
   sshpass \
   pkg-config \
   rsync \
   cmake \ 
   unzip \
    gawk

RUN mkdir -p /root/tools
WORKDIR /root/tools
RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
ENV PATH="${PATH}:/root/tools/depot_tools"

RUN mkdir -p /root/Workspace
WORKDIR /root/Workspace
RUN git clone https://github.com/kclyu/rpi_rootfs

RUN mkdir /root/Workspace/rpi_rootfs/tools
WORKDIR /root/Workspace/rpi_rootfs/tools

#RUN wget -O arm-linux-gnueabihf.tar.xz https://www.dropbox.com/s/zom2m8vwkcryn1e/gcc-linaro-6.4.1-2017.01-x86_64_arm-linux-gnueabihf.tar.xz%3Fdl%3D0?dl=0  && \
ADD gcc-linaro-6.4.1-2018.10-x86_64_arm-linux-gnueabihf.tar.xz .
RUN    ln -sf gcc-linaro-6.4.1-2018.10-x86_64_arm-linux-gnueabihf  arm-linux-gnueabihf && \
    cd /opt &&  ln -sf /root/Workspace/rpi_rootfs
ENV PATH="${PATH}:/opt/rpi_rootfs/tools/arm-linux-gnueabihf/bin"

RUN mkdir -p /root/Workspace/webrtc
WORKDIR /root/Workspace/webrtc
RUN fetch --nohooks webrtc && \ 
    gclient sync && \
    cd /root/Workspace/webrtc/src && \
    git config branch.autosetupmerge always && \
    git config branch.autosetuprebase always && \
    git checkout master


# Installing ssh client to connect to raspberry pi
RUN RUN echo -e "\n\n\n\n" | ssh-keygen -t rsa -N '' -f /root/.ssh/id_rsa && \
    sshpass -p raspberry ssh-copy-id -oStrictHostKeyChecking=no pi@raspberrypi.local

#hacks to ./install-build-deps.sh work
#we haven't set up sudo so we will remove it
#lsb_release is not available in ubuntu docker images
RUN cd /root/Workspace/webrtc/src/build && \
    sed 's/sudo//g' install-build-deps.sh > install-build-deps.sh && \
    sed 's/$(lsb_release --codename --short)/bionic/g' install-build-deps.sh > install-build-deps.sh && \
    sed 's/$(lsb_release --id --short)/Ubuntu/g' install-build-deps.sh > install-build-deps.sh && \
    chmod +x install-build-deps.sh  && \
    ./install-build-deps.sh 

RUN cd /root/Workspace/rpi_rootfs && \
    ./rpi_rootfs.py pi@raspberrypi.local ./

WORKDIR /root/Workspace
#RUN git clone https://github.com/kclyu/rpi-webrtc-streamer
ADD . ./rpi-webrtc-streamer
WORKDIR /root/Workspace/webrtc/src
RUN mkdir arm_build
RUN cp /root/Workspace/rpi-webrtc-streamer/misc/webrtc_arm_build_args.gn arm_build/args.gn
RUN gn gen arm_build && ninja -C arm_build

WORKDIR /root/Workspace/rpi-webrtc-streamer/src
RUN sh ../mk/config_libwebsockets.sh
RUN make

# Deploy
WORKDIR /root/Workspace/rpi-webrtc-streamer/src
RUN apt install -y cmake unzip gawk
RUN ssh pi@raspberrypi.local  mkdir -p /home/pi/Workspace/rws
RUN scp -r etc tools log web-root webrtc-streamer pi@raspberrypi.local:/home/pi/Workspace/rws
