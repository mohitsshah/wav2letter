# Steps to build and run docker images

## Build images
```
$ docker build -t w2l/w2l-sever-base -f Dockerfile-Server-Base .
```
```
$ docker build -t w2l/w2l-server -f Dockerfile-Server
```
## Run gRPC server
```
$ docker run -p 50051:50051 -v <PATH_TO_MODEL_ON_HOST_MACHINE>:/root/model -it w2l/w2l-server
```

## Run gRPC client
```
$ cd client
$ python client1.py
```