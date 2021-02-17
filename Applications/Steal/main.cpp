#include <iostream>
#include <vector>
#include <fstream>
#include <map>

#include <lemon/core/url.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <getopt.h>

int help = false;
int verbose = false;

char* outFile = nullptr;

enum HTTPStatusCode {
    Continue = 100,
    SwitchingProtocols = 101,
    OK = 200,
    Created = 201,
    Accepted = 202,
    NonAuthoritativeInformation = 203,
    NoContent = 204,
    ResetContent = 205,
    PartialContent = 206,
    MultipleChoices = 300,
    MovedPermanently = 301,
    Found = 302,
    SeeOther = 303,
    NotModified = 304,
    UseProxy = 305,
    SwitchProxy = 306,
    TemporaryRedirect = 307,
    PermanentRedirect = 308,
    BadRequest = 400,
    Unauthorized = 401,
    PaymentRequired = 402,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    NotAcceptable = 406,
    ProxyAuthenticationRequired = 407,
    RequestTimeout = 408,
    Conflict = 409,
    Gone = 410,
    LengthRequired = 411,
    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    ServiceUnavailable = 503,
    GatewayTimeout = 504,
    HTTPVersionNotSupported = 505,
    VariantAlsoNegotiates = 506,
};

class HTTPResponse {
public:
    std::string status;
    HTTPStatusCode statusCode;

    std::map<std::string, std::string> fields;
};

void UpdateProgress(size_t recievedData, size_t dataLeft){
    if(outFile){
        size_t total = recievedData + dataLeft;
        std::cout << "\033[1K " << recievedData / 1024 << " / " << total / 1024 << " KB [";

        if(total){
            size_t pBarCount = ((recievedData * 40) / total);
            size_t fillCount = 40 - pBarCount;

            for(unsigned i = 0; i < pBarCount; i++){
                std::cout << "#";
            }

            for(unsigned i = 0; i < fillCount; i++){
                std::cout << " ";
            }
        }

        std::cout << "]";

        std::cout.flush();
    }
}

int HTTPGet(int sock, const std::string& host, const std::string& resource, HTTPResponse& response, std::ostream* stream){
    char request[4096];
    int reqLength = snprintf(request, 4096, "GET /%s HTTP/1.1\r\n\
Host: %s\r\n\
User-Agent: steal/0.1\r\n\
Accept: */*\r\n\
\r\n", resource.c_str(), host.c_str());

    if(verbose){
        printf("\n%s---\n\n", request);
    }

    if(ssize_t len = send(sock, request, reqLength, 0); len != reqLength){
        if(reqLength < 0){
            perror("steal: Failed to send data");
        } else {
            std::cout << "steal: Failed to send all " << reqLength << " bytes (only sent " << len << ").\n";
        }

        return 11;
    }

    bool receivedHeader = false;
    char receiveBuffer[4096];

    std::string line;

    response.status.clear();
    response.fields.clear();

    ssize_t expectedData = 0;
    ssize_t receivedData = 0;
    ssize_t readSize = 4096;
    while(1){
        ssize_t len = recv(sock, receiveBuffer, readSize, 0);
        if(len < 0){
            std::cout << "steal: Error (" << errno << ") recieving data: " << strerror(errno) << "\n";
            return 12;
        }

        if(!receivedHeader){
            int i = 0;
            for(; i < len; i++){
                char c = receiveBuffer[i];

                if(c == '\n'){
                    if(!line.length()){
                        receivedHeader = true; // Empty line so end of header

                        if(verbose){
                            std::cout.write(receiveBuffer, i);
                        }

                        if(auto it = response.fields.find("Content-Length"); it != response.fields.end()){ // Check for Content-Length field
                            expectedData = std::stoull(it->second);
                        }

                        if(response.statusCode != HTTPStatusCode::OK){
                            return 0;
                        }
                        
                        i++;
                        break;
                    } else if(!response.status.length()){
                        response.status = line;

                        size_t pos = line.find(' ');
                        if(pos == std::string::npos){
                            return 20; // Bad response
                        }

                        response.statusCode = (HTTPStatusCode)std::stoi(line.substr(pos + 1, 3)); // Get response code
                    } else if(size_t pos = line.find(':'); pos != std::string::npos){
                        response.fields[line.substr(0, pos)] = line.substr(pos + 1); // Insert field into map
                    } // Invalid field

                    line.clear();
                } else if(c == '\r') {
                    // Ignore carriage return
                } else {
                    line += c;
                }
            }

            if(receivedHeader){
                receivedData = len - i;
                stream->write(receiveBuffer + i, len - i);
                expectedData -= (len - i);
            } else if(verbose){
                std::cout << receiveBuffer;
            }
        } else {
            if(len > expectedData){
                len = expectedData;
            }

            stream->write(receiveBuffer, len);
            receivedData += len;
            expectedData -= len;
        }

        UpdateProgress(receivedData, expectedData);

        if(receivedHeader && expectedData <= 0){
            stream->flush();
            break; // If expectedData is zero and we have received the header then assume that all the data has been received
        }
    }

    return 0;
}

