#!/bin/bash
# ==============================================================================
# Copyright (C) 2019 Intel Corporation
#
# Launch script to Install and run the intelligent classroom analytics application
# ==============================================================================

OPENVINO_WGET_URL=registrationcenter-download.intel.com/akdlm/irc_nas/15693/l_openvino_toolkit_p_2019.3.334.tgz
VIDEO_PATH=https://raw.githubusercontent.com/intel-iot-devkit/sample-videos/master/classroom.mp4
file=l_openvino_toolkit_p_2019.3.334.tgz
videofile=resources/classroom.mp4

classroom="9B"
camera="/resources/classroom.mp4"
influxIp="172.21.0.6"

while getopts 'r:c:i:h' OPTION; do
  case "$OPTION" in
    r)
      classroom="$OPTARG"
      ;;

    c)
      camera="$OPTARG"
      ;;

    i)
      influxIp="$OPTARG"
      ;;
    h)
      echo -e "script usage: $(basename $0) [-r Classroom Name] [-c Ip Camera Link] [-i InfluxDB Ip] \n" >&2
      exit 1
      ;;
    ?)
      echo "script usage: $(basename $0) [-r Classroom Name] [-c Ip Camera Link] [-i InfluxDB Ip] " >&2
      exit 1
      ;;
  esac
done

echo -e "\nThe Camera link provided for Classroom $classroom is $camera\n"

check_yes_no(){
while true; do
    echo "Do you wish to install Classroom Analytics Container?" 
    echo "Please choose 1 for 'Yes' and 2 for 'No'"
    echo "1) Yes"
    echo "2) No"

    read yn
    case $yn in
        1 )
           echo "Installing Classroom Analytics Container, Please Wait.."
        break;;
        2 )
           echo "Exiting from installing Classroom Analytics Container."
        exit;;
        * )
        echo "Entered incorrect input : ${yn}"
    esac
done
}

#------------------------------------------------------------------------------
# proxy_enabled_network
#
# Description:
#        Configure proxy settings for docker client and docker daemon to connect
#        to internet and also for containers to access internet
# Usage:
#        proxy_enabled_network
#-------------------------------------------------------------------------------

proxy_enabled_network()
{
    # 1. Configure the Docker client
    USER_PROXY=""
    echo "Please enter your proxy address (Ex: <proxy.example.com>:<port_number>):"
    read USER_PROXY
    while [ -z "${USER_PROXY}" ]
    do
        echo "${_red}Proxy is empty, please enter again${_reset}"
        read USER_PROXY
    done

echo "{
 \"proxies\":
  {
  \"default\":
   {
   \"httpProxy\": \"http://${USER_PROXY}\",
   \"httpsProxy\": \"http://${USER_PROXY}\",
   \"noProxy\": \"127.0.0.1,localhost\"
   }
  }
}" > ~/.docker/config.json

    # 2. HTTP/HTTPS proxy
    if [ -d /etc/systemd/system/docker.service.d ];then
            sudo rm -rf /etc/systemd/system/docker.service.d
    fi
    if [ ! -d /etc/systemd/system/docker.service.d ];then
        sudo mkdir -p /etc/systemd/system/docker.service.d
        sudo touch /etc/systemd/system/docker.service.d/http-proxy.conf
        sudo touch /etc/systemd/system/docker.service.d/https-proxy.conf
    else
        if [ ! -f /etc/systemd/system/docker.service.d/http-proxy.conf ];then
            sudo touch /etc/systemd/system/docker.service.d/http-proxy.conf
            sudo touch /etc/systemd/system/docker.service.d/https-proxy.conf
        fi
    fi
sudo bash -c "cat > /etc/systemd/system/docker.service.d/http-proxy.conf" <<EOF
[Service]
Environment="HTTP_PROXY=http://${USER_PROXY}/" "NO_PROXY=localhost,127.0.0.1"
EOF
sudo bash -c "cat > /etc/systemd/system/docker.service.d/https-proxy.conf" <<EOF
[Service]
Environment="HTTP_PROXY=http://${USER_PROXY}/" "NO_PROXY=localhost,127.0.0.1"
EOF

    # Flush the changes
    sudo systemctl daemon-reload
    # Restart docker
    sudo systemctl restart docker

    return 0
}

#------------------------------------------------------------------------------
# dns_server_setting
#
# Description:
#        Updating correct DNS server details in /etc/resolv.conf
# Usage:
#        dns_server_setting
#------------------------------------------------------------------------------
dns_server_settings()
{
    UBUNTU_VERSION=`grep "DISTRIB_RELEASE" /etc/lsb-release | cut -d "=" -f2`

    echo "${_green}Updating correct DNS server details in /etc/resolv.conf${_reset}"
    # DNS server settings for Ubuntu 16.04 or earlier
    VERSION_COMPARE=`echo "${UBUNTU_VERSION} <= 16.04" | bc`
    if [  ${VERSION_COMPARE} -eq "1" ];then
        if [ -f /etc/NetworkManager/NetworkManager.conf ];then
            grep "#dns=dnsmasq" /etc/NetworkManager/NetworkManager.conf
            if [ $? -ne "0" ];then
                sudo sed -i 's/dns=dnsmasq/#dns=dnsmasq/g' /etc/NetworkManager/NetworkManager.conf
                sudo systemctl restart network-manager.service
		sudo sed -i 's/#dns=dnsmasq/dns=dnsmasq/g' /etc/NetworkManager/NetworkManager.conf
                #Verify on the host
                echo "${_green}Udpated DNS server details on host machine${_reset}"
                cat /etc/resolv.conf
            fi
        fi
    fi

    return 0
}

