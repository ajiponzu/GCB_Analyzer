FROM ubuntu:22.04

# tools

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
	&& apt-get install -y \
	git

RUN apt-get update \
	&& apt-get install -y tzdata

RUN apt-get update \
	&& apt-get install -y build-essential cmake python3-numpy python3-dev python3-tk libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libdc1394-dev libeigen3-dev libgtk-3-dev libvtk7-qt-dev ffmpeg ninja-build libjsoncpp-dev uuid-dev zlib1g-dev openssl libssl-dev ccache \
	&& apt-get -y clean \
	&& rm -rf /var/lib/apt/lists/*

# opencv

RUN cd ~ \
	&& git clone https://github.com/opencv/opencv.git \
	&& cd opencv \
	&& git checkout 4.7.0 \
	&& mkdir build \
	&& cd build \
	&& cmake -GNinja -D CMAKE_BUILD_TYPE=Release -D WITH_FFMPEG=ON -D OPENCV_FFMPEG_USE_FIND_PACKAGE=ON -D BUILD_TESTS=OFF -D BUILD_PERF_TESTS=OFF -D BUILD_opencv_calib3d=OFF -D BUILD_opencv_dnn=OFF -D BUILD_opencv_features2d=OFF -D BUILD_opencv_flann=OFF -D BUILD_opencv_gapi=OFF -D BUILD_opencv_ml=OFF -D BUILD_opencv_objdetect=OFF -D BUILD_opencv_photo=OFF -D BUILD_opencv_stitching=OFF -D BUILD_opencv_video=OFF -D CMAKE_INSTALL_PREFIX=/usr/local -D WITH_TBB=ON -D WITH_OPENGL=ON -D OPENCV_GENERATE_PKGCONFIG=ON .. \
	&& ninja -j$(nproc) && ninja install \
	&& ldconfig 

# drogon

RUN cd ~ \
	&& git clone https://github.com/drogonframework/drogon \
	&& cd drogon \
	&& git submodule update --init \
	&& mkdir build \
	&& cd build \
	&& cmake -GNinja .. \
	&& ninja && ninja install \
	&& ldconfig

# workspace

RUN cd ~ \
	&& mkdir myapp
WORKDIR /myapp
COPY . /myapp/
RUN rm -rf /myapp/build
RUN cd /myapp \
	&& chmod +x execute.sh \
	&& mkdir build \
	&& sh build.sh

ENTRYPOINT [ "./execute.sh" ] 