#include "common.h"
// Cross-platform thread include
#ifndef _WIN32
#include <ifaddrs.h>
#include <netdb.h>
#include <pthread.h>

#endif

// Global State
typedef struct {
  int sockfd;
  struct sockaddr_in address;
  struct sockaddr_in udp_addr; // UDP Address for media
  int has_udp;                 // Flag if UDP is registered
  char username[NAME_LEN];
  int is_logged_in;
} Client;

Client *clients[MAX_CLIENTS];
UserCred valid_users[100];
int user_count = 0;

// Windows Threading Wrapper
#ifdef _WIN32
HANDLE clients_mutex;
#define LOCK_MUTEX WaitForSingleObject(clients_mutex, INFINITE)
#define UNLOCK_MUTEX ReleaseMutex(clients_mutex)
#else
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_MUTEX pthread_mutex_lock(&clients_mutex)
#define UNLOCK_MUTEX pthread_mutex_unlock(&clients_mutex)
#endif

// --- Helper Functions ---

void log_event(const char *msg) {
  FILE *fp = fopen("server_log.txt", "a");
  if (fp) {
    time_t now = time(NULL);
    char *t = ctime(&now);
    t[strlen(t) - 1] = '\0';
    fprintf(fp, "[%s] %s\n", t, msg);
    printf("[%s] %s\n", t, msg);
    fclose(fp);
  }
}

void load_users() {
  FILE *fp = fopen("users.txt", "r");
  if (!fp) {
    // Create default if not exists
    fp = fopen("users.txt", "w");
    fprintf(fp, "admin admin123\nuser1 pass1\nuser2 pass2\n");
    fclose(fp);
    fp = fopen("users.txt", "r");
  }

  user_count = 0;
  while (fscanf(fp, "%31s %31s", valid_users[user_count].username,
                valid_users[user_count].password) != EOF) {
    user_count++;
  }
  fclose(fp);
  char buf[100];
  snprintf(buf, sizeof(buf), "Loaded %d users.", user_count);
  log_event(buf);
}

int authenticate(const char *user, const char *pass) {
  for (int i = 0; i < user_count; i++) {
    if (strcmp(valid_users[i].username, user) == 0 &&
        strcmp(valid_users[i].password, pass) == 0) {
      return 1;
    }
  }
  return 0;
}

void send_msg(int sockfd, Message *msg) {
  send(sockfd, (char *)msg, sizeof(Message), 0);
}

// --- Message Routing ---

void broadcast(Message *msg, int sender_fd) {
  LOCK_MUTEX;
  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (clients[i] && clients[i]->is_logged_in &&
        clients[i]->sockfd != sender_fd) {
      send_msg(clients[i]->sockfd, msg);
    }
  }
  UNLOCK_MUTEX;
}

void send_private(Message *msg, int sender_fd) {
  int found = 0;
  LOCK_MUTEX;
  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (clients[i] && clients[i]->is_logged_in &&
        strcmp(clients[i]->username, msg->target) == 0) {
      send_msg(clients[i]->sockfd, msg);
      found = 1;
      break;
    }
  }
  UNLOCK_MUTEX;

  if (!found) {
    Message err;
    err.type = MSG_TEXT;
    strcpy(err.source, "Server");
    snprintf(err.data, BUFFER_SIZE, "User '%s' not found or offline.",
             msg->target);
    send_msg(sender_fd, &err);
  }
}

// --- Client Handling ---

#ifdef _WIN32
DWORD WINAPI handle_client(LPVOID arg)
#else
void *handle_client(void *arg)
#endif
{
  Client *cli = (Client *)arg;
  Message msg;
  int bytes;

  while ((bytes = recv(cli->sockfd, (char *)&msg, sizeof(Message), 0)) > 0) {
    if (msg.type == MSG_LOGIN) {
      if (authenticate(msg.source, msg.data)) {
        // Check if already logged in
        int already_in = 0;
        LOCK_MUTEX;
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (clients[i] && clients[i] != cli &&
              strcmp(clients[i]->username, msg.source) == 0) {
            already_in = 1;
            break;
          }
        }
        UNLOCK_MUTEX;

        if (already_in) {
          msg.type = MSG_LOGIN_FAIL;
          strcpy(msg.data, "User already logged in.");
          send_msg(cli->sockfd, &msg);
        } else {
          strcpy(cli->username, msg.source);
          cli->is_logged_in = 1;
          msg.type = MSG_LOGIN_SUCCESS;
          strcpy(msg.data, "Welcome to the Chat!");
          send_msg(cli->sockfd, &msg);

          char log[100];
          snprintf(log, sizeof(log), "%s logged in.", cli->username);
          log_event(log);
        }
      } else {
        msg.type = MSG_LOGIN_FAIL;
        strcpy(msg.data, "Invalid username or password.");
        send_msg(cli->sockfd, &msg);
      }
    } else if (cli->is_logged_in) {
      if (msg.type == MSG_TEXT) {
        // Broadcast
        char log[BUFFER_SIZE + 50];
        snprintf(log, sizeof(log), "Broadcast from %s: %s", cli->username,
                 msg.data);
        log_event(log);
        broadcast(&msg, cli->sockfd);
      } else if (msg.type == MSG_PRIVATE || msg.type == MSG_FILE_OFFER ||
                 msg.type == MSG_FILE_ACCEPT || msg.type == MSG_FILE_DATA ||
                 msg.type == MSG_VOICE_REQ || msg.type == MSG_VOICE_ACCEPT ||
                 msg.type == MSG_VOICE_END) {
        // Route Private
        if (msg.type == MSG_PRIVATE) {
          char log[BUFFER_SIZE + 50];
          snprintf(log, sizeof(log), "Private from %s to %s: %s", cli->username,
                   msg.target, msg.data);
          log_event(log);
        }
        send_private(&msg, cli->sockfd);
      } else if (msg.type == MSG_LIST_REQ) {
        Message res;
        res.type = MSG_LIST_RES;
        strcpy(res.source, "Server");
        strcpy(res.data, "Active Users:\n");

        LOCK_MUTEX;
        for (int i = 0; i < MAX_CLIENTS; ++i) {
          if (clients[i] && clients[i]->is_logged_in) {
            strcat(res.data, "- ");
            strcat(res.data, clients[i]->username);
            strcat(res.data, "\n");
          }
        }
        UNLOCK_MUTEX;
        send_msg(cli->sockfd, &res);
      } else if (msg.type == MSG_EXIT) {
        break;
      }
    }
  }

  CLOSESOCKET(cli->sockfd);

  LOCK_MUTEX;
  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (clients[i] == cli) {
      clients[i] = NULL;
      break;
    }
  }
  UNLOCK_MUTEX;

  if (cli->is_logged_in) {
    char log[100];
    snprintf(log, sizeof(log), "%s disconnected.", cli->username);
    log_event(log);
  }

  free(cli);
  return 0;
}

