// The MIT License (MIT)
//
// Copyright (c) 2017 THINK BIG LABS SL
// Author: alvarolb@gmail.com (Alvaro Luis Bustamante)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef THINGER_CLIENT_H
#define THINGER_CLIENT_H

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>

#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include "core/thinger.h"

using namespace protoson;

dynamic_memory_allocator alloc;
memory_allocator& protoson::pool = alloc;

#ifndef THINGER_SERVER
    #define THINGER_SERVER "iot.thinger.io"
#endif

#ifndef THINGER_PORT
    #define THINGER_PORT 25200
#endif

#ifndef RECONNECTION_TIMEOUT_SECONDS
    #define RECONNECTION_TIMEOUT_SECONDS 15
#endif


class thinger_client : public thinger::thinger {

public:

    enum THINGER_STATE{
        NETWORK_CONNECTING,
        NETWORK_CONNECTED,
        NETWORK_CONNECT_ERROR,
        SOCKET_CONNECTING,
        SOCKET_CONNECTED,
        SOCKET_CONNECTION_ERROR,
        SOCKET_DISCONNECTED,
        SOCKET_TIMEOUT,
        SOCKET_ERROR,
        THINGER_AUTHENTICATING,
        THINGER_AUTHENTICATED,
        THINGER_AUTH_FAILED,
        THINGER_STOP_REQUEST
    };

    thinger_client(const char* user, const char* device, const char* device_credential, const char* thinger_server = THINGER_SERVER) :
      sockfd(-1), username_(user), device_id_(device), device_password_(device_credential), thinger_server_(thinger_server),
      out_buffer_(NULL), out_size_(0), buffer_size_(0)
    {
        #if DAEMON
          daemonize();
        #endif
    }

    virtual ~thinger_client()
    {}

protected:
    virtual const char* get_server(){
      return thinger_server_;
    }

    virtual unsigned short get_server_port(){
      return THINGER_PORT;
    }

    // TODO change this to a monotonic clock implementation. Using c++11?
    unsigned long millis() {
        struct timeval te;
        gettimeofday(&te, NULL);
        unsigned long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000;
        return milliseconds;
    }

    virtual bool connected(){
        return true;
    }

    virtual void disconnected(){
        thinger_state_listener(SOCKET_TIMEOUT);
        thinger::disconnected();
        if(sockfd>=0){
            close(sockfd);
            thinger_state_listener(SOCKET_DISCONNECTED);
        }
        sockfd = -1;
    }

    virtual bool read(char* buffer, size_t size){
        if(sockfd==-1) return false;
        ssize_t read_size = ::read(sockfd, buffer, size);
        if(read_size!=size){
            disconnected();
        }
        return read_size == size;
    }

    virtual bool write(const char* buffer, size_t size, bool flush=false){
        if(size>0){
            if(size+out_size_>buffer_size_){
                buffer_size_ = out_size_ + size;
                out_buffer_ = (uint8_t*) realloc(out_buffer_, buffer_size_);
            }
            memcpy(&out_buffer_[out_size_], buffer, size);
            out_size_ += size;
        }
        if(flush && out_size_>0){
            bool success = to_socket(out_buffer_, out_size_);
            out_size_ = 0;
            if(!success){
                disconnected();
            }
            return success;
        }
        return true;
    }

    virtual void thinger_state_listener(THINGER_STATE state){
        #ifdef DEBUG
        switch(state){
            case NETWORK_CONNECTING:
                std::cout << std::fixed << millis()/1000.0 << " ";
                std::cout << "[NETWORK] Starting connection..." << std::endl;
                break;
            case NETWORK_CONNECTED:
                std::cout << std::fixed << millis()/1000.0 << " ";
                std::cout << "[NETWORK] Connected!" << std::endl;
                break;
            case NETWORK_CONNECT_ERROR:
                std::cout << std::fixed << millis()/1000.0 << " ";
                std::cout << "[NETWORK] Cannot connect!" << std::endl;
                break;
            case SOCKET_CONNECTING:
                std::cout << std::fixed << millis()/1000.0 << " ";
                std::cout << "[_SOCKET] Connecting to " << thinger_server_ << ":" << get_server_port() << "..." << std::endl;
                std::cout << std::fixed << millis()/1000.0 << " ";
                std::cout << "[_SOCKET] Using secure TLS/SSL connection: ";
                std::cout << (get_server_port() == 25200 ? "no" : "yes") << std::endl;
                break;
            case SOCKET_CONNECTED:
                std::cout << std::fixed << millis()/1000.0 << " ";
                std::cout << "[_SOCKET] Connected!" << std::endl;
                break;
            case SOCKET_CONNECTION_ERROR:
                std::cout << std::fixed << millis()/1000.0 << " ";
                std::cout << "[_SOCKET] Error while connecting!" << std::endl;
                break;
            case SOCKET_DISCONNECTED:
                std::cout << std::fixed << millis()/1000.0 << " ";
                std::cout << "[_SOCKET] Is now closed!" << std::endl;
                break;
            case SOCKET_ERROR:
                std::cout << std::fixed << millis()/1000.0 << " ";
                std::cout << "[_SOCKET] Socket Error!" << std::endl;
                break;
            case SOCKET_TIMEOUT:
                std::cout << std::fixed << millis()/1000.0 << " ";
                std::cout << "[_SOCKET] Timeout!" << std::endl;
                break;
            case THINGER_AUTHENTICATING:
                std::cout << std::fixed << millis()/1000.0 << " ";
                std::cout << "[THINGER] Authenticating. User: " << username_ << " Device: " << device_id_ << std::endl;
                break;
            case THINGER_AUTHENTICATED:
                std::cout << std::fixed << millis()/1000.0 << " ";
                std::cout << "[THINGER] Authenticated" << std::endl;
                break;
            case THINGER_AUTH_FAILED:
                std::cout << std::fixed << millis()/1000.0 << " ";
                std::cout << "[THINGER] Auth Failed! Check username, device id, or device credentials." << std::endl;
                break;
            case THINGER_STOP_REQUEST:
                std::cout << std::fixed << millis()/1000.0 << " ";
                std::cout << "[THINGER] Client was requested to stop." << std::endl;
                break;
        }
        #endif
        if(state_listener_) state_listener_(state);
    }

