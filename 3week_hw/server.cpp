#include "lib.h"

#define DEFAULT_BUFLEN 1024
#define MAX_EVENTS 10

struct Session {
    int sock = INVALID_SOCKET;
    char buf[DEFAULT_BUFLEN];
    int recvbytes = 0;
    int sendbytes = 0;
};

int main() {
    int servsock = socket(AF_INET, SOCK_STREAM, 0);
    if (servsock == INVALID_SOCKET) {
        cout << "socket() error" << endl;
        return 1;
    }

    int flags = fcntl(servsock, F_GETFL, 0);
    fcntl(servsock, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(8001);

    if (bind(servsock, (sockaddr*)&servaddr, sizeof(servaddr)) == SOCKET_ERROR) {
        cout << "bind() error" << endl;
        return 1;
    }

    if (listen(servsock, SOMAXCONN) == SOCKET_ERROR) {
        cout << "listen() error" << endl;
        return 1;
    }

    vector<Session*> sessions;
    sessions.reserve(100);

    sessions.push_back(new Session{servsock});

    int epollfd = epoll_create1(0);
    if (epollfd == SOCKET_ERROR) {
        cout << "epoll_create1() error" << endl;
        return 1;
    }

    epoll_event epEvents[MAX_EVENTS], epEvent;

    epEvent.events = EPOLLIN | EPOLLET;
    epEvent.data.fd = servsock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, servsock, &epEvent) == SOCKET_ERROR) {
        cout << "epoll_ctl() error" << endl;
        return 1;
    }

    cout << "Connect Waiting..." << endl;

    while (true) {
        int epfds = epoll_wait(epollfd, epEvents, MAX_EVENTS, INFINITE);
        if (epfds == SOCKET_ERROR) {
            cout << "epoll_wait() error" << endl;
            break;
        }

        for (int i = 0; i < epfds; i++) {
            if (epEvents[i].data.fd == servsock) {
                sockaddr_in cliaddr;
                socklen_t addrlen = sizeof(cliaddr);

                int clisock = accept(servsock, (sockaddr*)&cliaddr, &addrlen);
                if (clisock == INVALID_SOCKET) {
                    cout << "accept() error" << endl;
                    break;
                }

                int flags = fcntl(clisock, F_GETFL, 0);
                fcntl(clisock, F_SETFL, flags | O_NONBLOCK);

                Session* newSession = new Session{clisock};

                epEvent.events = EPOLLIN | EPOLLOUT | EPOLLET;
                epEvent.data.ptr = newSession;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, clisock, &epEvent) == SOCKET_ERROR) {
                    cout << "epoll_ctl() error" << endl;
                    return 1;
                }

                sessions.push_back(newSession);

                cout << "Reverse Server Connected" << endl;
                continue;
            }
            Session* session = (Session*)epEvents[i].data.ptr;
            bool isClosed = false;

            if (epEvents[i].events & EPOLLIN) {
                while (true) {
                    int recvlen = recv(session->sock, session->buf, DEFAULT_BUFLEN, 0);
                    if (recvlen == SOCKET_ERROR && errno == EAGAIN) {
                        cout << "(Server) Rev_Server -> Server : " << session->buf << endl;
                        if (strstr((session->buf),"HelloWorld!") != NULL ){
                            strcpy((session->buf),"Welcome!");
                        }
                        cout << "(Server) Server -> Rev_Server : " << session->buf << endl;
                        break;
                    }
                    else if (recvlen > 0) {
                        session->recvbytes += recvlen;
                        if (session->recvbytes == DEFAULT_BUFLEN) {
                            break;
                        }
                    }
                    else {
                        cout << "Rev_Server Disconnected" << endl;
                        close(session->sock);
                        sessions.erase(
                            remove(sessions.begin(), sessions.end(), session),
                            sessions.end()
                        );
                        delete session;
                        isClosed = true;

                        break;
                    }
                }

                if (isClosed) { continue; }
            }

            if (epEvents[i].events & EPOLLOUT) {
                while (true) {
                    int sendlen = send(session->sock, session->buf, session->recvbytes, 0);
                    if (sendlen == SOCKET_ERROR) {
                        if (errno == EAGAIN) {
                            break;
                        }
                        cout << "send() error" << endl;
                        close(session->sock);
                        sessions.erase(
                            remove(sessions.begin(), sessions.end(), session),
                            sessions.end()
                        );
                        delete session;

                        break;
                    }

                    session->sendbytes += sendlen;

                    if (session->sendbytes == session->recvbytes) {
                        session->sendbytes = 0;
                        session->recvbytes = 0;

                        break;
                    }
                }
            }
        }
    }

    close(servsock);
    close(epollfd);
    return 0;
}