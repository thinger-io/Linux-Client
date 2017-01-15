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

#define THINGER_SERVER "iot.thinger.io"
#define THINGER_PORT 25200
#define RECONNECTION_TIMEOUT_SECONDS 15

class thinger_client : public thinger::thinger {

public:
	thinger_client(const char* user, const char* device, const char* device_credential) :
    	sockfd(-1), username_(user), device_id_(device), device_password_(device_credential),
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
    	return THINGER_SERVER;
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
		thinger::disconnected();
		if(sockfd>=0){
			#ifdef DEBUG
			std::cout << "[" <<  std::fixed << millis()/1000.0 << "]: " << "Closing socket!" << std::endl;
			#endif
			close(sockfd);
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

    bool handle_connection()
    {
    	while(sockfd<0){
			#ifdef DEBUG
			std::cout << "[" <<  std::fixed << millis()/1000.0 << "]: " << "Not connected!" << std::endl;
			#endif
    		if(!connect_client()){
				#ifdef DEBUG
				std::cout << "[" <<  std::fixed << millis()/1000.0 << "]: " << "Cannot Connect! Trying again in a few seconds..." << std::endl;
        		#endif
				sleep(RECONNECTION_TIMEOUT_SECONDS);
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

		#ifdef DEBUG
		std::cout << "[" <<  std::fixed << millis()/1000.0 << "]: " << "Connecting to " << get_server() << ":" << get_server_port() << " ..." << std::endl;
		#endif

		// try connecting the socket to the server address
        if (::connect(sockfd,(struct sockaddr *) &serv_addr, sizeof(serv_addr)) == 0 && connected()){

			// set tcp no delay
			int flag = 1;
			int result = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
			if (result < 0){
				#ifdef DEBUG
				std::cerr << "Cannot set TCP_NODELAY!" << std::endl;
                #endif
			}

			#ifdef DEBUG
			std::cout << "[" <<  std::fixed << millis()/1000.0 << "]: " << "Connected!" << std::endl;
			std::cout << "[" <<  std::fixed << millis()/1000.0 << "]: " << "Authenticating..." << std::endl;
			#endif
            bool auth = thinger::thinger::connect(username_, device_id_, device_password_);
            if(!auth){
				#ifdef DEBUG
				std::cout << "[" <<  std::fixed << millis()/1000.0 << "]: " << "Cannot authenticate!" << std::endl;
				#endif
        		disconnected();
            }
			#ifdef DEBUG
			else{
        		std::cout << "[" <<  std::fixed << millis()/1000.0 << "]: " << "Authenticated!" << std::endl;
            }
			#endif
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
    		}else{
				bool data_available = retval>0 && FD_ISSET(sockfd, &rfds);
				thinger::thinger::handle(millis(), data_available);
			}
    	}
    }

protected:

	virtual bool to_socket(const uint8_t* buffer, size_t size){
		if(sockfd==-1) return false;
		ssize_t written = ::write(sockfd, buffer, size);
		return size == written;
	}

    int sockfd;
    const char* username_;
    const char* device_id_;
    const char* device_password_;
	uint8_t* out_buffer_;
	size_t out_size_;
	size_t buffer_size_;

};

#endif
