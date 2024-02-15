#include "lib.h"

std::string Readfile(std::string filepath){
    std::ifstream file(filepath);
	std::string s;
	if (file.is_open()) {
		file.seekg(0, std::ios::end);
		int size = file.tellg();
		s.resize(size);
		file.seekg(0, std::ios::beg);
		file.read(&s[0], size);
		//std::cout << s << std::endl;
        return s;
	}
	else {
		s = "파일을 찾을 수 없습니다!";
        return s;
	}
}

int main() {
    int servsock = socket(AF_INET, SOCK_STREAM, 0);
    if (servsock == INVALID_SOCKET) {
        cout << "socket() error" << endl;
        return 0;
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
        return 0;
    }

    if (listen(servsock, SOMAXCONN) == SOCKET_ERROR) {
        cout << "listen() error" << endl;
        return 0;
    }

    while (true) {
        sockaddr_in cliaddr;
        uint addrlen = sizeof(cliaddr);
        int clisock = accept(servsock, (sockaddr*)&cliaddr, &addrlen);
        if (clisock == INVALID_SOCKET) {
            if (errno == EWOULDBLOCK) {
                continue;
            }
            else {
                cout << "accept() error: " << endl;
                return 0;
            }
        }

        cout << "Client Connected" << endl;

        char buf[1024] = "";

        //쿠키 user 값
        char username[32];
        int user_id = 0;

        int recvlen;
        while (true) {
            recvlen = recv(clisock, buf, sizeof(buf), 0);
            if (recvlen == SOCKET_ERROR) {
                if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
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
        //main page
        if (strstr(buf, "GET / HTTP/1.1") != NULL) {
            

            strcpy(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection:close\r\n\r\n");
            std::string filepath = "html/index.html";
            std::string content = Readfile(filepath);
            strcat(buf, content.c_str());
        }

        //error page
        if (strstr(buf, "GET /logout HTTP/1.1") != NULL) {
            if (strstr(buf , "Cookie") != NULL){
                cout << "로그인 상태임을 확인함" << endl;
                //Max-Age를 -1로 설정하여 쿠키를 즉시 삭제함
                strcpy(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nSet-Cookie: user=");
                strcat(buf, username);
                strcat(buf, "; Max-Age=-1; expires=Sat, 02 Oct 2022 11:34:04 GMT;\r\n\r\n");
            }
            // strcpy(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
            std::string filepath = "html/logout.html";
            std::string content = Readfile(filepath);
            strcat(buf, content.c_str());
        }

        //test json page
        if (strstr(buf, "GET /test HTTP/1.1") != NULL) {
            strcpy(buf, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n");
            std::string filepath = "json/test.json";
            std::string content = Readfile(filepath);
            strcat(buf, content.c_str());
        }

        //user json page
        if (strstr(buf, "GET /user HTTP/1.1") != NULL) {
            strcpy(buf, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n");
            std::string filepath = "json/user.json";
            std::string content = Readfile(filepath);
            strcat(buf, content.c_str());
        }



        //login success page
        if (strstr(buf, "GET /login HTTP/1.1") != NULL) {
            if (strstr(buf, "Cookie") != NULL){
                strcpy(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h2>Cookie yes</h2><p>Welcome ");
                strcat(buf, username);
                strcat(buf, "</p>");

            }
            else {
                strcpy(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h2>Cookie no</h2>");
            }
        }

        //register page
        if (strstr(buf, "GET /register HTTP/1.1") != NULL) {
            strcpy(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
            std::string filepath = "html/register.html";
            std::string content = Readfile(filepath);
            strcat(buf, content.c_str());
        }

        //get
        if (strstr(buf, "GET /login") != NULL){
            cout << "---------------------------" << endl;
            cout << "Recv : " << buf << endl;
            char* ptr_1 = strstr(buf, "?name=");
            ptr_1 += strlen("?name=");
            char* ptr_2 = strstr(buf, " HTTP/1.1");

            int len = ptr_2 - ptr_1;
            strncpy(username, ptr_1, len);
            username[strcspn(username, " ")] = '\0';

            strcpy(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nSet-Cookie: user=");
            string username_str(username);
            strcat(buf, username);
            strcat(buf, "; id=");
            std::string id_str = to_string(user_id);
            strcat(buf, id_str.c_str());
            strcat(buf, "; Max-Age=300; Path=/\r\n\r\n");
            string str1("<h1>Hello, ");
            string str2("!</h1>");
            str1 += username;
            str1 += str2;
            strcat(buf, str1.c_str());


            //json 업데이트 코드
            std::string filepath = "json/user.json";
            std::string content = Readfile(filepath);
            cout << "json file :" << content << endl;

            size_t position = content.find("\"user\": [");
            position = content.find("[", position + 1);
            //std::string username_str(username);
            content.insert(position + 1, "\n{\"name\": \"" + username_str + "\", \"id\": " + id_str + "},");

            std::ofstream outFile("user.json");
            outFile << content;
            outFile.close();
        }

        if (recvlen == 0) {
            close(clisock);
            cout << "Client Disconnected" << endl;
            break;
        }

        buf[recvlen] = '\0';

        cout << "Recv: " << buf << endl;

        
        while (true) {
            int sendlen;
            sendlen = send(clisock, buf, strlen(buf) + 1, 0);
            if (sendlen == SOCKET_ERROR) {
                if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
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

        close(clisock);
        cout << "Client Disconnected" << endl;
        continue;
    }
    close(servsock);
    cout << "Server Socket Closed" << endl;
    return 0;
}
