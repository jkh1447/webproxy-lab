#include "csapp.h"

int main(int argc, char ** argv)
{
  int clientfd;
  char *host, *port, buf[MAXLINE];
  rio_t rio;
  // 예 입력, telnet www.aol.com 80
  if(argc != 3){
    // 파일스트림에 쓰기
    fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
    exit(0);
  }
  
  host = argv[1];
  port = argv[2];

  clientfd = Open_clientfd(host, port);
  Rio_readinitb(&rio, clientfd);
  
  // Fgets: 표준입력에서 한 줄을 읽어서 buf에 저장
  while(Fgets(buf, MAXLINE, stdin) != NULL){
    // 읽은 문자가 저장된 buf를 서버에 보냄
    Rio_writen(clientfd, buf, strlen(buf));
    // 서버가 보낸 응답을 buf에 저장
    Rio_readlineb(&rio, buf, MAXLINE);
    // 화면에 출력
    Fputs(buf, stdout);
  }
  Close(clientfd);
  exit(0);
}