#------------------------------------------------------------------------------
# proxy_settings
#
# Description:
#        Configuring proxy if user in proxy enabled network else
#        the setup will be done with no-proxy settings
# Usage:
#        proxy_settings
#------------------------------------------------------------------------------

proxy_settings()
{
    # Prompt the user for proxy address
    while :
    do
        echo "Is this system in Proxy enabled network?"
        echo "Please choose 1 for 'Yes' and 2 for 'No'"
        echo "1) Yes"
        echo "2) No"

        read yn

        case ${yn} in
            1)
                #Creating config.json
                if [ ! -d ~/.docker ];then
                    mkdir ~/.docker
                fi
                if [ ! -f ~/.docker/config.json ];then
                    touch ~/.docker/config.json
                    chmod 766 ~/.docker/config.json
                fi
                echo "${_green}Configuring proxy setting in the system${_reset}"
                echo "Docker services will be restarted after configuring proxy"
                proxy_enabled_network
                check_for_errors "$?" "Failed to configure proxy settings on the system." \
                    "${_green}Configured proxy settings in the system successfully.${_reset}"
                dns_server_settings
                check_for_errors "$?" "Failed to configure DNS server settings on the system." \
                    "${_green}Configured DNS server settings in the system successfully.${_reset}"
                break;;
            2)
                echo "${_green}Continuing the setup with system network settings.${_reset}"
                break;;
            *)
                echo "Entered incorrect input : ${yn}"
        esac
    done

    return 0
}

#------------------------------------------------------------------
# check_for_errors
#
# Description:
#        Check the return code of previous command and display either
#        success or error message accordingly.
#        if there is an error.
# Args:
#        string : return-code
#        string : failure message to display if return-code is non-zero
#        string : success message to display if return-code is zero (optional)
# Return:
#       None
# Usage:
#        check_for_errors "return-code" "failure-message" <"success-message">
#------------------------------------------------------------------


check_for_errors()
{
    return_code=${1}
    error_msg=${2}
    if [ "${return_code}" -ne 0 ];then
        echo "${_red}ERROR : (Error Code: ${return_code}) ${error_msg}${_reset}"
        exit 1
    else
        if [ "$#" -ge 3 ];then
            success_msg=${3}
            echo ${success_msg}
        fi
    fi
    return 0
}

echo -e "This Script will install and run Classroom Analytics application \n"
proxy_settings
check_yes_no

    if [ -f "$file" ];then
	echo "$file found."
    else
	echo "$file not found."
        echo "-----------------------------------"
	echo " ${_green}Downloading the OpenVINO tool kit${_reset} "
        echo "-----------------------------------"
        echo "Downloading OpenVINO. Please wait."
	wget ${OPENVINO_WGET_URL}
	check_for_errors "$?" "OpenVINO toolkit tar file download failed" \
                 "OpenVINO toolkit tar file download completed"
    fi

    if [ -f "$videofile" ];then
        echo "$videofile found."
    else
	echo "Downloading classroom video file. Please wait."
	wget ${VIDEO_PATH}
	check_for_errors "$?" "classroom video file download failed"
	mv classroom.mp4 resources/
    fi
sudo chmod a+x *.sh

# if the container image doesn't exist locally
if [[ "$(docker images -q classroom-analytics 2> /dev/null)" == "" ]]; then
    echo -e 'Classroom Analytics Docker Image Does not exist : Building Images\n'
    bash setup.sh
fi
echo -e 'Starting Docker Containers For the Reference Implementation\n'
docker-compose up -d 
containerId=$(docker ps -qf "name=classroom-analytics")
echo -e "\nContainerId of the Classroom Analytics Container: $containerId\n"
echo -e 'Running the application .......\n'

xhost +
echo -e "\n"
echo -ne '###################                     (33%)\r'
sleep 10
echo -ne '######################################             (66%)\r'
sleep 10
echo -ne '#########################################################   (100%)\r'
echo -ne '\n'
echo -e "\n"

docker exec -it "$containerId" /bin/bash -c "source /opt/intel/openvino/bin/setupvars.sh && \
/root/inference_engine_samples_build/intel64/Release/classroom-analytics \
-pdc=/resources/intel/person-detection-action-recognition-0005/FP32/person-detection-action-recognition-0005.xml \
-c=/resources/intel/face-detection-adas-0001/FP32/face-detection-adas-0001.xml \
-lrc=/resources/intel/landmarks-regression-retail-0009/FP32/landmarks-regression-retail-0009.xml \
-pc=/resources/intel/head-pose-estimation-adas-0001/FP32/head-pose-estimation-adas-0001.xml \
-sc=/resources/intel/emotions-recognition-retail-0003/FP32/emotions-recognition-retail-0003.xml \
-frc=/resources/intel/face-reidentification-retail-0095/FP32/face-reidentification-retail-0095.xml \
-fgp=/opt/intel/openvino/inference_engine/samples/classroom_analytics/faces_gallery.json \
-i=\"$camera\" --influxip=$influxIp --cs=$classroom"