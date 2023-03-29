#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

#include "server.h"

int init_listen_fd(uint16_t port) {
  int lfd, ret, opt = 1;
  if ((lfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
    perror("socket");
    return -1;
  }

  if ((ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt)) ==
      -1) {
    perror("setsockopt");
    return -1;
  }

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if ((ret = bind(lfd, (struct sockaddr *)&addr, sizeof addr)) == -1) {
    perror("bind");
    return -1;
  }

  if ((ret = listen(lfd, 128)) == -1) {
    perror("listen");
    return -1;
  }

  return lfd;
}

int epoll_run(int lfd) {
  struct epoll_event ev;
  struct epoll_event evs[1024];
  int epfd, ret, evs_size = sizeof(evs) / sizeof(evs[0]);

  if ((epfd = epoll_create(1)) == -1) {
    perror("epoll_create");
    return -1;
  }

  ev.data.fd = lfd;
  ev.events = EPOLLIN;
  if ((ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev)) == -1) {
    perror("epoll_ctl");
    return -1;
  }

  while (1) {
    int num = epoll_wait(epfd, evs, evs_size, -1);
    for (int i = 0; i < num; i++) {
      int fd = evs[i].data.fd;
      if (fd == lfd) {
        // Establish A New Connection
        accept_client(lfd, epfd);
      } else {
        // Reveive Data
        recv_http_request(fd, epfd);
      }
    }
  }

  return 0;
}

int accept_client(int lfd, int epfd) {
  int cfd, flag, ret;
  struct epoll_event ev;

  // establish
  if ((cfd = accept(lfd, NULL, NULL)) == -1) {
    perror("accept");
    return -1;
  }

  // set nonblock
  flag = fcntl(cfd, F_GETFL);
  flag |= O_NONBLOCK;
  fcntl(cfd, F_SETFL, flag);

  // make epoll to manage cfd
  ev.data.fd = cfd;
  ev.events = EPOLLIN | EPOLLET;
  if ((ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev)) == -1) {
    perror("epoll_ctl");
    return -1;
  }

  return 0;
}

int recv_http_request(int cfd, int epfd) {
  int len = 0, total = 0;
  char tmp[1024] = {0};
  char buf[4096] = {0};
  while ((len = recv(cfd, tmp, sizeof tmp, 0)) > 0) {
    if (total + len < sizeof buf) {
      memcpy(buf + total, tmp, len);
    }
    total += len;
  }
  if (len == -1 && errno == EAGAIN) {
    // parse request line
    char *pt = strstr(buf, "\r\n");
    *pt = '\0';
    parse_request_line(buf, cfd);
  } else if (len == 0) {
    // request finished and remove
    epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
    close(cfd);
  } else {
    perror("recv");
  }

  return 0;
}

int parse_request_line(const char *line, int cfd) {
  int ret;
  char method[12];
  char path[1024];

  // request line: get /xxx/1.jpg http/1.1
  sscanf(line, "%[^ ] %[^ ]", method, path);
  url_decode(path, path);
  fprintf(stdout, "method: %s path: %s\n", method, path);
  if (strcasecmp(method, "get") != 0) {
    return -1;
  }
  // handle static resouce requested by client
  char *file = NULL;
  if (strcmp(path, "/") == 0) {
    file = "./";
  } else {
    file = path + 1;
  }
  // get file state
  struct stat st;
  if ((ret = stat(file, &st)) == -1) {
    // the file doesn't exist -- return 404
    send_header_msg(cfd, 404, "Not Found", get_file_type(".html"), -1);
    send_file("404.html", cfd);
  }
  if (S_ISDIR(st.st_mode)) {
    send_header_msg(cfd, 200, "OK", get_file_type(".html"), -1);
    send_dir(file, cfd);
  } else {
    send_header_msg(cfd, 200, "OK", get_file_type(file), st.st_size);
    send_file(file, cfd);
  }

  return 0;
}

int send_file(const char *path, int cfd) {
  int fd = open(path, O_RDONLY), size;
  off_t offset = 0;

  assert(fd > 0);

  size = lseek(fd, 0, SEEK_END);
  lseek(fd, 0, SEEK_SET);

  while (offset < size) {
    sendfile(cfd, fd, &offset, size);
  }

  close(fd);
  return 0;
}

int send_header_msg(int cfd, int status, const char *descr, const char *type,
                    int length) {
  char buf[4096] = {0};
  // Status Line
  sprintf(buf, "HTTP/1.1 %d %s\r\n", status, descr);
  // Response Header
  sprintf(buf + strlen(buf), "Content-Type: %s\r\n", type);
  sprintf(buf + strlen(buf), "Content-Length: %d\r\n\r\n", length);

  send(cfd, buf, strlen(buf), 0);

  return 0;
}

const char *get_file_type(const char *name) {
  const char *dot = strrchr(name, '.');
  if (dot == NULL)
    return "text/plain; charset=utf-8";
  if (!strcmp(dot, ".html") || !strcmp(dot, ".htm"))
    return "text/html; charset=utf-8";
  if (!strcmp(dot, ".jpg") || !strcmp(dot, ".jpeg"))
    return "image/jpeg";
  if (!strcmp(dot, ".png"))
    return "iamge/png";
  if (!strcmp(dot, ".css"))
    return "text/css";
  if (!strcmp(dot, ".pdf"))
    return "application/pdf";
  return "text/plain; charset=utf-8";
}

int send_dir(const char *path, int cfd) {
  char buf[4096] = {0};
  sprintf(buf, "<html><head><title>%s</title></head><body><table>", path);
  struct dirent **namelist;
  int num = scandir(path, &namelist, NULL, alphasort);
  for (int i = 0; i < num; i++) {
    char *name = namelist[i]->d_name;
    struct stat st;
    char sub_path[1024] = {0};
    sprintf(sub_path, "%s/%s", path, name);
    stat(sub_path, &st);
    if (S_ISDIR(st.st_mode)) {
      sprintf(buf + strlen(buf),
              "<tr><td><a href=\"%s/\">%s</a></td><td>%.1fK</td></tr>", name,
              name, (double)st.st_size / 1024.0f);
    } else {
      sprintf(buf + strlen(buf),
              "<tr><td><a href=\"%s\">%s</a></td><td>%.1fK</td></tr>", name,
              name, (double)st.st_size / 1024.0f);
    }
    send(cfd, buf, strlen(buf), 0);
    memset(buf, 0, sizeof(buf));
    free(namelist[i]);
  }
  sprintf(buf, "</table></body></html>");
  send(cfd, buf, strlen(buf), 0);
  free(namelist);
  return 0;
}

int hex_to_dec(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return 0;
}

void url_decode(const char *from, char *to) {
  for (; *from != '\0'; to++, from++) {
    if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {
      *to = hex_to_dec(from[1]) * 16 + hex_to_dec(from[2]);
      from += 2;
    } else {
      *to = *from;
    }
  }
  *to = '\0';
}
