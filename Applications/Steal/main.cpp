#include <vector>
#include <map>

#include <Lemon/Core/URL.h>

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>

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

struct IOObject {
    virtual ~IOObject() = default;

    virtual ssize_t Read(void* buffer, size_t len) = 0;
    virtual ssize_t Write(void* buffer, size_t len) = 0;
};

struct RawSocket final : IOObject {
    RawSocket(int fd)
        : sock(fd) {}

    ssize_t Read(void* buffer, size_t len){
        return recv(sock, buffer, len, 0);
    }

    ssize_t Write(void* buffer, size_t len){
        return send(sock, buffer, len, 0);
    }

    int sock;
};

struct TLSSocket : IOObject {
    BIO* tlsIO;

    TLSSocket(BIO* io)
        : tlsIO(io) {}

    ssize_t Read(void* buffer, size_t len){
        ssize_t rlen = BIO_read(tlsIO, buffer, len);
        while(rlen <= 0){
            if(!BIO_should_retry(tlsIO)){
                ERR_print_errors_fp(stderr);
                return -1;
            }

            rlen = BIO_read(tlsIO, buffer, len);
        }

        return rlen;
    }

    ssize_t Write(void* buffer, size_t len){
        ssize_t rlen = BIO_write(tlsIO, buffer, len);
        while(rlen <= 0){
            if(!BIO_should_retry(tlsIO)){
                ERR_print_errors_fp(stderr);
                return -1;
            }

            rlen = BIO_write(tlsIO, buffer, len);
        }

        return rlen;
    }
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
        printf("\033[1K %lu / %lu KB [", recievedData / 1024, total / 1024);

        if(total){
            size_t pBarCount = ((recievedData * 40) / total);
            size_t fillCount = 40 - pBarCount;

            for(unsigned i = 0; i < pBarCount; i++){
                putc('#', stdout);
            }

            for(unsigned i = 0; i < fillCount; i++){
                putc(' ', stdout);
            }
        }

        putc(']', stdout);
        fflush(stdout);
    }
}

int RecieveResponse(IOObject* sock, HTTPResponse& response){
    std::string line = "";

    response.fields.clear();
    response.status.clear();

    char c;
    ssize_t ret;
    while((ret = sock->Read(&c, sizeof(char))) == 1){
        if(verbose){
            fputc(c, stdout);
        }

        if(c == '\r'){
            continue; // We don't care about CR
        } else if(c == '\n'){
            if(!line.length()){
                break;
            }

            if(!response.status.length()){
                response.status = std::move(line); // We do not need the data in line anymore

                size_t pos = response.status.find(' ');
                if(pos == std::string::npos){
                    return 20; // Bad response
                }

                response.statusCode = (HTTPStatusCode)std::stoi(response.status.substr(pos + 1, 3)); // Get response code
            } else if(size_t pos = line.find(':'); pos != std::string::npos){
                std::transform(line.begin(), line.begin() + pos, line.begin(), tolower); // Convert field name to lowercase
                auto& value = response.fields[line.substr(0, pos)];
                value = line.substr(pos + 2); // Insert field into map

                if(value.front() == ' '){ // Remove any leading whitespace
                    value.erase(0, 1);
                }
            } else {
                return 20; // Invalid field
            }

            line.clear();
        } else {
            line += c;
        }
    }

    if(ret == 0){
        return 12; // Unexpected
    } else if(ret < 0) {
        printf("Browser: Error (%i) recieving data: %s\n", errno, strerror(errno));
        return 12;
    }

    return 0;
}