// --- UDP Media Relay ---
#ifdef _WIN32
DWORD WINAPI handle_udp(LPVOID arg)
#else
void *handle_udp(void *arg)
#endif
{
  int udp_sock;
  struct sockaddr_in server_addr, cli_addr;
  int addr_len = sizeof(cli_addr);
  MediaPacket packet;

  if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("UDP Socket creation failed");
    return 0;
  }

  // Allow reuse
  int opt = 1;
  setsockopt(udp_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt,
             sizeof(opt));

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(UDP_PORT);

  if (bind(udp_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("UDP Bind failed");
    return 0;
  }

  printf("UDP Media Server listening on port %d\n", UDP_PORT);

  while (1) {
    int len = recvfrom(udp_sock, (char *)&packet, sizeof(MediaPacket), 0,
                       (struct sockaddr *)&cli_addr, (socklen_t *)&addr_len);
    if (len > 0) {
      if (packet.type == MEDIA_REGISTER) {
        // Register User's UDP Address
        LOCK_MUTEX;
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (clients[i] && strcmp(clients[i]->username, packet.source) == 0) {
            clients[i]->udp_addr = cli_addr;
            clients[i]->has_udp = 1;
            printf("Registered UDP for user %s\n", packet.source);
            break;
          }
        }
        UNLOCK_MUTEX;
      } else {
        // Relay Packet
        LOCK_MUTEX;
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (clients[i] && clients[i]->has_udp &&
              strcmp(clients[i]->username, packet.target) == 0) {
            sendto(udp_sock, (char *)&packet, len, 0,
                   (struct sockaddr *)&clients[i]->udp_addr,
                   sizeof(clients[i]->udp_addr));
            break;
          }
        }
        UNLOCK_MUTEX;
      }
    }
  }
  return 0;
}

void print_ip_addresses() {
  printf("----------------------------------------\n");
  printf("Server IP Addresses:\n");
#ifdef _WIN32
  // Windows implementation omitted for brevity, but easy to add if needed
  // Simple and effective for Windows
  system("ipconfig | findstr IPv4");
  printf("  (Run 'ipconfig' for more details)\n");
#else
  struct ifaddrs *ifaddr, *ifa;
  if (getifaddrs(&ifaddr) == -1) {
    perror("getifaddrs");
    return;
  }

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL)
      continue;
    if (ifa->ifa_addr->sa_family == AF_INET) { // IPv4
      char host[NI_MAXHOST];
      if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host,
                      NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0) {
        if (strcmp(host, "127.0.0.1") != 0) {
          printf("  %s: %s\n", ifa->ifa_name, host);
        }
      }
    }
  }
  freeifaddrs(ifaddr);
#endif
  printf("----------------------------------------\n");
}

int main() {

#ifdef _WIN32
  WSADATA wsa;
  WSAStartup(MAKEWORD(2, 2), &wsa);
  clients_mutex = CreateMutex(NULL, FALSE, NULL);
  CreateThread(NULL, 0, handle_udp, NULL, 0, NULL);
#endif

  load_users();

  int server_fd, new_socket;
  struct sockaddr_in address;
  int addrlen = sizeof(address);

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 10) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  log_event("Server started on port 9999");
  print_ip_addresses();

#ifndef _WIN32
  pthread_t udp_tid;
  pthread_create(&udp_tid, NULL, handle_udp, NULL);
  pthread_detach(udp_tid);
#endif

  while (1) {
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                             (socklen_t *)&addrlen)) < 0) {
      perror("accept");
      continue;
    }

    Client *cli = (Client *)malloc(sizeof(Client));
    cli->sockfd = new_socket;
    cli->address = address;
    cli->is_logged_in = 0;

    LOCK_MUTEX;
    int added = 0;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
      if (!clients[i]) {
        clients[i] = cli;
        added = 1;
        break;
      }
    }
    UNLOCK_MUTEX;

    if (!added) {
      printf("Max clients reached. Connection rejected.\n");
      CLOSESOCKET(new_socket);
      free(cli);
    } else {
#ifdef _WIN32
      CreateThread(NULL, 0, handle_client, (LPVOID)cli, 0, NULL);
#else
      pthread_t tid;
      pthread_create(&tid, NULL, handle_client, (void *)cli);
      pthread_detach(tid);
#endif
    }
  }

  return 0;
}
