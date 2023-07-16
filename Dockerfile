FROM ubuntu:22.04

# Install necessary dependencies
RUN apt-get update && \
    apt-get install -y \
    build-essential \
    cmake \
    libcpprest-dev \
    libmongoc-1.0-0 \
    libssl-dev \
    pkg-config \
    git \
    wget \
    curl

# Get Libmongoc 1.24.2
RUN wget https://github.com/mongodb/mongo-c-driver/releases/download/1.24.2/mongo-c-driver-1.24.2.tar.gz && \
    tar -zxf mongo-c-driver-1.24.2.tar.gz && \
    cd mongo-c-driver-1.24.2 && \
    mkdir cmake-build && \
    cd cmake-build && \
    cmake -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF .. && \
    cmake --build . && \
    cmake --build . --target install

# Get mongoCxx
RUN curl -OL https://github.com/mongodb/mongo-cxx-driver/releases/download/r3.8.0/mongo-cxx-driver-r3.8.0.tar.gz && \
    tar -xzf mongo-cxx-driver-r3.8.0.tar.gz && \
    cd mongo-cxx-driver-r3.8.0/build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local && \
    cmake --build . && \
    cmake --build . --target install

# Set the working directory
WORKDIR /app

# Copy the source code into the container
COPY . /app

# Build the application
RUN g++ -I/usr/local/include/mongocxx/v_noabi -I/usr/local/include/bsoncxx/v_noabi -I/usr/include/cpprest -o API_Challenge main.cpp -lmongocxx -lbsoncxx -lcpprest -lssl -lcrypto -Wl,-rpath,/usr/local/lib

EXPOSE 8080

# Set the entry point for the container
ENTRYPOINT ["./API_Challenge"]
