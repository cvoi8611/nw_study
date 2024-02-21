#include "lib.h"

int main() {

    int clisock = socket(AF_INET, SOCK_STREAM, 0);
    if (clisock == INVALID_SOCKET) {
        cout << "socket() error" << endl;
        return 0;
    }

    int flags = fcntl(clisock, F_GETFL, 0);
    fcntl(clisock, F_SETFL, flags | O_NONBLOCK);

    char ip[16] = "";
    cout << "Input server IP: "; cin >> ip;

    sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(ip);
    servAddr.sin_port = htons(8000);

    while (true) {
        if (connect(clisock, (sockaddr*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR) {
            if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
                continue;
            }
            else if (errno == EISCONN || errno == EALREADY) {
                break;
            }
            else {
                cout << "connect() error"<< endl;
                return 0;
            }
        }
    }
    
    char buf[1024] = "";

    while (true) {
        cout << "Input: "; cin >> buf;

        while (true) {
            if (send(clisock, buf, strlen(buf) + 1, 0) == SOCKET_ERROR) {
                if (errno == EWOULDBLOCK || errno == EINPROGRESS) {
                    continue;
                }
                else {
                    cout << "send() error" << endl;
                    return 0;
                }
            }
            else {
                break;
            }
        }

        int recvlen;
        while (true) {
            recvlen = recv(clisock, buf, sizeof(buf), 0);
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

        if (recvlen == 0) {
            break;
        }

        buf[recvlen] = '\0';

        cout << "Echo : " << buf << endl;
    }
    

    close(clisock);
    return 0;
}