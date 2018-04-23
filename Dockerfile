FROM nvidia/cuda:9.1-cudnn7-runtime
MAINTAINER Lucas Servén <lserven@gmail.com>
COPY vendor/darknet/cfg /opt/darkweb/vendor/darknet/cfg
COPY vendor/darknet/data /opt/darkweb/vendor/darknet/data
COPY vendor/darknet/yolov3.weights /opt/darkweb/vendor/darknet/yolov3.weights
COPY vendor/darknet/yolov2-tiny.weights /opt/darkweb/vendor/darknet/yolov2-tiny.weights
COPY vendor/darknet/yolo9000.weights /opt/darkweb/vendor/darknet/yolo9000.weights
COPY darkweb /opt/darkweb/darkweb
WORKDIR /opt/darkweb
ENTRYPOINT ["/opt/darkweb/darkweb"]
