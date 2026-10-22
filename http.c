#include <asm-generic/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


void sendHTML(int clientfd, char *req) {

  FILE *res_html;

  if (strcmp(req, "GET") == 0) {
    res_html = fopen("index.html", "r");
  } else if (strcmp(req, "POST") == 0) {
    res_html = fopen("servePost.html", "r");
  }

  // sending html as a response in chunks
  char buff[1024];
  size_t bytes_read;

  while ((bytes_read = fread(buff, 1, sizeof buff, res_html)) > 0) {
    size_t total_sent = 0;
    while (total_sent < bytes_read) {
      size_t n = send(clientfd, buff + total_sent, bytes_read - total_sent, 0);
      total_sent += (size_t)n;
    }
  }
  fclose(res_html);
}

int main() {

  int sockfd = socket(AF_INET, SOCK_STREAM, 0); // init the socket file descriptor
  if (sockfd < 0) {
    perror("Lol, lost the sock\n");
    exit(1);
  }

  int yes = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes); // socket re-use issue fix

  struct addrinfo hints = {0}, *res; // res is the pointer to the linked list returned by getaddrinfo()
  // sockaddr_storage; struct big enough to accomodate either v4 or 6, depending upon which one is returned
  struct sockaddr_storage their_addr; // points to the socket address, not necessary here coz im only using IPv4, alternatively caan use sockaddr_in

  hints.ai_family = AF_INET; // IPv4
  hints.ai_flags = AI_PASSIVE;
  hints.ai_socktype = SOCK_STREAM;

  int getaddr_res = getaddrinfo(NULL, "8080", &hints, &res);
  printf("getaddrinfo res: %d\n", getaddr_res);

  int bind_res = bind(sockfd, res->ai_addr, res->ai_addrlen); // bind socket to local ip
  if (bind_res < 0) {
    perror("Bind failed\n");
    exit(1);
  }
  printf("Bind Successfull\n");

  int l_res = listen(sockfd, 0); // big eaars
  if (l_res == -1) {
    perror("listen");
    close(sockfd);
    exit(1);
  }

  printf("Listening...\n");
  socklen_t addr_size = sizeof their_addr;

  while (1) {
    int new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
    int max_len = 1000;
    char recv_req[max_len];
    memset(recv_req, 0, max_len); // 0 all the bytes, reusable

    int num_bytes = recv(new_fd, &recv_req, max_len, 0);
    printf("bytes: %d\n", num_bytes);
    int i = 0;
    while (recv_req[i] != '\0') {
      printf("%c", recv_req[i]);
      i++;
    }

    if (strncmp(recv_req, "GET", 3) == 0) {
      printf("Received HTTP GET request\n");

      const char *getheaders = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: text/html\r\n"
                               "Connection: close\r\n"
                               "\r\n";

      send(new_fd, getheaders, strlen(getheaders), 0);

      sendHTML(new_fd, "GET");
      close(new_fd);
    }

    else if (strncmp(recv_req, "POST /blog", 10) == 0) {

      printf("POST: \n");
      FILE *json = fopen("data.json", "w");

      if (json == NULL) {
        perror("Couldn't open file: ");
        exit(1);
      }

      char *body = strstr(recv_req, "\r\n\r\n");
      if (body == NULL) {
        body += 4;
      }

      fprintf(json, "{\n");

      while (*body == '\r' || *body == '\n' || *body == ' ' || *body == '\t') {
        body++;
      }

      char *saveptr;
      char *pair = strtok_r(body, "&", &saveptr);
      int first = 1;

      while (pair != NULL) {
        char *eq = strchr(pair, '=');

        if (eq != NULL) {
          *eq = '\0';

          char *key = pair;
          char *value = eq + 1;
          char *plus = value;

          while ((plus = strchr(plus, '+')) != NULL) {
            *plus = ' ';
            plus++;
          }

          if (strchr(key, '+') != NULL) {

            char *plus_k = key;
            while ((plus_k = strchr(plus_k, '+')) != NULL) {
              *plus_k = ' ';
              plus_k++;
            }
          }
          if (!first) {
            fprintf(json, ",\n");
          }
          fprintf(json, "  \"%s\": \"%s\"", key, value);
          first = 0;
        }
        pair = strtok_r(NULL, "&", &saveptr);
      }
      fprintf(json, "\n}");

      fclose(json);

      const char *postheaders = "HTTP/1.1 201 Created\r\n"
                                "Content-Type: text/html\r\n"
                                "Connection: close\r\n"
                                "\r\n";

      send(new_fd, postheaders, strlen(postheaders), 0);
      sendHTML(new_fd, "POST");
      close(new_fd);
    }
  }

  close(sockfd);
  return 0;
}
