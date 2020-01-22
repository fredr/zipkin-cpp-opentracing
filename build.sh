#!/bin/bash

docker build -f Dockerfile . -t newzipkin
id=$(docker create newzipkin)
docker cp $id:/usr/local/lib/libzipkin_opentracing.so.0.5.2 - > libzipkin_opentracing.so.0.5.2.tar
docker rm -v $id
tar -xvf libzipkin_opentracing.so.0.5.2.tar
