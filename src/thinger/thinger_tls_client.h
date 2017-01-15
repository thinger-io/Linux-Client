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

#ifndef THINGER_TLS_CLIENT_H
#define THINGER_TLS_CLIENT_H

#include "thinger_client.h"
#include <openssl/ssl.h>

#define THINGER_SSL_PORT 25202

class thinger_tls_client : public thinger_client {

public:
	thinger_tls_client(const char* user, const char* device, const char* device_credential) :
    	thinger_client(user, device, device_credential), sslCtx(NULL), ssl(NULL)
    {
		SSL_library_init();
    }

	virtual ~thinger_tls_client()
    {
		 EVP_cleanup();
    }

protected:
    virtual unsigned short get_server_port(){
    	return THINGER_SSL_PORT;
    }

	virtual bool connected(){
		// create SSL context
		sslCtx = SSL_CTX_new(SSLv23_method());
		if(sslCtx==NULL) return false;

		//SSL_CTX_set_verify(sslCtx, SSL_VERIFY_PEER, NULL);
		//SSL_CTX_set_verify_depth(sslCtx, 4);

		// force TLS 1.1 or TLS 1.2
		const long flags = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
		SSL_CTX_set_options(sslCtx, flags);

		// create SSL instance
		ssl = SSL_new (sslCtx);
		if(ssl==NULL) return false;

		// set preferred ciphers
		const char* const PREFERRED_CIPHERS = "HIGH:!aNULL:!kRSA:!PSK:!SRP!MD5:!RC4";
		SSL_set_cipher_list(ssl, PREFERRED_CIPHERS);

		// set socket file descriptor
	    if (!SSL_set_fd(ssl, sockfd)) return false;

	    // initiate SSL handshake
		if (!SSL_connect (ssl)) return false;

		return true;
	}

	virtual void disconnected(){
		// release SSL
		if(ssl!=NULL){
			SSL_free(ssl);
			ssl = NULL;
		}

		// release SSL context
		if(sslCtx!=NULL){
			SSL_CTX_free(sslCtx);
			sslCtx = NULL;
		}

		// release socket
		thinger_client::disconnected();
	}

	virtual bool read(char* buffer, size_t size){
		if(ssl==NULL) return false;
		int read_size = SSL_read(ssl, buffer, size);
		if(read_size!=size){
			disconnected();
		}
		return read_size == size;
	}

protected:

	virtual bool to_socket(const uint8_t* buffer, size_t size){
		if(ssl==NULL) return false;
		int write_size = SSL_write(ssl, buffer, size);
		return write_size == size;
	}

private:
	SSL_CTX *sslCtx;
	SSL *ssl;
};

#endif