int main(int argc, char** argv){
    
    option opts[] = {
        {"help", no_argument, &help, true},
        {"verbose", no_argument, nullptr, 'v'},
        {"output", required_argument, nullptr, 'o'},
        {nullptr, 0, nullptr, 0},
    };
    
    int option;
    while((option = getopt_long(argc, argv, "v", opts, nullptr)) >= 0){
        switch(option){
            case 'v':
                verbose = true;
                break;
            case 'o':
                outFile = strdup(optarg);
                break;
            case '?':
            default:
                std::cout << "Usage: " << argv[0] << "[options] <url>\nSee " << argv[0] << "--help\n";
                return 1;
        }
    }

    if(argc - optind < 1){
        std::cout << "Usage: " << argv[0] << " [options] <url>\nSee " << argv[0] << " --help\n";
        return 1;
    }

    if(help){
        std::cout << "steal [options] <url>\n"
            << "Steal data from <url> (Default HTTP)\n\n"
            << "--help              Show Help\n"
            << "-v, --verbose       Show extra information\n";

        return 0;
    }

    Lemon::URL url(argv[optind]);

    if(!url.IsValid() || !url.Host().length()){
        std::cout << "steal: Invalid/malformed URL";
        return 2;
    }

    if(url.Protocol().length() && url.Protocol().compare("http")){
        std::cout << "steal: Unsupported protocol: '" << url.Protocol() << "'\n";
        return 2;
    }

    std::ostream* out = nullptr;
    if(outFile){
        auto file = new std::ofstream(outFile, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);

        if(!file->is_open()){
            perror("steal: Error creating file for output");
            return 3;
        }

        file->seekp(0);

        out = reinterpret_cast<std::ostream*>(file);
    } else {
        out = &std::cout;
    }

redirect:
    uint32_t ip;
    if(inet_pton(AF_INET, url.Host().c_str(), &ip) > 0){

    } else {
        addrinfo hint;
        memset(&hint, 0, sizeof(addrinfo));
        hint = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };

        addrinfo* result;
        if(getaddrinfo(url.Host().c_str(), nullptr, &hint, &result)){
            std::cout << "steal: Failed to resolve host '" << url.Host() << "': " << strerror(errno) << "\n";
            return 4;
        }

        ip = reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr.s_addr;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in address;
    address.sin_addr.s_addr = ip;
    address.sin_family = AF_INET;
    address.sin_port = htons(80);

    if(int e = connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(sockaddr_in)); e){
        perror("steal: Failed to connect");
        return 10;
    }

    out->seekp(std::ios::beg);

    HTTPResponse response;
    if(int e = HTTPGet(sock, url.Host(), url.Resource(), response, out); e){
        return e;
    }

    if(response.statusCode == HTTPStatusCode::MovedPermanently){
        auto it = response.fields.find("Location");
        if(it == response.fields.end()){
            std::cout << "steal: Invalid redirect: No location field";
            return 13;
        }

        std::string& redirectURL = it->second;
        size_t pos = redirectURL.find_first_not_of(' ');
        url = Lemon::URL(redirectURL.substr(pos).c_str());

        if(!url.IsValid()){
            std::cout << "steal: Invalid redirect: Invalid URL '" << redirectURL.c_str() << "'\n";
            return 14;
        }

        if(url.Protocol().compare("http")){
            std::cout << "steal: Invalid redirect: Unknown URL protocol '" << url.Protocol().c_str() << "'\n";
            return 15;
        }

        goto redirect;
    } else if(response.statusCode != HTTPStatusCode::OK){
        std::cout << "steal: HTTP " << response.statusCode << "\n";
        return 20;
    }

    return 0;
}
