# Intelligent Classroom

| Details            |              |
|-----------------------|---------------|
| Target OS:            |  Ubuntu\* 16.04 LTS   |
| Programming Language: |  C++ |
| Time to Complete:    |  1 hour     |

## Introduction

The aim of this Reference Implementation is to give feedback to the teachers about the class without interfering with their lecture. The Intelligent Classroom reference implementation monitors the live classroom and provides metrics for Class Attentivity, Class Participation, Happiness Index and also captures automatic attendance.

The data from each classroom can be visualized on Grafana*. The complete solution is deployed as a Docker* containers. We use different containers for each service [ ClassRoom Analytics RI, InfluxDB* Datastore, Grafana Visualizations ]


![Grafana Charts for a live classroom](/images/Grafana-dashboard.jpg)
*Fig:1: Grafana Charts for a live classroom*
## Requirements

### Hardware
* 6th to 8th generation Intel® Core™ processor with Iris® Pro graphics or Intel® HD Graphics.

* IP Camera

>If IP camera is not available, use the USB camera as mentioned [here](#testing-with-usb-camera) or use the sample classroom video file by giving file path as /data/sample.mp4

### Software
* [Ubuntu\* 16.04 LTS](http://releases.ubuntu.com/16.04/)<br>
*Note*: We recommend using a 4.14+ Linux* kernel with this software. Run install_4_14_kernel.sh located in /opt/intel/openvino/install_dependencies/ on the host machine. Run the following command to determine your kernel version:

    ```
    uname -a
    ```
* OpenCL™ Runtime Package
* Intel® Distribution of OpenVINO™ toolkit 2019 R1 release
* Docker (18.09.5)
* Docker-compose (1.24.0)
* Git
* InfluxDB (1.7.6)
* Grafana (6.1.6)

## How it works

The application uses the Inference Engine included in the Intel® Distribution of OpenVINO™ toolkit. The Reference Implementation shows an example of joint usage of several neural networks to detect the following metrics from  a live classroom:


- **Attentivity Index**: Captures the overall Attentivity of the students in the classroom. The students looking at the front towards the teacher/BlackBoard, taking notes, reading or writing is marked as attentive in the class. To calculate this metric, it uses `head-pose-estimation-adas-0001` to get the value of the head pose of each student and calculate the mean.

- **Participation Index**: Captures the real-time class participation ie: students standing for answering a query, raising a hand for asking a query. To calculate the metric it uses `person-detection-action-recognition-0005`, for finding persons and simultaneously predicting their current actions like sitting/standing/hand-raising.

- **Happiness Index** - Uses `emotions-recognition-retail-0003` pre-trained model to detect the average emotions of the students and calculate the real-time Happiness of the classroom.

- **Automatic Attendance** - Captures the total strength of the class and absentees during the class. Uses `face-reidentification-retail-0095` pre-trained model to detect the individual faces in the Classroom.


![Architecture Diagram Simple](/images/simple_architecture.png)
*Fig:2: Architecture Diagram for the Reference Implementation*

The Reference Implementation can be deployed as a Docker container for each classroom. On the start-up, the application reads command line parameters and loads six networks to the Inference Engine. Using these networks the application does inference on the frames and create quantitative metrics for the user. The metrics from each classroom are collected in an InfluxDB Datastore container and are visualized using Grafana container.


## Setup

### Download the repo

Download the repository or do a git clone of the repository.

>If git is not installed in the host system, Install it using: "_sudo apt update && apt install git_ " command.

### Pre-requisites

1. Adding Gallery for the students for the Classroom Attendance:-
    To recognize faces on a frame, the Reference Implementation needs a gallery of reference images. Add the frontal-oriented faces of student faces in the students folder inside the cloned folder. Each student can have multiple images. The images should be named as id_name.png, ...

    >The files are located at : data/students/

2. Adding the classroom timetable. The entries for the classroom can be configured in timetable.txt file.
    >The file is located at :classroom_analytics/timetable.txt

3. Download the Latest [OpenVINO R1](https://software.intel.com/en-us/openvino-toolkit/choose-download) release and copy the downloaded .tgz file to the cloned directory

4. Install OpenVINO using this [link](https://docs.openvinotoolkit.org/latest/_docs_install_guides_installing_openvino_linux.html).

5. Install NEO_OCL drivers on the host using this [link](https://docs.openvinotoolkit.org/latest/_docs_install_guides_installing_openvino_linux.html#additional-GPU-steps).

Once the required software packages are downloaded and necessary changes made. We can begin with the installation of the application.

### Install dependencies

The Reference Implementation depends on a few software packages [ docker and docker-compose] on the vanilla Ubuntu system. To install the dependencies, open the terminal, cd into the cloned repository and run the following commands:-

Run the script to download and install the software packages.

```console
cd classroom-analytics-cpp
sudo ./install_docker.sh
 ```
Run the command below to run docker commands without sudo. Log out and Log in again after running the command.

 ```console
sudo usermod -aG docker $USER
  ```
>If the cloned/downloaded files are  not executable, add executable permission by _"chmod +x (Script Name)"_

![Installer Script](images/installing_dependencies.gif)

*Fig:3: Running the installer script on the host*

Once the installation is complete, the Docker service will start automatically. This can be verified by typing:

```console
 sudo systemctl status docker
```

The output will look something like this:
```console
docker.service - Docker Application Container Engine
   Loaded: loaded (/lib/systemd/system/docker.service; enabled; vendor preset: enabled)
   Active: active (running) since Mon 2019-04-18 01:22:00 PDT; 6min ago
     Docs: https://docs.docker.com
 Main PID: 10647 (dockerd)
    Tasks: 21
   CGroup: /system.slice/docker.service
```

Test docker-compose installation.

```console
docker-compose --version

docker-compose version 1.24.0, build 1110ad01
```

### Build Containers

Once we have the dependencies installed, we will proceed with building the containers for the Classroom-analytics Reference Implementation, InfluxDB, Grafana.

Run the following command from the cloned folder.

```console
./build_ri_images.sh
```
The following script does the following tasks:-

* Builds the classroom-analytics Docker image.
* Download/Install Dependencies in the image
* Copies specific files: timetable.txt, students folder and other scripts in the image.
* Install the required pre-trained models.
* Create students list.
* Set necessary Environment variables.
* Pulls and creates Grafana and InfluxDB container images.

Once the command completes execution we should see three Docker images:

```console
docker images
```
![Docker Images on the host](/images/docker-images.png)

### Run Containers

- After building the containers, we can run the containers by using the docker-compose command. Run the following command from inside the cloned folder.

```console
docker-compose up
```
The above commands create 3 containers from the images, create bridge networks and assign IP addresses, establishes connectivity between the containers and adds persistent storage.

Now open a new terminal to log into the containers and start the application.

>You can also run "docker-compose up -d" to run the containers in the background.

![Running via Compose](images/docker-compose-up.gif)
*Fig:5: Running Containers via Docker Compose*

### Start the application

To start the application, we need to login into the Classroom-analytics Container and run the executable binary. With the docker-compose command, we ran in the previous step we should have all the three container running in the host system.

- Open New terminal and log into the Classroom analytics RI shell. To get the Container ID of your running container, use the following command.
```console
docker ps -aqf "name=classroom-analytics"
92de4bf20987
```

- Before logging into the container, run '**xhost +**' in the host terminal to give GUI access to the container.

- Login to the containers using the command
```console
docker exec -it <container ID> /bin/bash
```

- Run the classroom analytics Reference Implementation. Set the relevant attributes in the flags. ie: Class Section, camera stream address etc.
```console
cd inference_engine_samples_build/intel64/Release/
```

- Important Flags to use while running the application

```console
--cs, --section (value:DEFAULT)
        specify the class section
--d_act, --device (value:CPU)
          Optional. Specify the target device for Person/Action Detection Retail (CPU, GPU).
--d_em, --device (value:CPU)
          Optional. Specify the target device for Emotions Retail (CPU, GPU).
--d_fd, --device (value:CPU)
          Optional. Specify the target device for Face Detection Retail (CPU, GPU).
--d_hp, --device (value:CPU)
          Optional. Specify the target device for Headpose Retail (CPU, GPU).
--d_lm, --device (value:CPU)
          Optional. Specify the target device for Landmarks Regression Retail (CPU, GPU).
--d_reid, --device (value:CPU)
          Optional. Specify the target device for Face Reidentification Retail (CPU, GPU).
--db_ip, --influxip (value:172.21.0.6)
        specify the Ip Address of the InfluxDB container
-h, --help (value:true)
        Print help message.
-i, --input
        Path to input image or video file.
--no-show, --noshow (value:0)
        specify no-show = 1 if don't want to see the processed Video
```

- Running the Application.

```console

cd root/inference_engine_samples_build/intel64/Release/


./classroom_analytics -pdc=/data/Retail/action_detection/pedestrian/rmnet_ssd/0165/dldt/person-detection-action-recognition-0005.xml -c=/data/Transportation/object_detection/face/pruned_mobilenet_reduced_ssd_shared_weights/dldt/face-detection-adas-0001.xml -lrc=/data/Retail/object_attributes/landmarks_regression/0009/dldt/landmarks-regression-retail-0009.xml -pc=/data/Transportation/object_attributes/headpose/vanilla_cnn/dldt/head-pose-estimation-adas-0001.xml -sc=/data/Retail/object_attributes/emotions_recognition/0003/dldt/emotions-recognition-retail-0003.xml -frc=/data/Retail/object_reidentification/face/mobilenet_based/dldt/face-reidentification-retail-0095.xml -fgp=/opt/intel/openvino/inference_engine/samples/classroom_analytics/faces_gallery.json -i=<Ip Of the Ip Camera/Location of video File (/data/sample.mp4)/ cam> --influxip=<Ip of InfluxDB Container/172.21.0.6> --nosh
```
>The default static ip of the InfluxDB container is set to 172.21.0.6

![Running the RI](images/docker-exec.gif)
*Fig:6: Running the classroom analytics Reference Implementation*

>If there is an error in viewing the GUI, run **xhost +SI:localuser:root** before logging into the container.

### Configure Grafana* for Visualizations

 * Open a browser on the host computer, go to http://localhost:3000. (You will get change password prompt while logging into grafana. Skip it.)

* Login with the default credentials as user : **admin** and password : **admin**.

* Click on the Configuration icon present on the left panel.

* Click on + Add data source button and provide the inputs below.

* Add data source from the InfluxDB container:

  * Type: InfluxDB
  * Name: InfluxDB
  * URL: http://(influxDBContainerIpaddress/172.21.0.6):8086
  * Database: Analytics
  * Click on “Save and Test”

>Default IP of influx Container : 172.21.0.6

![Adding InfluxDB datasource](/images/Add-grafana-datasource.gif)
*Fig:7: Adding InfluxDB Datastore in Grafana*

* Import the classroom-analytics.json from the Grafana import dashboards.

![A sample dashboard](/images/import-dashboards.png)
*Fig:8: Adding a sample dashboard in Grafana*

* Monitor the live metrics from the pre-configured Grafana dashboard.

![Live charts](/images/grafana-live-charts.gif)
*Fig:9: Live data charts from the ClassRoom*

>In case you see no data in the  Charts, hover over the widget name (Happiness Index etc) and go to edit option in every widget and change the classroom name as per your classroom.
---

## Additional Steps

#### Adding more classrooms

With our current approach, we can add more classrooms and gather their live data as well for visualization and analytics of the defined metrics.

![Advanced Architecture](/images/advanced_architecture.png)
*Fig:10:Multiple Classes means Multiple containers*

To scale the application for multiple classrooms we can create multiple containers using the following command.
```console
docker-compose up --scale classroom-analytics=<N>
```

![Scaling commands](/images/advanced-scaling.gif)

*Fig:11: Running Compose command for supporting multiple Classrooms*

This will create 2 container instances of the classroom-analytics image. We can log into each container and run the command mentioned in the above steps to get the data from the particular classroom.

To get the Container IDs of your running containers, use the following command

```console
docker ps -aqf "name=classroom-analytics"

92de4bf20987
58dba925f788
```

Login to the containers in different terminals using the following command:-

```console
docker exec -it <container ID> /bin/bash
```
Once we are at the container console, execute the command for running the application as mentioned above. The --i IP Camera stream and --cs (section) flag input needs to be changed as per the new classrooms being added.

For Example, let's say we have two classrooms namely 9B and 10B. We run the following commands after logging into the containers in two different terminals.

- Classroom 1 : Name = 9B

```console
cd root/inference_engine_samples_build/intel64/Release/

./classroom_analytics -pdc=/data/Retail/action_detection/pedestrian/rmnet_ssd/0165/dldt/person-detection-action-recognition-0005.xml -c=/data/Transportation/object_detection/face/pruned_mobilenet_reduced_ssd_shared_weights/dldt/face-detection-adas-0001.xml -lrc=/data/Retail/object_attributes/landmarks_regression/0009/dldt/landmarks-regression-retail-0009.xml -pc=/data/Transportation/object_attributes/headpose/vanilla_cnn/dldt/head-pose-estimation-adas-0001.xml -sc=/data/Retail/object_attributes/emotions_recognition/0003/dldt/emotions-recognition-retail-0003.xml -frc=/data/Retail/object_reidentification/face/mobilenet_based/dldt/face-reidentification-retail-0095.xml -fgp=/opt/intel/openvino/inference_engine/samples/classroom_analytics/faces_gallery.json -i=<Ip Of the Ip Camera/Location of video File (/data/sample.mp4)/ cam> --influxip=<Ip of InfluxDB Container/172.21.0.6> --noshow=1 --cs=9B
```

- Classroom 2 : Name = 10B

```console
cd root/inference_engine_samples_build/intel64/Release/

./classroom_analytics -pdc=/data/Retail/action_detection/pedestrian/rmnet_ssd/0165/dldt/person-detection-action-recognition-0005.xml -c=/data/Transportation/object_detection/face/pruned_mobilenet_reduced_ssd_shared_weights/dldt/face-detection-adas-0001.xml -lrc=/data/Retail/object_attributes/landmarks_regression/0009/dldt/landmarks-regression-retail-0009.xml -pc=/data/Transportation/object_attributes/headpose/vanilla_cnn/dldt/head-pose-estimation-adas-0001.xml -sc=/data/Retail/object_attributes/emotions_recognition/0003/dldt/emotions-recognition-retail-0003.xml -frc=/data/Retail/object_reidentification/face/mobilenet_based/dldt/face-reidentification-retail-0095.xml -fgp=/opt/intel/openvino/inference_engine/samples/classroom_analytics/faces_gallery.json -i=<Ip Of the Ip Camera/Location of video File ( /data/sample.mp4)/ cam> --influxip=<Ip of InfluxDB Container/172.21.0.6> --noshow=1 --cs=10B
```

In Grafana, import the same **Classroom Analytics.json** file and change the classroom name as per the one given in the application flags by editing individual panels in the Dashboard. To edit the panels click on the panel name and select Edit from the drop-down menu. Save the changes when you are done.

![Running the RI](images/multipledashboard.gif)
*Fig:12: Adding more classes in Grafana*

#### Testing with USB Camera

To test with a USB Camera attach the Camera to the USB port of the host machine. Add the following lines in the classroom-analytics service section in the docker-compose.yml.

```console
devices:
  - /dev/video0:/dev/video0
```
While running the application, use _-i=cam_ as the option to use the video from the USB camera.  

>With the above USB camera changes, when running Docker compose for Multiple classrooms the camera will be accessible only in one container, where _-i=cam_  command is run first.


#### Launch Script

If you are using the Reference implementation for the first time, run the launch script provided in the folder after completing the prerequisites. The script takes care of building, installing, creating and running the containers for a single classroom. There are a few flags that can be set while running the script.

```console
./launch.sh -h

script usage: $(launch.sh) [-r Classroom Name] [-c Ip Camera Link] [-i InfluxDB Ip]
```
Example:

```console
./launch.sh -r 9B -i 172.21.0.6
```

To see the Classroom Analytics charts, follow the steps mentioned [here](#configure-grafana-for-visualizations)

#### Running on the integrated GPU (optional)

By default, the application runs on the CPU. User can specify which models to run on GPU by using the flags specified.

---

##### NOTES

>With Additional Classroom containers being run simultaneously the overall inference rate will drop.

>Attendance is calculated in the half-time of the class ie: if the class starts from 10 am in timetable.txt. Attendance for that subject will be calculated at 10:30 am.

>The Overall Accuracy of the inference depends on the individual model resolution and accuracy of the model. To learn more, visit  [Overview of OpenVINO™ Toolkit Pre-Trained Models](https://docs.openvinotoolkit.org/latest/_docs_Pre_Trained_Models.html).
