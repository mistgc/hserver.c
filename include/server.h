#ifndef _SERVER_H_
#define _SERVER_H_

#include "common.h"
#include <stdint.h>

struct HServerArgv {
  int epfd;
  int fd;
  pthread_t pthread_id;
};

int init_listen_fd(uint16_t port);

int epoll_run(int lfd);

int accept_client(int lfd, int epfd);
int recv_http_request(int cfd, int epfd);

void *callback_accpet_client(void *argv);
void *callback_recv_http_request(void *argv);

int parse_request_line(const char *line, int cfd);

int send_header_msg(int cfd, int status, const char *descr, const char *type,
                    int length);
int send_file(const char *path, int cfd);
int send_dir(const char *path, int cfd);

const char *get_file_type(const char *name);
int hex_to_dec(char c);
void url_decode(const char *from, char *to);

#endif // _SERVER_H_