int HTTPGet(IOObject* sock, const std::string& host, const std::string& resource, HTTPResponse& response, FILE* stream){
    {
        char request[4096];
        int reqLength = snprintf(request, 4096, "GET /%s HTTP/1.1\r\n\
Host: %s\r\n\
User-Agent: curl/0.1\r\n\
Accept: */*\r\n\
Accept-Encoding: identity\r\n\
\r\n", resource.c_str(), host.c_str());

        if(verbose){
            printf("\n%s---\n\n", request);
        }

        if(ssize_t len = sock->Write(request, reqLength); len != reqLength){
            if(len < 0){
                perror("browser: Failed to send data");
            } else {
                printf("browser: Failed to send all %i bytes (only sent %li).\n", reqLength, len);
            }

            return 11;
        }
    }

    std::string line;

    response.status.clear();
    response.fields.clear();

    if(int e = RecieveResponse(sock, response); e){
        return e;
    }

    if(response.statusCode != HTTPStatusCode::OK){
        return 0;
    }

    ssize_t contentLength = -1;
    enum {
        TransferNormal,
        TransferChunked,
        TransferUnknown
    } transferEncoding = TransferNormal;

    auto fieldsEnd = response.fields.end();
    if(auto it = response.fields.find("transfer-encoding"); it != fieldsEnd){
        if(!it->second.compare("chunked")){
            transferEncoding = TransferChunked;
        } else if(!it->second.compare("identity")){
            transferEncoding = TransferNormal;
        } else {
            transferEncoding = TransferUnknown;
            printf("browser: Unknown transfer encoding '%s'.\n", it->second.c_str());
            return 21;
        }
    }

    if(auto it = response.fields.find("content-length"); it != fieldsEnd){
        contentLength = std::stol(it->second);
    }

    if(transferEncoding == TransferNormal){
        if(contentLength <= 0){
            printf("browser: Content length not specified and transfer encoding is not 'chunked'.\n");
            return 22;
        }

        ssize_t readSize = 4096;
        ssize_t expectedData = contentLength;
        char receiveBuffer[4096];
        while(1){
            if(readSize > expectedData){
                readSize = expectedData;
            }

            ssize_t len = sock->Read(receiveBuffer, readSize);
            if(len < 0){
                printf("browser: Error (%i) recieving data: %s\n", errno, strerror(errno));
                return 12;
            }

            if(len > expectedData){
                len = expectedData;
            }

            fwrite(receiveBuffer, 1, len, stream);
            expectedData -= len;

            UpdateProgress(expectedData, contentLength - expectedData);

            if(expectedData <= 0){
                break; // If expectedData is zero and we have received the header then assume that all the data has been received
            }
        }
    } else if(transferEncoding == TransferChunked) {
        auto getSocketChar = [sock]() -> int {
            char c;
            if(ssize_t ret = sock->Read(&c, 1); ret <= 0){
                return -1;
            }
            return c;
        };

        auto getChunkSize = [getSocketChar]() -> ssize_t {
            char buf[33];
            int bufIndex = 0;

            while(1){
                int c = getSocketChar();
                if(c <= 0){ // Read error
                    printf("browser: Error (%i) recieving data: %s\n", errno, strerror(errno));
                    return 12;
                } else if(c == '\r'){ // Ignore carriage return
                    continue;
                } else if(c == '\n'){
                    if(bufIndex == 0){
                        continue; // Leading CRLF?
                    }
                    break;
                } else if(c == ';'){ // Marks start of extra information
                    fprintf(stderr, "browser: Warning: Ignoring extra chunk information.\n"); // We don't care about the extra chunk information 
                    while((c = getSocketChar()) != '\n'){ // Discard everything until newline
                        if(c <= 0){
                            printf("browser: Error (%i) recieving data: %s\n", errno, strerror(errno));
                            return 12;
                        }
                    }
                    break;
                } else if(bufIndex >= 32){
                    printf("browser: Error: chunk size field >= 32 characters\n");
                    return 23;
                } else {
                    buf[bufIndex++] = c;
                }
            }
            buf[bufIndex] = 0;

            return std::stoll(buf, nullptr, 16);
        };

        while(1){
            ssize_t chunkSize = getChunkSize();
            if(chunkSize < 0){
                return chunkSize;
            } else if(chunkSize == 0){
                return 0;
            }

            char recieveBuffer[chunkSize];
            ssize_t received = 0;
            while(received < chunkSize){
                ssize_t ret = sock->Read(recieveBuffer + received, chunkSize - received);
                if(ret <= 0){
                    printf("browser: Error (%i) recieving data: %s\n", errno, strerror(errno));
                    return 12;
                }


                received += ret;
            }

            fwrite(recieveBuffer, 1, received, stream);
        }
        
    } else {
        assert(!"Invalid transfer encoding!");
    }

    fflush(stream);
    return 0;
}

int ResolveHostname(const std::string& host, uint32_t& ip){
    if(inet_pton(AF_INET, host.c_str(), &ip) > 0){ // Check if it is an IP address

    } else {
        addrinfo hint;
        memset(&hint, 0, sizeof(addrinfo));
        hint = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };

        addrinfo* result;
        if(getaddrinfo(host.c_str(), nullptr, &hint, &result)){
            printf("browser: Failed to resolve host '%s': %s.\n", host.c_str(), strerror(errno));
            return 4;
        }

        ip = reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr.s_addr;
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
                printf("Usage: %s [options] <url>\nSee --help\n", argv[0]);
                return 1;
        }
    }

    if(argc - optind < 1){
        printf("Usage: %s [options] <url>\nSee --help\n", argv[0]);
        return 1;
    }

    if(help){
        printf("browser [options] <url>\n");
        printf("browser data from <url> (Default HTTP)\n\n");
        printf("--help              Show Help\n");
        printf("-v, --verbose       Show extra information\n");

        return 0;
    }

    Lemon::URL url(argv[optind]);

    if(!url.IsValid() || !url.Host().length()){
        printf("browser: Invalid/malformed URL.\n");
        return 2;
    }

    enum {
        ProtocolHTTP,
        ProtocolHTTPS,
    } protocol = ProtocolHTTP;

    if(url.Protocol().length()){
        if(!url.Protocol().compare("http")){
            protocol = ProtocolHTTP;
        } else if(!url.Protocol().compare("https")){
            protocol = ProtocolHTTPS;
        } else {
            printf("browser: Unsupported protocol: '%s'.\n", url.Protocol().c_str());
            return 2;
        }
    }

    FILE* out = nullptr;
    if(outFile){
        FILE* file = fopen(outFile, "w");
        if(!file){
            perror("browser: Error creating file for output");
            return 3;
        }

        fseek(file, 0, SEEK_SET);
        out = file;
    } else {
        out = stdout;
    }

    bool sslLoaded = false;
    SSL_CTX* sslContext;
    const SSL_METHOD* method;

