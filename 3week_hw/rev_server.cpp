#include "lib.h"

#define DEFAULT_BUFLEN 1024
#define MAX_EVENTS 10 // epoll의 한 번에 처리할 이벤트 수

struct Session {
    int sock = INVALID_SOCKET;
    char buf[DEFAULT_BUFLEN];
    int recvbytes = 0;
    int sendbytes = 0;
};

int main() {

    // 서버용 소켓
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
    servaddr.sin_port = htons(8000);

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

    // 리버스 프록시 서버 -> 서버로 Connect 시도

    int recvsock = socket(AF_INET, SOCK_STREAM, 0);
    if (recvsock == INVALID_SOCKET) {
        cout << "socket() error" << endl;
        return 0;
    }

    flags = fcntl(recvsock, F_GETFL, 0);
    fcntl(recvsock, F_SETFL, flags | O_NONBLOCK);

    char ip[16] = "127.0.0.1";
    //cout << "Input server IP: "; cin >> ip;

    sockaddr_in ToservAddr;
    memset(&ToservAddr, 0, sizeof(ToservAddr));
    ToservAddr.sin_family = AF_INET;
    ToservAddr.sin_addr.s_addr = inet_addr(ip);
    ToservAddr.sin_port = htons(8001);

    cout << "Connecting..." << endl;

    while (true) {
        if (connect(recvsock, (sockaddr*)&ToservAddr, sizeof(ToservAddr)) == SOCKET_ERROR) {
            if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
                continue;
            }
            else if (errno == EISCONN || errno == EALREADY) {
                break;
            }
            else {
                continue;
            }
        }
    }

    cout << "Connected to Server" << endl;

    //connect에 성공하면? epoll 이벤트 감지 시작, 즉 연결이 되고 나서부터 입력을 받기 시작함

while (true) {
        int epfds = epoll_wait(epollfd, epEvents, MAX_EVENTS, INFINITE);
        if (epfds == SOCKET_ERROR) {
            cout << "epoll_wait() error" << endl;
            break;
        }

        for (int i = 0; i < epfds; i++) {
            if (epEvents[i].data.fd == servsock) {
                int clisock;
                sockaddr_in cliaddr;
                socklen_t addrlen = sizeof(cliaddr);

                clisock = accept(servsock, (sockaddr*)&cliaddr, &addrlen);
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

                cout << "Client Connected" << endl;

                continue;
            }

            // 동일하게 포인터를 사용해서 효율적으로 처리
            Session* session = (Session*)epEvents[i].data.ptr;
            bool isClosed = false; // 소켓이 닫히면 두 번 break를 하기 위한 플래그

            if (epEvents[i].events & EPOLLIN) {
                while (true) { // ET 모드에서는 반복문을 사용하여 모든 데이터를 읽어야 함

                    int recvlen = recv(session->sock, session->buf, DEFAULT_BUFLEN, 0);

                    if (recvlen == SOCKET_ERROR && errno == EAGAIN) {
                        // 클라이언트 -> 리버스 프록시 서버 로 Input이 들어옴
                        cout << "(Rev_Serv) Client -> Rev_Serv : " << session->buf << endl;

                        char buf[1024] = "";
                        strcpy (buf, (session->buf));

                        while (true) {
                            if (send(recvsock, buf, strlen(buf) + 1, 0) == SOCKET_ERROR) {
                                if (errno == EWOULDBLOCK || errno == EINPROGRESS) {
                                    continue;
                                }
                                else {
                                    // cout << "errno : " << errno << endl;
                                    // cout << "strerror : " << strerror(errno) << endl;
                                    cout << "send() error" << endl;
                                    break;
                                }
                            }
                            else {
                                cout << "(Rev_Serv) Rev_Serv -> Serv : " << buf << endl;
                                break;
                            }
                        }

                        if (isClosed) break;

                        int recvlen;
                        while (true) {
                            recvlen = recv(recvsock, buf, sizeof(buf), 0);
                            if (recvlen == SOCKET_ERROR) {
                                if (errno == EWOULDBLOCK || errno == EINPROGRESS) {
                                    continue;
                                }
                                else {
                                    cout << "recv() error" << endl;
                                    return 0;
                                }
                            }
                            else {
                                break;
                            }
                        }

                        buf[recvlen] = '\0';
                        
                        cout << "(Rev_Serv) Serv -> Rev_Serv : " << buf << endl;

                        strcpy((session->buf), buf);
                        break;
                    }
                    else if (recvlen > 0) {
                        session->recvbytes += recvlen;
                        if (session->recvbytes == DEFAULT_BUFLEN) {
                            break;
                        }
                    }
                    else {
                        cout << "Client Disconnected" << endl;
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
                            // ET 모드에서는 EAGAIN 에러가 발생하면 모든 데이터를 보낸 것
                            // 클라이언트 -> 리버스 프록시 서버 로 Input이 들어옴
                            
                            cout << "(Rev_Serv) Rev_Serv -> Client : " << session->buf << endl;

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