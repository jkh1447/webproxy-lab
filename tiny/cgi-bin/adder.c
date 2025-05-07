/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;
  char *method;
  method = getenv("REQUEST_METHOD");
  if((buf = getenv("QUERY_STRING")) != NULL){
    // char *strchr(const char *str, int c);
    // 문자 c 가 처음 등장하는 포인터를 반환
    p = strchr(buf, '&');
    *p = '\0';
    buf = strchr(buf, '=');
    strcpy(arg1, buf+1);
    p = strchr(p+1, '=');
    strcpy(arg2, p+1);
    // atoi: 문자열을 정수로 변환
    n1 = atoi(arg1);
    n2 = atoi(arg2);
  }

  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sThe Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  if(strcasecmp(method, "HEAD") != 0){
    printf("%s", content);
    fflush(stdout);

  }
  
  exit(0);
}
/* $end adder */
