/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, int isHEAD);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

/*
tiny 서버는 반복실행 서버이다.
전형적인 무한 서버 루프를 실행한다
*/
// void sigchld_handler(int sig){
//   while(waitpid(-1, NULL, WNOHANG) > 0);
// }

void sigchld_handler(int sig) {
  int status;
  pid_t pid;
  // 종료된 자식 프로세스를 정리
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
      if (WIFEXITED(status)) {
          // 자식 프로세스가 정상적으로 종료된 경우
          printf("Child process %d terminated normally with exit status %d\n", pid, WEXITSTATUS(status));
      } else if (WIFSIGNALED(status)) {
          // 자식 프로세스가 신호로 종료된 경우
          printf("Child process %d terminated by signal %d\n", pid, WTERMSIG(status));
      } else if (WIFSTOPPED(status)) {
          // 자식 프로세스가 신호로 멈춘 경우
          printf("Child process %d stopped by signal %d\n", pid, WSTOPSIG(status));
      }
  }

  if (pid == -1) {
      perror("waitpid error");
  }
}

int main(int argc, char **argv) {
  int listenfd, connfd; // 듣기식별자와, 연결식별자
  char hostname[MAXLINE], port[MAXLINE]; // 호스트이름과 포트
  socklen_t clientlen; // 소켓 주소 구조체의 크기
  struct sockaddr_storage clientaddr;
  
  // 자식프로세스 종료를 위한 핸들러
  signal(SIGCHLD,sigchld_handler);

  /*
  argc: 전달된 인자의 개수
  argv: 인자 문자열들의 배열
  예) ./server 8080
  */
  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  // 듣기 소켓 오픈, 
  // 서버쪽 소켓은 대체로 모든 요청을 받기 때문에 host쪽은 와일드카드이므로 port만 인자로 넘겨준다.
  listenfd = Open_listenfd(argv[1]); 
  while (1) {
    clientlen = sizeof(clientaddr);
    // 듣기 식별자
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
}

/*

rio_t 구조체의 정의는 다음과 같다:

#define RIO_BUFSIZE 8192

typedef struct {
    int rio_fd;                // 읽을 대상 파일 디스크립터
    int rio_cnt;               // 남아 있는 읽기 버퍼의 바이트 수
    char *rio_bufptr;          // 다음으로 읽을 버퍼 위치
    char rio_buf[RIO_BUFSIZE]; // 내부 읽기 버퍼
} rio_t;

*/



void doit(int fd){
  int is_static, isHEAD = 0;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /*
    rio_readinitb(rio_t *rp, int fd):
    rio_t 구조체를 초기화한다.
    rio_t 구조체를 초기화하여 내부 버퍼 기반의 안전한 읽기 준비.
    rp: rio_t 구조체의 포인터 (읽기 버퍼를 위한 상태 저장용)
    fd: 읽을 대상 파일 디스크립터 (예: 클라이언트 소켓)
  */
  Rio_readinitb(&rio, fd);
  /*
    ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);
    fd 파일 디스크립터에서 한 줄을 읽어서 usrbuf에 저장한다.
    rp: 초기화된 rio_t 구조체 포인터
    usrbuf: 사용자가 읽은 데이터를 저장할 버퍼
    maxlen: 한 번에 읽을 최대 길이 (보통 MAXLINE)
    '\n' 문자까지 읽고, '\n'도 저장한다.
    리턴은 읽은 바이트수.
    반환값이 0이면 EOF
  */

  // 클라이언트가 보낸 요청 헤더가 저장돼있는 buf의 내용을 출력한다.
  if(!(Rio_readlineb(&rio, buf, MAXLINE))){
    return;
  }
  printf("Request headers:\n");
  printf("%s", buf);
  // buf 문자열을 공백을 기준으로 읽어서 method, uri, version에 저장
  // ex) "GET /index.html HTTP/1.1"
  sscanf(buf, "%s %s %s", method, uri, version);
  // 대소문자 구분없이 두 문자열을 비교한다. 같으면 0을 반환 다르면 양수나 음수를 반환
  if(strcasecmp(method, "GET") && strcasecmp(method, "HEAD")){
    // 404 Not Found 에러 페이지를 HTML로 만들어 클라이언트에게 반환한다.
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  // 클라이언트가 보낸 HTTP 요청 헤더를 읽어들임. 브라우저가 자동으로 보낸다.
  read_requesthdrs(&rio);


  if(!strcasecmp(method, "HEAD")){
    isHEAD = 1;
  }
  
  //is_static = parse_uri(uri, filename, cgiargs);
  // uri: 클라이언트가 요청한 uri, filename: 실제 서버 파일 시스템에서 접근할 경로,
  // cgiargs: CGI 스크립트 실행시 넘겨줄 인자 (예. ?x:1&y:2)
  // uri를 분석해서 요청이 정적 콘텐츠인지 동적 콘텐츠인지 구분함
  // filename 인자에 경로를 반환함.
  // 정적이면 1, 동적이면 0을 리턴한다.
  is_static = parse_uri(uri, filename, cgiargs);

  // stat(filename, &sbuf)
  // 지정한 파일의 상태 정보를 가져온다.
  // filename: 확인하고 싶은 파일 경로.
  // &sbuf: 파일의 상태 정보가 저장될 struct stat 타입의 포인터
  // 성공시 0, 실패시 -1
  /*
  struct stat {
    dev_t     st_dev;     // 파일이 있는 디바이스 ID
    ino_t     st_ino;     // inode 번호
    mode_t    st_mode;    // 파일 유형 및 권한
    nlink_t   st_nlink;   // 하드 링크 개수
    uid_t     st_uid;     // 소유자 사용자 ID
    gid_t     st_gid;     // 소유자 그룹 ID
    off_t     st_size;    // 파일 크기 (바이트 단위)
    time_t    st_mtime;   // 마지막 수정 시간
    ...
  };
  */
  if(stat(filename, &sbuf) < 0){
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }
  
  // 정적 콘텐츠인 경우
  if(is_static){
    /*
    S_ISREG함수는 stat으로 얻은 파일이 일반 파일인지 확인하는 데 쓰이는 매크로
    일반파일이면 1, 아니면 0
    S_IRUSR & sbuf.st_mode: 해당 파일이 소유자(user)에 의해 읽을 수 있는지 검사함
    권한이 없다면 0, 아니면 1
    */
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    // 클라이언트에게 정적 콘텐츠 파일을 보내주는 함수.
    serve_static(fd, filename, sbuf.st_size, isHEAD);
  }
  else{
    // 동적 컨텐츠인 경우
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs, method);

  }

}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
  char buf[MAXLINE], body[MAXLINE];

  // HTTP body를 만듬
  /*
    sprintf(char *str, const char *format, ...);
    str: 결과 문자열이 저장될 버퍼 (문자 배열)
    format: 출력 형식 문자열 (예: "%s %d")
    ...: 포맷 문자열에 맞는 인자들

    호출할 때마다 덮어씌우기 때문에 이어쓰고 싶다면 포인터를 글자수만큼 밀던가, %s를 이용하여 
    이어 써야한다.

    \r\n은 한 조합으로, 줄 바꿈을 의미한다.
  */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""fffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // HTTP응답을 출력한다
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);

  /*
  버퍼의 데이터를 지정된 파일 디스크립터에 buf의 크기만큼 쓴다.
  */
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf)); // 요청헤더와
  Rio_writen(fd, body, strlen(buf)); // 요청본체를 파일디스크립터에 쓴다.
}

