#include "external.hpp"
#include "text.hpp"

using namespace std;
using namespace restCpp;

int connectSocket(string hostname, int portno) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd <= 0) throw runtime_error("could not create socket");
    struct hostent* server = gethostbyname(hostname.c_str());
    if (server == NULL)  throw runtime_error("no such host");
    struct sockaddr_in serv_addr;
    memset((char *) &serv_addr,0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
        throw runtime_error("could not connect");
    return sockfd;
}

string request(string url, string data="", string contentType="text/plain") {
    string prot = before(url,"//");
    if (prot != "http:") 
        throw runtime_error("only http suported");
    string rest = after(url, "//");
    string hostname = before(rest,"/");
    int portno = 80;
    if (contains(hostname , ":")) {
        string port = after(hostname , ":");
        portno = atoi(port.c_str());
        hostname = before(hostname,":");
    }
    string path = "/";
    if (contains(rest,"/")) {
        string path = "/" + (after(rest,"/"));
    }
    int sockfd = connectSocket(hostname,portno);
    string msg;
    if (data == "") {
        msg = "GET " + path + " HTTP/1.0\r\n\r\n";
    } else {
        stringstream oss;
        oss << "POST " << path << " HTTP/1.0\r\n";
        oss << "Content-Type: " << contentType << "\r\n";
        oss << "Content-Length: " << data.size() << "\r\n\r\n" << data;
        msg = oss.str();
    }
    int n = write(sockfd,msg.c_str(),msg.size());
    if (n < 0) throw runtime_error("ERROR writing to socket");
    const int MAX = 2000;
    char buffer[MAX];
    bzero(buffer,MAX);
    string out = "";
    while (1) { 
        n = read(sockfd,buffer,MAX);
        if (n < 0) throw runtime_error("read error");
        if (n == 0) break;
        out.append(buffer,n);
    }
    close(sockfd);
    string firstLine = before(out,"\r\n");
    string status = before(after(firstLine , " ") , " ");
    if (status != "200") throw runtime_error(status + " http error");
    string body = after(out , "\r\n\r\n");
    return body;
}

int main(int argc,char** argv) {
    if (argc < 2) {
        cerr << "usage:\n" << 
        "echo body | " << argv[0] << " <url> <contentType>" << endl;
        cerr << "or:\n" << argv[0] << " <url> <contentType>" << endl;
        cerr << "or:\n" << argv[0] << " <url>" << endl;
        return(1);
    }
    string data = "";
    string out = request(argv[1],data);
    cout << out << endl;
}
