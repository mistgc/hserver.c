#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc < 3) {
    printf("Usage: %s <port> <dir_path>\n", argv[0]);
    return -1;
  }
  uint16_t port = atoi(argv[1]);
  // change workspace
  chdir(argv[2]);
  // Initialize Socket for Listening
  int lfd = init_listen_fd(port);
  // Run Application
  epoll_run(lfd);
  return 0;
}
