FROM debian:bookworm
RUN     apt update \
    &&  apt install -y \
                    build-essential \
                    git \
                    cmake \
                    libcurl4-openssl-dev \
                    libssl-dev \
                    uuid-dev \
                    default-libmysqlclient-dev \
                    libopencv-dev \
                    redis-server \
                    libtool \
                    python3
WORKDIR /build_temp
COPY . /build_temp
RUN     git clone https://github.com/google/gumbo-parser.git \
    &&  cd gumbo-parser \
    &&  ./autogen.sh \
    &&  ./configure \
    &&  make && make install \
    &&  cd /build_temp \
    &&  mkdir build && cd build \
    &&  cmake -DCMAKE_BUILD_TYPE=Release .. \
    &&  make \
    &&  mkdir /MigangBot \
    &&  cp /build_temp/build/bin/MigangBot /MigangBot \
    &&  cp /build_temp/resources/* -r /MigangBot \
    &&  rm -rf /build_temp/
WORKDIR /MigangBot
ENTRYPOINT ./MigangBot
EXPOSE 8080
