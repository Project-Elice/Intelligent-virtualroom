# Copyright (C) 2019 Intel Corporation
#
# Classroom Analytics docker file

FROM ubuntu:16.04
ARG INSTALL_DIR=/opt/intel/openvino
ARG TEMP_DIR=/tmp/openvino_installer
RUN mkdir -p $TEMP_DIR && cd $TEMP_DIR 
COPY l_openvino_toolkit*.tgz .

# COMMON BUILD TOOLS
RUN DEBIAN_FRONTEND=noninteractive apt-get update && apt-get install -y -q --no-install-recommends \
    build-essential \
    autoconf \
    automake \
    make \
    git \
    cpio \
    pciutils \
    wget \
    python3-pip \
    python3-setuptools \
    cmake \
    vim \
    sudo \ 
    curl \
    libboost-all-dev \
    lsb-release && \
    rm -rf /var/lib/apt/lists/*

#Install OpenVINO R3
RUN tar xf l_openvino_toolkit*.tgz && \
    cd l_openvino_toolkit* && \
    sed -i 's/decline/accept/g' silent.cfg && \
    ./install.sh -s silent.cfg && \
    rm -rf $TEMP_DIR

RUN chmod +x $INSTALL_DIR/install_dependencies/install_openvino_dependencies.sh && \
     cd $INSTALL_DIR/install_dependencies && \
     ./install_openvino_dependencies.sh && \
     ./install_NEO_OCL_driver.sh


RUN apt-get update && \
    apt-get install -y libboost-filesystem1.58.0 libboost-thread1.58.0

HEALTHCHECK --interval=5m --timeout=3s --retries=5 \
  CMD curl -f http://localhost/ || exit 1

#Copy RI and data to the container folder 
COPY classroom_analytics/ $INSTALL_DIR/inference_engine/samples/classroom_analytics/
COPY resources/ /resources/

# build RI and Inference Engine samples
RUN /bin/bash -c "source $INSTALL_DIR/bin/setupvars.sh"
RUN /bin/bash -c "$INSTALL_DIR/inference_engine/samples/build_samples.sh"

#Install Python packages and Download Required models 
RUN pip3 install requests && \
    pip3 install pyyaml 

RUN cd /resources && \
    python3 $INSTALL_DIR/deployment_tools/tools/model_downloader/downloader.py --list $INSTALL_DIR/inference_engine/samples/classroom_analytics/models.LST && \
    cd $INSTALL_DIR/inference_engine/samples/classroom_analytics/ && \
    python3 create_list.py /resources/
