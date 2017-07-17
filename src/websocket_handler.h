/*
Copyright (c) 2017, rpi-webrtc-streamer Lyu,KeunChang

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <memory>
#include <string>
#include <map>
#include <set>

#ifndef WEBSOCKET_HANDLER_H_
#define WEBSOCKET_HANDLER_H_


enum WebSocketHandlerType {
    SINGLE_INSTANCE,        // allow only one handler runtime 
    MULTIPLE_INSTANCE,      // allow multiple handler runtime
};

enum HttpRequestType {
    HTTP_GET = 1,
    HTTP_POST,
    HTTP_DELETE,
    HTTP_PUT,
    HTTP_DONTCARE
};

enum FileMappingType {
    MAPPING_DEFAULT,
    MAPPING_FILE,
    MAPPING_DIRECTORY,
};

struct FileMapping {
    FileMapping(const std::string uri_prefix, 
            FileMappingType type, const std::string uri_resource_path)
        : uri_prefix_(uri_prefix), type_(type),
        uri_resource_path_(uri_resource_path) {}
    ~FileMapping() {}
    // allowed prefix for input URI
    const std::string uri_prefix_;           
    FileMappingType type_;
    // mapping local resource path for uri_prefix
    const std::string uri_resource_path_;    
};

struct HttpRequest {
    HttpRequest() : content_length_(-1) {};
    std::string url_;           // URL 
    int content_length_;
    std::string server_;          // Server name 
    HttpRequestType type_;      // request type, GET,POST
    std::string data_;          // request data only available on POST
    std::map<int,std::string> header_;
    std::map<std::string,std::string> args_;
    void AddHeader(int header_id, const std::string value );
    void AddArgs(const std::string args_param );
    void AddData(const std::string data );
    void Print();
};

// only text data response is allowed
struct HttpResponse {
    HttpResponse() : byte_sent_(0), header_sent_(false) {};
    int byte_sent_;
    int status_;            // return status value i.e 200, 404
    std::string mime_;      // mime type of response
    std::map<int,std::string> header_;
    std::string response_;  // response body, allow only text data 
    void AddHeader(int header_id, const std::string value );
    void Print();
    void SetHeaderSent();
    bool IsHeaderSent();
private:
    bool header_sent_;      // marked as true after sending header.
};

struct HttpHandler {
    virtual bool DoGet(HttpRequest* req, HttpResponse* res) = 0;
    virtual bool DoPost(HttpRequest* req, HttpResponse* res) = 0;
protected:
    virtual ~HttpHandler() {}
};

struct WebSocketHandler {
    virtual void OnConnect(int sockid) = 0;
    virtual bool OnMessage(int sockid, const std::string& message) = 0;
    virtual void OnDisconnect(int sockid) = 0;
    virtual void OnError(int sockid, const std::string& message) = 0;
protected:
    virtual ~WebSocketHandler() {}
};

struct WebSocketMessage {
    virtual void SendMessage(int sockid, const std::string& message) = 0;
    virtual void Close(int sockid, int reason_code, 
            const std::string& message) = 0;
protected:
    virtual ~WebSocketMessage() {}
};


#endif // WEBSOCKET_HANDLER_H_

