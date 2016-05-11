#include "external.hpp"
#include "text.hpp"
using namespace std;
using namespace restCpp;

#define P(X) if ((X) < 0) throw runtime_error(#X)

class Selectable {
    public:
        bool verbose = false;
        static const int ctrlC = 2;
        static const int ctrlPipe = 3;
        static const int ctrlZ = SIGTSTP;
        static const int kill = SIGTERM;
        virtual int fileno() = 0;
        virtual void whenReady() = 0;
        virtual void onTimeOut() { }
        virtual void onSignal(int signal) { }
};

constexpr double INF = std::numeric_limits<double>::infinity();

int dispatch(vector<Selectable*> selectables, double wait) {
    // returns what select returns; 0 on timeout, number of actives otherwise
    if (wait != wait || wait < 0)
        throw runtime_error("invalid wait!");
	fd_set readSet, writeSet, errorSet;
		FD_ZERO( &readSet);
		errorSet = writeSet = readSet;
	    struct timeval tval;
    if (wait != INF) {
        tval.tv_sec =  (int) wait;
	    tval.tv_usec = (int) ((wait - tval.tv_sec) * 1000 * 1000);
    }
    int maxVal = 0;
    for (auto i=selectables.begin();i != selectables.end(); i++){
        Selectable* s = *i;
        int j = s->fileno();
        if (j > maxVal) maxVal = j;
		    FD_SET(j, &errorSet);
		    FD_SET(j, &readSet);
    }
		int ret;
    if (wait == INF)
        ret = select(maxVal+1, &readSet, &writeSet, &errorSet, NULL);
    else
        ret = select(maxVal+1, &readSet, &writeSet, &errorSet, &tval);
    if (ret == 0) return ret;
    if( ret < 0) {	// error
        if (errno == EINTR) {
            errno = 0;
            return ret;
        }
        string msg = string("unexpected errno ") + to_string(errno);
        errno = 0;
        throw runtime_error(msg);
    }
    for (auto i=selectables.begin();i != selectables.end(); i++){
        Selectable* s = *i;
        int j = s->fileno();
		    if(FD_ISSET(j, &errorSet)) 
            throw runtime_error("socket in error state");
		    if(FD_ISSET(j, &readSet)) {
                s->whenReady();
            }
    }
    return ret;
}

using smap = map<string,string>;
using Ip = string;
class Connection {
    private:
        int fd_;
        static const int bufferSize_ = 4096;
    public:
        virtual int fileno(){ return fd_; }
        Connection(int fd) { 
            fd_ = fd; 
        }
        ~Connection() {
            close(fd_);
        }
        void operator()(string& data) {
            write(fd_,data.c_str(),data.size());
        }
        string operator()() {
            char buffer[bufferSize_];
            int got = recv(fd_,buffer,bufferSize_,0);
            return string(buffer,got);
        }
        Ip getPeerIp() {
            socklen_t len;
            struct sockaddr_storage addr;
            char ipstr[INET6_ADDRSTRLEN];
            int port;
            
            len = sizeof addr;
            getpeername(fd_, (struct sockaddr*)&addr, &len);
            
            // deal with both IPv4 and IPv6:
            if (addr.ss_family == AF_INET) {
                struct sockaddr_in *s = (struct sockaddr_in *)&addr;
                port = ntohs(s->sin_port);
                inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
            } else { // AF_INET6
                struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
                port = ntohs(s->sin6_port);
                inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
            }
            
            //printf("Peer IP address: %s\n", ipstr);
            //printf("Peer port      : %d\n", port);
            return string(ipstr);
        }
};

class Listener {
    private:
        int listening;
    public:
        bool verbose;
        virtual int fileno() { return listening; }
        Listener(uint16_t port) {
            P(listening = socket(AF_INET, SOCK_STREAM, 0));
            int one = 1;
            P(setsockopt(listening,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one)));

            struct sockaddr_in servaddr;
            int b = sizeof(servaddr);
            memset(&servaddr, 0, b);
            servaddr.sin_family      = AF_INET;
            servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
            servaddr.sin_port        = htons(port);

            P(::bind(listening,(struct sockaddr *) &servaddr, b));
            P(listen(listening,128));
        }
        virtual ~Listener() {
            if (verbose) cerr << "closing listening port" << endl;
            P(close(listening));
        }
        Connection* doAccept(){
            struct sockaddr_in cli_addr;
            uint32_t clilen = sizeof(cli_addr);
            int newsockfd = accept(listening,
                 (struct sockaddr *) &cli_addr,
                 &clilen);
            Connection* con = new Connection(newsockfd);
            return con;
        }
};

class HttpServer: public Selectable {
private:
    Listener listener;
public:
    HttpServer(uint16_t port=8080):listener(port) { }
    virtual int fileno() override { return listener.fileno(); }
    virtual void whenReady() override {
        if (verbose) cerr << "<HttpServer.whenReady()>" << endl;
        Connection* conn = listener.doAccept();
        string received = "";
        if (verbose) cerr << "\t<HttpServer.whenReady.receive>" << endl;
        while (1) {
            received += (*conn)();
            if (contains(received , "\r\n\r\n")) break;
        }
        if (verbose) cerr << repr(received) << endl;
        if (verbose) cerr << "\t</HttpServer.whenReady.receive>" << endl;
        string first = before(received,"\r\n");
        string method = before(first," ");
        smap fields;
        string data = after(received,"\r\n\r\n");
        string header = before(received,"\r\n\r\n");
        if (contains(header,"\r\n")) {
            string rest = after(header,"\r\n");
            auto lines = split(rest,"\r\n");
            for (auto i=lines.begin();i!=lines.end();i++) {
                string abc = strip(before(*i , ":"));
                string xyz = strip(after(*i , ":"));
                fields[abc] = xyz;
            }
        }
        string path = before(after(first," ")," ");
        Ip ip = conn->getPeerIp();
        string out;
        try {
            out = onRequest(path,method,fields,data,ip);
        } catch (exception& e) {
            string prob = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
            prob += e.what();
            (*conn)(prob);
            delete conn;
            return;
        }
        (*conn)(out);
        delete conn;
        if (verbose) cerr << "</HttpServer.whenReady()>" << endl;
    }
    virtual string 
    onRequest(string& path,string& method,smap& fields,string& data,Ip& ip) {
        stringstream oss;
        oss << "HTTP/1.0 200 OK\r\n\r\n";
        oss << "ip=>" << ip << "<=" << endl;
        oss << "path=>" << path << "<=" << endl;
        oss << "method=>" << method << "<=" << endl;
        oss << "data=>" << data << "<=" << endl;
        for (auto i=fields.begin();i!=fields.end();i++)
            oss << "(" << i->first << "|" << i->second << ")" << endl;
        return oss.str();
    }
    virtual void onSignal(int signal) override { 
        if (verbose) cerr << " HttpServer got signal: " << signal << endl;
    }
};

int main(int argc,char** argv) {
    HttpServer httpServer;
    //httpServer.verbose = true;
    vector<Selectable*> selectables;
    selectables.push_back(&httpServer);
    while (true) {
        int out = dispatch(selectables,INF);
        if (not out) break;
    }
}
