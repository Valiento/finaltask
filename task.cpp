#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/stat.h>

#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <sstream>
#include <stdio.h>
#include <fcntl.h>
#include <cerrno>
#include <thread>

std::string directory;

// ===========================================================================================================
std::string http_error_404() {
    std::stringstream ss;
    ss << "HTTP/1.0 404 NOT FOUND";
    ss << "\r\n";
    ss << "Content-length: 0";
    ss << "\r\n";
    ss << "Content-type: text/html";
    ss << "\r\n\r\n";
    return ss.str();
}
// -----------------------------------------------------------------------------------------------------------
std::string http_ok_200(const std::string &data) {
    std::stringstream ss;
    ss << "HTTP/1.0 200 OK";
    ss << "\r\n";
    ss << "Content-length: ";
    ss << data.size();
    ss << "\r\n";
    ss << "Content-type: text/html";
    ss << "\r\n";
    ss << "Content: ";
    ss << data;
    ss << "\r\n\r\n";
    return ss.str();
}
// -----------------------------------------------------------------------------------------------------------
std::string parse_request(const std::string &request) {

    std::size_t pos1 = request.find("GET /");
    std::size_t pos2 = request.find(" HTTP/1");
    if (pos1 == std::string::npos || pos2 == std::string::npos)
        return "";
    std::string index = request.substr(pos1+5, pos2-pos1-5);
    if (index.size() == 0)
        return "index.html";

    std::size_t pos = index.find('?');
    if (pos == std::string::npos)
        return index;
    else return index.substr(0, pos);
}
// ===========================================================================================================
void process(int &fd, const std::string &request) {
    std::string filename = parse_request(request);
    if (filename == "") {
        std::string err = http_error_404();
        send(fd, err.c_str(), err.length() + 1, MSG_NOSIGNAL);
        return;
        // ---------------------------------------------------------------------------------------------------
    } else {
        std::stringstream ss;
        ss << directory;
        if (directory.length() > 0 && directory[directory.length()-1] != '/')
            ss << "/";
        ss << filename;

        FILE *file = fopen(ss.str().c_str(), "r");
        // ---------------------------------------------------------------------------------------------------
        if (file) {
            std::stringstream ss;
            std::string data;
            char c = '\0';
            while ((c = fgetc(file)) != EOF)
                ss << c;
            data = ss.str();
            // -----------------------------------------------------------------------------------------------
            std::string ok = http_ok_200(data);
            send(fd, ok.c_str(), ok.size(), MSG_NOSIGNAL);
            fclose(file);
            // -----------------------------------------------------------------------------------------------
        } else {
            std::string err = http_error_404();
            send(fd, err.c_str(), err.size(), MSG_NOSIGNAL);
        }
    }
}
// ===========================================================================================================
void add_event(int epfd, int master, bool oneshot) {
    struct epoll_event event;
    event.data.fd = master;
    event.events = EPOLLIN | EPOLLET;
    if (oneshot)
        event.events |= EPOLLONESHOT;

    epoll_ctl(epfd, EPOLL_CTL_ADD, master, &event);
}
// -----------------------------------------------------------------------------------------------------------
void set_oneshot(int &epfd, int &fd) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event);
}
// -----------------------------------------------------------------------------------------------------------
void work(int epfd, int slave) {
    int len = 10;
    char buf[len];
    std::string request;
    std::fill(buf, buf+len, '\0');
    // -------------------------------------------------------------------------------------------------------
    while (1) {
        int size = recv(slave, buf, size-1, 0);
        if (size) {
            shutdown(slave, SHUT_RDWR);
            close(slave);
            break;
        } else if (size < 0 && errno == EAGAIN) {
            set_oneshot(epfd, slave);
            process(slave, request);
            break;
        } else {
            request += buf;
        }
    }
}
// ===========================================================================================================
void get_args(int argc, char *argv[], sockaddr_in &addr) {
    char option;
    addr.sin_family = AF_INET;
    while ((option = getopt(argc, argv, "h:p:d:")) != -1) {
        switch(option) {
            case 'h':
                addr.sin_addr.s_addr = inet_addr(optarg);
                break;
            case 'p':
                addr.sin_port = htons(atoi(optarg));
                break;
            case 'd':
                directory = optarg;
        }
    }
}
// -----------------------------------------------------------------------------------------------------------
int setnonblock(int fd) {
    int flags;
    #if defined(O_NONBLOCK)
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, (unsigned) flags | O_NONBLOCK);
    #else
    flags = 1;
    return ioctl(fd, FIONBIO, &flags);
    #endif
}
// -----------------------------------------------------------------------------------------------------------
int run(int argc, char *argv[]) {
    int master, epfd, prep, opt = 1;
    // -------------------------------------------------------------------------------------------------------
    if ((master = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        exit(-1);;

    setnonblock(master);
    struct sockaddr_in addr;
    get_args(argc, argv, addr);
    
    if (setsockopt(master, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        exit(-1);
    
    if (bind(master, (struct sockaddr *) &addr, sizeof(addr)))
        exit(-1);

    if (listen(master, SOMAXCONN) < 0)
        exit(-1);
    // -------------------------------------------------------------------------------------------------------
    struct epoll_event events[1024];
    if ((epfd = epoll_create1(0)) < 0)
        exit(-1);
    add_event(epfd, master, false);
    // -------------------------------------------------------------------------------------------------------
    while (1) {
        if ((prep = epoll_wait(epfd, events, 1024, -1)) < 0)
            break;
        // ---------------------------------------------------------------------------------------------------
        for (int i = 0; i < prep; ++i) {
            if (events[i].data.fd == master) {
                int slave = accept(master, 0, 0);
                add_event(epfd, slave, true);
                // -------------------------------------------------------------------------------------------
            } else {
                int fd = events[i].data.fd;
                std::thread worker(&work, epfd, fd);
                worker.detach();
            }
        }
    }
    // -------------------------------------------------------------------------------------------------------
    close(master);
    close(epfd);
    return 0;
}
// ===========================================================================================================
void daemonize() {
    pid_t pid = fork();

    if (pid == -1)
        exit(-1);
    else if (!pid)
        exit(0);

    chdir("/");
    umask(0);
    setsid();

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}
// -----------------------------------------------------------------------------------------------------------
int main(int argc, char *argv[]) {
    daemonize();
    //while (1)
    run(argc, argv);
    return 0;
}
