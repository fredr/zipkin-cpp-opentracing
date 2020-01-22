ARG NGINX_LABEL=latest

FROM nginx:${NGINX_LABEL}

ARG OPENTRACING_CPP_VERSION=v1.5.1
ARG ZIPKIN_CPP_VERSION=v0.5.2
ARG LIGHTSTEP_VERSION=v0.8.1
ARG JAEGER_CPP_VERSION=v0.4.2
ARG GRPC_VERSION=v1.22.x
ARG DATADOG_VERSION=v1.1.2

COPY . /src

RUN set -x \
# install nginx-opentracing package dependencies
  && apt-get update \
  && apt-get install --no-install-recommends --no-install-suggests -y \
              libcurl4-openssl-dev \
              libprotobuf-dev \
              protobuf-compiler \
# save list of currently-installed packages so build dependencies can be cleanly removed later
	&& savedAptMark="$(apt-mark showmanual)" \
# new directory for storing sources and .deb files
	&& tempDir="$(mktemp -d)" \
	&& chmod 777 "$tempDir" \
			\
# (777 to ensure APT's "_apt" user can access it too)
## Build OpenTracing package and tracers
  && apt-get install --no-install-recommends --no-install-suggests -y \
              build-essential \
              cmake \
              git \
              ca-certificates \
              pkg-config \
              wget \
              golang \
              libz-dev \
              automake \
              autogen \
              autoconf \
              libtool \
              g++-7 \
 && true


RUN true \
# reset apt-mark's "manual" list so that "purge --auto-remove" will remove all build dependencies
# (which is done after we install the built packages so we don't have to redownload any overlapping dependencies)
	&& apt-mark showmanual | xargs apt-mark auto > /dev/null \
	&& { [ -z "$savedAptMark" ] || apt-mark manual $savedAptMark; } \
	\
  && cd "$tempDir" \
### Use g++ 7
  # && update-alternatives --remove-all cc \
  # && update-alternatives --remove-all g++ \
  # && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.3 10
  # && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-4.3 10
  # && update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 30 \
  # && update-alternatives --set cc /usr/bin/gcc
  # && update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++ 30 \
  # && 
  && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 5 \
  && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-7 5 \
### Build opentracing-cpp
  && git clone -b $OPENTRACING_CPP_VERSION https://github.com/opentracing/opentracing-cpp.git \
  && cd opentracing-cpp \
  && mkdir .build && cd .build \
  && cmake -DCMAKE_BUILD_TYPE=Release \
           -DBUILD_TESTING=OFF .. \
  && make && make install \
  && cd "$tempDir" \
### Build zipkin-cpp-opentracing
  && cp -r /src zipkin-cpp-opentracing \
  && cd zipkin-cpp-opentracing \
  && mkdir .build && cd .build \
  && cmake -DBUILD_SHARED_LIBS=1 -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF .. \
  && make && make install \
  && cd "$tempDir" \
  && ln -s /usr/local/lib/libzipkin_opentracing.so /usr/local/lib/libzipkin_opentracing_plugin.so

RUN ls -al /usr/local/lib/libzipkin_opentracing.so.0.5.2
