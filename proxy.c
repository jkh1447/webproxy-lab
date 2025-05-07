#include <stdio.h>
#include "csapp.h"
#include "ctype.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void doit(int fd);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE]; // 호스트이름과 포트
  socklen_t clientlen; // 소켓 주소 구조체의 크기
  struct sockaddr_storage clientaddr;


  printf("%s", user_agent_hdr);

  if(argc != 2){
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); //듣기 소켓을 만듬
  //printf("open listenfd\n");
  while(1){
    clientlen = sizeof(clientaddr);
    // 듣기 식별자

    // 클라이언트의 요청이 올 때 까지 대기하면서 요청이 오면 연결 식별자를 생성 ex) telnet localhost 8080
    // accept를 통해 접속한 클라이언트의 주소와 포트를 업데이트함
    connfd = Accept(listenfd, (SA *)&clientaddr, 
                    &clientlen);  // line:netp:tiny:accept
    // 문자열 주소를 받아옴
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);

    // 트랜잭션을 수행
    doit(connfd);   // line:netp:tiny:doit
    Close(connfd);  // line:netp:tiny:close
  }

  return 0;
}

void doit(int fd){

  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  // char* host = "127.0.0.1";
  // char* port = "8080";
  char host[MAXLINE];
  char port[16];
  int clientfd;
  rio_t rio;
  rio_t rio_2;

  Rio_readinitb(&rio, fd);
  
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request Header:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  

  char *tmp = strstr(uri, "://");
  char *host_start, *path_start, *colon_pos;
  if (tmp != NULL) {
      host_start = tmp + 3; // :// 바로 다음 포인터
      path_start = strchr(host_start, '/'); // ://이후 처음으로 /가 시작하는 부분
      
      if (path_start) {
          strcpy(uri, path_start);  // 경로 복사
      } else {
          strcpy(uri, "/");
      }
  
      colon_pos = strchr(host_start, ':'); // :가 처음으로 등장하는 위치
      if (colon_pos && colon_pos < path_start) {
          int host_len = colon_pos - host_start; // host의 길이
          strncpy(host, host_start, host_len);
          host[host_len] = '\0';
  
          int port_len = path_start - (colon_pos + 1);
          strncpy(port, colon_pos + 1, port_len);
          port[port_len] = '\0';
      } else {
          int host_len = path_start - host_start;
          strncpy(host, host_start, host_len);
          host[host_len] = '\0';
  
          strcpy(port, "80");  // 기본값
      }
  }


  //printf("cur uri: %s\n", uri);
  clientfd = Open_clientfd(host, port);
  
  char request_line[MAXLINE];
  snprintf(request_line, sizeof(request_line), "%s %s %s\r\n", method, uri, version);
  Rio_writen(clientfd, request_line, strlen(request_line));
  printf("%s", request_line);

  Rio_writen(clientfd, buf, strlen(buf));
  
  Rio_readlineb(&rio, buf, MAXLINE); 

  printf("%s", buf);
  while(strcmp(buf, "\r\n")){
    Rio_writen(clientfd, buf, strlen(buf));
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("%s", buf);
  }
  Rio_writen(clientfd, buf, strlen(buf));
  

  int c_len = 0;
  // tiny서버에서 받은 응답을 다시 클라이언트에게 돌려준다.
  printf("Proxy: Request return to client\n");

  Rio_readinitb(&rio_2, clientfd);
  Rio_readlineb(&rio_2, buf, MAXLINE); 

  printf("%s", buf);
  while(strcmp(buf, "\r\n")){
    if(strcasestr(buf, "Content-length")){
      char *p = strchr(buf, ':');
      // while (*p != '\0' && isspace((unsigned char)*p)) { 
      //   p++; // 공백 건너뛰기
      // }
      c_len = atoi(p+2);
    }
    Rio_writen(fd, buf, strlen(buf));
    Rio_readlineb(&rio_2, buf, MAXLINE);
    printf("%s", buf);
  }
  printf("Content-length: %d", c_len);
  Rio_writen(fd, buf, strlen(buf));
  printf("%s", buf);

  Rio_readn(clientfd, buf, c_len);
  printf("%s", buf);
  Rio_writen(fd, buf, c_len);
}