    bool handle_connection() {
        while(sockfd<0){
            thinger_state_listener(NETWORK_CONNECTING);
            if(!connect_client()){
                thinger_state_listener(NETWORK_CONNECT_ERROR);
                sleep(RECONNECTION_TIMEOUT_SECONDS);
            } else {
                thinger_state_listener(NETWORK_CONNECTED);
            }
        }
        return true;
    }

    bool connect_client(){
        // resolve server name
        struct hostent* server = gethostbyname(get_server());
        if (server == NULL) return false;

        // configure IP address and server port
        struct sockaddr_in serv_addr;
        bzero((char *) &serv_addr, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
        serv_addr.sin_port = htons(get_server_port());

        // create a new socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) return false;

        thinger_state_listener(SOCKET_CONNECTING);

        // try connecting the socket to the server address
        if (::connect(sockfd,(struct sockaddr *) &serv_addr, sizeof(serv_addr)) == 0 && connected()){

            // set tcp no delay
            int flag = 1;
            int result = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
            if (result < 0){
                thinger_state_listener(SOCKET_CONNECTION_ERROR);
            }

            thinger_state_listener(SOCKET_CONNECTED);
            thinger_state_listener(THINGER_AUTHENTICATING);
            bool auth = thinger::thinger::connect(username_, device_id_, device_password_);
            if(!auth){
                thinger_state_listener(THINGER_AUTH_FAILED);
                disconnected();
            } else {
                thinger_state_listener(THINGER_AUTHENTICATED);
            }
            return auth;
        }

        // call disconnected to cleanup socket resources
        disconnected();
        return false;
    }

public:

    /**
     * Static method for allowing run the program as a daemon. Must be called in the main.
     */
    static void daemonize(){
        /* Our process ID and Session ID */
        pid_t pid, sid;

        /* Fork off the parent process */
        pid = fork();
        if (pid < 0) {
            exit(EXIT_FAILURE);
        }
        /* If we got a good PID, then
           we can exit the parent process. */
        if (pid > 0) {
           exit(EXIT_SUCCESS);
        }

        /* Change the file mode mask */
        umask(0);

        /* Open any logs here */

        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }

        /* Change the current working directory */
        if ((chdir("/")) < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
        }

        /* Close out the standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    void start(){
        while(true){
            handle();
        }
    }

    void handle(){
        if(handle_connection()){
            fd_set rfds;
            struct timeval tv;

            FD_ZERO(&rfds);
            FD_SET(sockfd, &rfds);

            tv.tv_sec = 1;
            tv.tv_usec = 0;

            int retval = select(sockfd+1, &rfds, NULL, NULL, &tv);
            if (retval == -1){
                disconnected();
            } else {
                bool data_available = retval>0 && FD_ISSET(sockfd, &rfds);
                thinger::thinger::handle(millis(), data_available);
            }
        }
    }

    void set_state_listener(std::function<void(THINGER_STATE)> state_listener){
        state_listener_ = state_listener;
    }

protected:

    virtual bool to_socket(const uint8_t* buffer, size_t size){
        if(sockfd==-1) return false;
        ssize_t written = ::write(sockfd, buffer, size);
        return size == written;
    }

    int sockfd;
    const char* thinger_server_;
    const char* username_;
    const char* device_id_;
    const char* device_password_;
    std::function<void(THINGER_STATE)> state_listener_;
    uint8_t* out_buffer_;
    size_t out_size_;
    size_t buffer_size_;

};

#endif
