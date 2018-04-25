# Darkweb
An API for Darknet image detection neural networks like YOLO.

[![Build Status](https://travis-ci.org/squat/darkweb.svg?branch=master)](https://travis-ci.org/squat/darkweb)

## Running
The easiest way to use Darkweb is to run the pre-built container:

```sh
docker run --rm --name darkweb -p 8080:8080 --device=/dev/nvidiactl --device=/dev/nvidia-uvm --device=/dev/nvidia0 --volume=/opt/nvidia:/usr/local/nvidia:ro squat/darkweb
```

You can then make requests against the container service, e.g.:

```sh
curl 127.0.0.1:8080/api/yolo -F '=@./vendor/darknet/data/dog.jpg'
```

To deploy Darkweb to a Kubernetes cluster, you must have nodes with GPUs and with [device plugins installed](https://kubernetes.io/docs/concepts/cluster-administration/device-plugins/). Once ready, create the example deployment:

```sh
kubectl apply -f kubernetes/deployment.yaml
```

## Building
If you prefer to build Darkweb yourself, first download the sources:

```sh
git clone https://github.com/squat/darkweb
cd darkweb
```

By default, Darkweb is built inside of a NVIDIA Docker container to ensure that the necessary CUDA libraries are present and to guarantee consistent builds.

```sh
make
```

*Note*: if you have all the necessary CUDA libraries installed locally and wish to run the builds directly on your machine, set `CONTAINERIZE=0`:

```sh
make CONTAINERIZE=0
```

If you want to build Darkweb without GPU support, set `GPU=0`:

```sh
make GPU=0
```

Finally, you will need to download the model weights to be able to use the networks:

```sh
make weights
```

## Usage
### API
By default, the Darkweb API server runs on port *8080*, though this can be configured with the `-p` flag.

```sh
darkweb -p=1337
```

#### POST `/api/yolo`
Run the YOLO object detector on an uploaded image. This endpoint expects an image in a multipart upload. Example:

```sh
curl 127.0.0.1:8080/api/yolo -F '=@./vendor/darknet/data/dog.jpg'
# {
#   "size": 163759,
#   "x": 768,
#   "y": 576,
#   "c": 3,
#   "time": 10.835417,
#   "detections": [
#     {
#       "label": "dog",
#       "p": 0.990042,
#       "x": 0.288539,
#       "y": 0.660512,
#       "w": 0.243191,
#       "h": 0.542467
#     },
#     {
#       "label": "truck",
#       "p": 0.92372,
#       "x": 0.756574,
#       "y": 0.222694,
#       "w": 0.280832,
#       "h": 0.147699
#     },
#     {
#       "label": "car",
#       "p": 0.259004,
#       "x": 0.774838,
#       "y": 0.224054,
#       "w": 0.246296,
#       "h": 0.142042
#     },
#     {
#       "label": "bicycle",
#       "p": 0.994177,
#       "x": 0.473209,
#       "y": 0.483861,
#       "w": 0.516853,
#       "h": 0.576053
#     }
#   ]
# }
```

#### POST `/api/yolo9000`
Run the YOLO9000 object detector on an uploaded image. This endpoint expects an image in a multipart upload. Example:

```sh
curl 127.0.0.1:8080/api/yolo9000 -F '=@./vendor/darknet/data/dog.jpg'
# {
#   "size": 163759,
#   "x": 768,
#   "y": 576,
#   "c": 3,
#   "time": 7.798958,
#   "detections": [
#     {
#       "label": "Shetland sheepdog",
#       "p": 0.563054,
#       "x": 0.305005,
#       "y": 0.647629,
#       "w": 0.23873,
#       "h": 0.528642
#     },
#     {
#       "label": "tom",
#       "p": 0.402929,
#       "x": 0.278132,
#       "y": 0.657537,
#       "w": 0.296026,
#       "h": 0.600402
#     },
#     {
#       "label": "tortoiseshell",
#       "p": 0.475844,
#       "x": 0.287831,
#       "y": 0.654079,
#       "w": 0.257309,
#       "h": 0.552583
#     },
#     {
#       "label": "push-bike",
#       "p": 0.257703,
#       "x": 0.514435,
#       "y": 0.503511,
#       "w": 0.421874,
#       "h": 0.384144
#     },
#     {
#       "label": "bicycle-built-for-two",
#       "p": 0.570866,
#       "x": 0.507706,
#       "y": 0.495363,
#       "w": 0.455473,
#       "h": 0.511342
#     },
#     {
#       "label": "limousine",
#       "p": 0.701897,
#       "x": 0.731044,
#       "y": 0.210221,
#       "w": 0.318366,
#       "h": 0.172348
#     }
#   ]
# }
```
 
#### POST `/api/tiny`
Run the tiny YOLO object detector on an uploaded image. This endpoint expects an image in a multipart upload. Example:

```sh
curl 127.0.0.1:8080/api/tiny -F '=@./vendor/darknet/data/dog.jpg'
# {
#   "size": 163759,
#   "x": 768,
#   "y": 576,
#   "c": 3,
#   "time": 1.051016,
#   "detections": [
#     {
#       "label": "dog",
#       "p": 0.819004,
#       "x": 0.284219,
#       "y": 0.689174,
#       "w": 0.28744,
#       "h": 0.516138
#     },
#     {
#       "label": "car",
#       "p": 0.738448,
#       "x": 0.74617,
#       "y": 0.221734,
#       "w": 0.267723,
#       "h": 0.161173
#     },
#     {
#       "label": "car",
#       "p": 0.380406,
#       "x": 0.113937,
#       "y": 0.176513,
#       "w": 0.04088,
#       "h": 0.049404
#     },
#     {
#       "label": "car",
#       "p": 0.304245,
#       "x": 0.67088,
#       "y": 0.208459,
#       "w": 0.152685,
#       "h": 0.132376
#     },
#     {
#       "label": "bicycle",
#       "p": 0.589162,
#       "x": 0.423873,
#       "y": 0.506519,
#       "w": 0.685571,
#       "h": 0.512563
#     },
#     {
#       "label": "person",
#       "p": 0.267455,
#       "x": 0.96026,
#       "y": 0.403358,
#       "w": 0.07195,
#       "h": 0.910909
#     },
#     {
#       "label": "person",
#       "p": 0.261761,
#       "x": 0.57678,
#       "y": 0.176673,
#       "w": 0.03534,
#       "h": 0.058247
#     }
#   ]
# }
```

#### GET `/healthz`
The `/healthz` endpoint returns a 200 if the API is running.
