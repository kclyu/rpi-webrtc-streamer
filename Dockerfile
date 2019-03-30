# Defining environment
ARG ARCH=armv7
ARG RPI=pi@raspberrypi.local

FROM ubuntu:18.04 AS osbase

ENV DEBIAN_FRONTEND=noninteractive
ENV HOME /root/
RUN apt update &&  apt install -y git \
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

RUN wget -O gcc-linaro-6.4.1-2018.10-x86_64_arm-linux-gnueabihf.tar.xz https://www.dropbox.com/s/qf6c68eia8478zk/gcc-linaro-6.4.1-2018.10-x86_64_arm-linux-gnueabihf.tar.xz?dl=1
RUN xz -dc gcc-linaro-6.4.1-2018.10-x86_64_arm-linux-gnueabihf.tar.xz | tar xvf -
RUN ln -sf gcc-linaro-6.4.1-2018.10-x86_64_arm-linux-gnueabihf  arm-linux-gnueabihf && \
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
RUN echo -e "\n\n\n\n" | ssh-keygen -t rsa -N '' -f /root/.ssh/id_rsa && \
    sshpass -p raspberry ssh-copy-id -oStrictHostKeyChecking=no pi@raspberrypi.local && \
    ssh -t pi@raspberrypi.local 'sudo apt-get update && sudo apt-get upgrade -y && \
    sudo apt-get install -y rsync libasound2-dev libcairo2-dev  libffi-dev libglib2.0-dev  \
    libgtk2.0-dev libpam0g-dev libpulse-dev  libudev-dev  libxtst-dev  \
    ttf-dejavu-core libatk1.0-0 libc6 libasound2  libcairo2 libcap2 libcups2  \
    libexpat1 libffi6 libfontconfig1 libfreetype6  libglib2.0-0 libgnome-keyring0  \
    libgtk2.0-0 libpam0g libpango1.0-0  libpcre3 libpixman-1-0 libpng12-0 libstdc++6  \
    libx11-6 libx11-xcb1 libxau6 libxcb1 libxcomposite1 libxcursor1 libxdamage1   \
    libxdmcp6 libxext6 libxfixes3 libxi6 libxinerama1 libxrandr2 libxrender1  \
    libxtst6 zlib1g gtk+-3.0 libavahi-client-dev'

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

FROM osbase AS Prod
WORKDIR /root/Workspace/webrtc/src
RUN mkdir arm_build
COPY ./misc/webrtc_arm_build_args.gn arm_build/args.gn
RUN gn gen arm_build && ninja -C arm_build

#FROM Prod AS  rws-install
#WORKDIR /root/Workspace
#ADD . ./rpi-webrtc-streamer
#WORKDIR /root/Workspace/rpi-webrtc-streamer/src
#RUN sh ../mk/config_libwebsockets.sh
#RUN make

# Deploy
#WORKDIR /root/Workspace/rpi-webrtc-streamer/src
#RUN ssh pi@raspberrypi.local  mkdir -p /home/pi/Workspace/rws
#RUN scp -r etc tools log web-root webrtc-streamer pi@raspberrypi.local:/home/pi/Workspace/rws