void read_requesthdrs(rio_t *rp){
  char buf[MAXLINE];
  
  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")){
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs){
  char *ptr;
  /*
    char *strstr(const char *haystack, const char *needle);
    haystack: 전체 문자열 (예: uri)
    needle: 찾고자 하는 부분 문자열 (예: "cgi-bin")
    포함돼있다면 그 포인터를 반환
    그렇지 않다면 NULL을 반환
  */
  if(!strstr(uri, "cgi-bin")){
    // cgi-bin이 포함되어있지 않다면, 즉 정적 컨텐츠
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    // strcat은 두 문자를 이어붙임
    strcat(filename, uri);
    if(uri[strlen(uri) - 1] == '/'){
      strcat(filename, "home.html");
    }
    return 1;
  }
  else{
    // 동적 컨텐츠
    // 예) uri : /cgi-bin/adder?x=3&y=5
    // ?의 위치를 확인한다.
    ptr = index(uri, '?');

    if(ptr){
      // ?다음부터 \0 나올때까지 복사
      strcpy(cgiargs, ptr+1);
      // ?를 \0로 바꿔서 문자열의 끝이라고 명시
      *ptr = '\0';
    }
    // ?가 없는 경우에 cgi인자 초기화
    else strcpy(cgiargs, "");
    strcpy(filename, ".");
    // ?전까지 합치게됨
    strcat(filename, uri);
    return 0;
    
  }
}

void serve_static(int fd, char *filename, int filesize, int isHEAD){
  printf("server_static here\n");
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXLINE];
  rio_t rio;

  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);
  if(!isHEAD){
    srcfd = Open(filename, O_RDONLY, 0);
    // malloc으로 전달하기
    srcp = malloc(filesize);
    Rio_readinitb(&rio, srcfd);
    Rio_readn(srcfd, srcp, filesize);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    free(srcp);
    // 연 파일 내용을 가상메모리에 매핑함. 그 시작주소가 리턴됨
    //srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    //Rio_writen(fd, srcp, filesize);
    //Munmap(srcp, filesize);
  }

}


void serve_dynamic(int fd, char *filename, char *cgiargs, char* method){
  char buf[MAXLINE], *emptylist[] = {NULL};
  printf("serve dynamic here\n");
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
  
  if(Fork() == 0){
    // 자식 프로세스일 경우
    // QUERY_STRING 이라는 CGI 환경변수를 설정
    setenv("QUERY_STRING", cgiargs, 1);
    setenv("REQUEST_METHOD", method, 1);
    // dup2는 fd가 가리키고 있는 주소를 STDOUT_FILENO(1)식별자도 가리키게 만듬.
    // 즉 printf등의 표준출력이 화면에 표시되지 않고 fd에 적힘
    Dup2(fd, STDOUT_FILENO);
    // 현재 자식프로세스를 filename파일로 덮어씌움, 환경변수도 같이 전달함
    Execve(filename, emptylist, environ);
  }
  // 자식 프로세스가 종료할때까지 대기, NULL이면 종료코드를 확인하지 않고 종료했는지만 확인한다.
  //Wait(NULL);
  
}

void get_filetype(char *filename, char *filetype){
  if(strstr(filename, ".html")){
    strcpy(filetype, "text/html");
  }
  else if(strstr(filename, ".gif")){
    strcpy(filetype, "image/gif");
  }
  else if(strstr(filename, ".png")){
    strcpy(filetype, "image/png");
  }
  else if(strstr(filename, ".jpg")){
    strcpy(filetype, "image/jpeg");
  }
  else if(strstr(filename, ".mp4")){
    strcpy(filetype, "video/mp4");
  }
  else
    strcpy(filetype, "text/plain");
}