redirect:
    HTTPResponse response;

    if(protocol == ProtocolHTTP){
        int sock = socket(AF_INET, SOCK_STREAM, 0);

        uint32_t ip;
        if(int e = ResolveHostname(url.Host(), ip)){
            return e;
        }

        sockaddr_in address;
        address.sin_addr.s_addr = ip;
        address.sin_family = AF_INET;
        address.sin_port = htons(80);

        if(int e = connect(sock, reinterpret_cast<sockaddr*>(&address), sizeof(sockaddr_in)); e){
            perror("browser: Failed to connect");
            return 10;
        }

        RawSocket io(sock);
        if(int e = HTTPGet(&io, url.Host(), url.Resource(), response, out); e){
            return e;
        }

        close(sock);
    } else if(protocol == ProtocolHTTPS){
        if(!sslLoaded){
            SSL_load_error_strings();
            SSL_library_init();
            ERR_load_BIO_strings();
            OpenSSL_add_all_algorithms();

            method = TLSv1_2_client_method();
            sslContext = SSL_CTX_new(method);
        }

        if(!method){
            perror("browser: TLS Error obtaining SSL method");
            printf("browser: Error setting TLS connection hostname:");
            ERR_print_errors_fp(stderr);
            return 50;
        }

        SSL *ssl = SSL_new(sslContext);
        assert(ssl);

	    SSL_set_connect_state(ssl);
        SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

        BIO* sslBasicIO = BIO_new(BIO_f_ssl());
        assert(sslBasicIO);

        BIO_set_ssl(sslBasicIO, ssl, BIO_CLOSE);

        BIO* basicIO;
        if(url.Port().length()){
            basicIO = BIO_new_connect((url.Host() + url.Port()).c_str());
        } else {
            basicIO = BIO_new_connect((url.Host() + ":443").c_str());
        }
        assert(basicIO);

        if(BIO_do_connect(basicIO) <= 0){
            BIO_free_all(sslBasicIO);
            BIO_free_all(basicIO);

            printf("browser: (BIO_do_connect) Failed to establish TLS connection to '%s'.", url.Host().c_str());
            ERR_print_errors_fp(stdout);
            
            return 42;
        }

        BIO_push (sslBasicIO, basicIO);

        TLSSocket io(sslBasicIO);
        if(int e = HTTPGet(&io, url.Host(), url.Resource(), response, out); e){
            BIO_free_all(sslBasicIO);
            BIO_free_all(basicIO);

            return e;
        }

        BIO_free_all(sslBasicIO);
        BIO_free_all(basicIO);
    }

    if(response.statusCode == HTTPStatusCode::MovedPermanently){
        auto it = response.fields.find("location");
        if(it == response.fields.end()){
            printf("browser: Invalid redirect: No location field");
            return 13;
        }

        std::string& redirectURL = it->second;
        size_t pos = redirectURL.find_first_not_of(' ');
        url = Lemon::URL(redirectURL.substr(pos).c_str());

        if(!url.IsValid()){
            printf("browser: Invalid redirect: Invalid URL '%s'.\n", redirectURL.c_str());
            return 14;
        }

        if(!url.Protocol().compare("http")){
            protocol = ProtocolHTTP;
        } else if(!url.Protocol().compare("https")){
            protocol = ProtocolHTTPS;
        } else {
            printf("browser: Invalid redirect: Unknown URL protocol '%s'.\n", url.Protocol().c_str());
            return 15;
        }

        if(verbose){
            printf("browser: Redirecting to '%s' (%s).\n", url.Host().c_str(), url.Protocol().c_str());
        }
        goto redirect;
    } else if(response.statusCode != HTTPStatusCode::OK){
        printf("browser: HTTP %d.\n", response.statusCode);
        return 20;
    }

    return 0;
}
