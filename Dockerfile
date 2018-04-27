FROM nvidia/cuda:9.1-cudnn7-runtime
MAINTAINER Lucas Servén <lserven@gmail.com>
COPY vendor/darknet/cfg /opt/darkapi/vendor/darknet/cfg
COPY vendor/darknet/data /opt/darkapi/vendor/darknet/data
COPY vendor/darknet/yolov3.weights /opt/darkapi/vendor/darknet/yolov3.weights
COPY vendor/darknet/yolov2-tiny.weights /opt/darkapi/vendor/darknet/yolov2-tiny.weights
COPY vendor/darknet/yolo9000.weights /opt/darkapi/vendor/darknet/yolo9000.weights
COPY darkapi /opt/darkapi/darkapi
WORKDIR /opt/darkapi
ENTRYPOINT ["/opt/darkapi/darkapi"]
