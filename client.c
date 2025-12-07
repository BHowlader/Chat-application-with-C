#include "common.h"

int sockfd;
char my_username[NAME_LEN];
int logged_in = 0;

// State for incoming requests
char incoming_file_user[NAME_LEN] = "";
char incoming_file_name[256] = "";
char incoming_call_user[NAME_LEN] = "";
int in_call = 0;

// UDP / Audio State
int udp_sockfd;
struct sockaddr_in udp_serv_addr;
volatile int call_active = 0;
Thread audio_send_thread, video_send_thread, udp_recv_thread;

// --- Audio Functions ---

THREAD_FUNC send_audio_handler(void *arg) {
  char buffer[MEDIA_CHUNK_SIZE];
  // Try to open microphone (SoX rec)
  // 8kHz, 16bit, Mono
#ifdef _WIN32
  // Windows: SoX often installed as 'sox'. 'rec' is alias. 2>NUL for silence.
  FILE *mic = popen(
      "sox -t waveaudio default -t raw -r 8000 -e signed -b 16 -c 1 -q - 2>NUL",
      "r");
#else
  FILE *mic =
      popen("rec -t raw -r 8000 -e signed -b 16 -c 1 -q - 2>/dev/null", "r");
#endif
  if (!mic) {
    printf("[Audio] 'rec' command not found or failed. Simulating audio...\n");
  }

  MediaPacket pkt;
  pkt.type = MEDIA_AUDIO;
  strcpy(pkt.source, my_username);
  // target is set globally or passed? We need target.
  // Let's use incoming_call_user as target (hacky but works for 1:1)
  strcpy(pkt.target, incoming_call_user);

  while (call_active) {
    int len;
    if (mic) {
      len = fread(pkt.data, 1, MEDIA_CHUNK_SIZE, mic);
    } else {
      // Simulate silence/noise
      len = MEDIA_CHUNK_SIZE;
      memset(pkt.data, 0, len);
      SLEEP_MS(100); // 100ms
    }

    if (len > 0) {
      pkt.type = MEDIA_AUDIO; // Reset type just in case
      pkt.data_len = len;
    }
  }
  if (mic)
    pclose(mic);
  return THREAD_RET;
}

THREAD_FUNC send_video_handler(void *arg) {
  // Capture Webcam and pipe to stdout
  // macOS: avfoundation, Linux: v4l2, Win: dshow. Assuming macOS based on user
  // env. Low res (320x240), low bitrate (200k), MPEG1Video for speed/resilience
#ifdef _WIN32

  // Windows: DirectShow. YOU MUST REPLACE "Integrated Camera" with your
  // actual camera name! Run 'ffmpeg -list_devices true -f dshow -i dummy'
  // to find it.
  const char *cmd = "ffmpeg -f dshow -framerate 25 -video_size 320x240 -i "
                    "video=\"Integrated "
                    "Camera\" -c:v mpeg1video -b:v 200k -f mpegts - 2>NUL";
#else
  const char *cmd = "ffmpeg -f avfoundation -framerate 25 -video_size "
                    "320x240 -i \"default\" "
                    "-c:v mpeg1video -b:v 200k -f mpegts - 2>/dev/null";
#endif

  FILE *cam = popen(cmd, "r");
  if (!cam) {
    printf("[Video] 'ffmpeg' command not found. Video will be black.\n");
  }

  MediaPacket pkt;
  pkt.type = MEDIA_VIDEO;
  strcpy(pkt.source, my_username);
  strcpy(pkt.target, incoming_call_user);

  while (call_active) {
    int len;
    if (cam) {
      len = fread(pkt.data, 1, MEDIA_CHUNK_SIZE, cam);
    } else {
      SLEEP_MS(100);
      len = 0;
    }

    if (len > 0) {
      pkt.type = MEDIA_VIDEO;
      pkt.data_len = len;
      sendto(udp_sockfd, (char *)&pkt, sizeof(MediaPacket), 0,
             (struct sockaddr *)&udp_serv_addr, sizeof(udp_serv_addr));
    }
  }

  if (cam)
    pclose(cam);
  return THREAD_RET;
}

THREAD_FUNC udp_receive_handler(void *arg) {
  MediaPacket pkt;
  struct sockaddr_in from;
  int len = sizeof(from);

#ifdef _WIN32
  FILE *spk = popen("sox -t raw -r 8000 -e signed -b 16 -c 1 -q - -t "
                    "waveaudio default 2>NUL",
                    "w");
  FILE *vid_out =
      popen("ffplay -f mpegts -framerate 25 -fflags nobuffer -window_title "
            "\"Incoming Call\" -x 320 -y 240 -autoexit - 2>NUL",
            "w");
#else
  FILE *spk =
      popen("play -t raw -r 8000 -e signed -b 16 -c 1 -q - 2>/dev/null", "w");
  FILE *vid_out =
      popen("ffplay -f mpegts -framerate 25 -fflags nobuffer -window_title "
            "'Incoming Call' -x 320 -y 240 -autoexit - 2>/dev/null",
            "w");
#endif

  if (!spk)
    printf("[Audio] 'play' tool not found.\n");
  if (!vid_out)
    printf("[Video] 'ffplay' tool not found.\n");

  while (call_active) {
    int n = recvfrom(udp_sockfd, (char *)&pkt, sizeof(MediaPacket), 0,
                     (struct sockaddr *)&from, (socklen_t *)&len);
    if (n > 0) {
      if (pkt.type == MEDIA_AUDIO && spk) {
        fwrite(pkt.data, 1, pkt.data_len, spk);
      } else if (pkt.type == MEDIA_VIDEO && vid_out) {
        fwrite(pkt.data, 1, pkt.data_len, vid_out);
      }
    }
  }
  if (spk)
    pclose(spk);
  if (vid_out)
    if (spk)
      pclose(spk);
  if (vid_out)
    pclose(vid_out);
  return THREAD_RET;
}

void start_call_threads() {
  if (call_active)
    return;
  call_active = 1;

#ifdef _WIN32
  audio_send_thread = CreateThread(NULL, 0, send_audio_handler, NULL, 0, NULL);
  video_send_thread = CreateThread(NULL, 0, send_video_handler, NULL, 0, NULL);
  udp_recv_thread = CreateThread(NULL, 0, udp_receive_handler, NULL, 0, NULL);
#else
  pthread_create(&audio_send_thread, NULL, send_audio_handler, NULL);
  pthread_create(&video_send_thread, NULL, send_video_handler, NULL);
  pthread_create(&udp_recv_thread, NULL, udp_receive_handler, NULL);
#endif
}

void stop_call_threads() {
  call_active = 0;
  // Threads will exit naturally
}

// Mutex for socket writing (to prevent interleaving)
#ifdef _WIN32
HANDLE sock_mutex;
#define LOCK_SOCK WaitForSingleObject(sock_mutex, INFINITE)
#define UNLOCK_SOCK ReleaseMutex(sock_mutex)
#else
pthread_mutex_t sock_mutex = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_SOCK pthread_mutex_lock(&sock_mutex)
#define UNLOCK_SOCK pthread_mutex_unlock(&sock_mutex)
#endif

void send_msg_safe(Message *msg) {
  LOCK_SOCK;
  send(sockfd, (char *)msg, sizeof(Message), 0);
  UNLOCK_SOCK;
}

// --- File Transfer ---

void send_file(char *target, char *filepath) {
  FILE *fp = fopen(filepath, "rb");
  if (!fp) {
    printf("Error: File '%s' not found.\n", filepath);
    return;
  }

  printf("Sending file '%s' to %s...\n", filepath, target);
  Message msg;
  msg.type = MSG_FILE_DATA;
  strcpy(msg.source, my_username);
  strcpy(msg.target, target);

  while ((msg.data_len = fread(msg.data, 1, BUFFER_SIZE, fp)) > 0) {
    send_msg_safe(&msg);
    SLEEP_MS(5); // Throttle slightly
  }
  fclose(fp);
  printf("File sent successfully.\n");
}

// --- Receive Thread ---

// This thread runs in the background to handle incoming messages without
// blocking the UI.
THREAD_FUNC receive_handler(void *arg) {
  Message msg;
  int bytes;

  while ((bytes = recv(sockfd, (char *)&msg, sizeof(Message), 0)) > 0) {
    switch (msg.type) {
    case MSG_LOGIN_SUCCESS:
      printf("[Server] %s\n", msg.data);
      logged_in = 1;
      break;
    case MSG_LOGIN_FAIL:
      printf("[Server] Error: %s\n", msg.data);
      logged_in = -1;
      break;
    case MSG_TEXT: // Broadcast
      printf("[Broadcast] %s: %s\n", msg.source, msg.data);
      break;
    case MSG_PRIVATE:
      printf("[Private] %s: %s\n", msg.source, msg.data);
      break;
    case MSG_LIST_RES:
      printf("%s\n", msg.data);
      break;

    case MSG_FILE_OFFER:
      printf("\n[FILE] %s wants to send you a file: %s\n", msg.source,
             msg.data);
      printf("Type '/accept file' to receive it.\n");
      strcpy(incoming_file_user, msg.source);
      strcpy(incoming_file_name, msg.data);
      break;
    case MSG_FILE_ACCEPT:
      printf("[FILE] %s accepted your file offer. Sending...\n", msg.source);
      // We need to trigger send_file.
      // We will just print "User accepted". The user then has to type
      // "/send <user> <file>" to actually send? No, that's bad UX. Let's
      // send immediately. Sender calls send_file(). To do this, we need to
      // store the pending file path in a global. See 'pending_file_path'
      // below.
      extern char pending_file_path[256];
      extern char pending_file_target[NAME_LEN];
      if (strcmp(msg.source, pending_file_target) == 0) {
        send_file(pending_file_target, pending_file_path);
      }
      break;
    case MSG_FILE_DATA: {
      printf("Receiving file data from %s...\n", msg.source);
      // Append to file
      char fname[300];
      snprintf(fname, sizeof(fname), "received_%s",
               incoming_file_name[0] ? incoming_file_name : "file.dat");
      FILE *fp = fopen(fname, "ab");
      if (fp) {
        fwrite(msg.data, 1, msg.data_len, fp);
        fclose(fp);
      }
    } break;

    case MSG_VOICE_REQ:
      printf("\n[CALL] %s is calling you (Voice/Video)...\n", msg.source);
      printf("Type '/accept call' to answer or '/hangup' to reject.\n");
      strcpy(incoming_call_user, msg.source);
      break;
    case MSG_VOICE_ACCEPT:
      printf("\n[CALL] %s accepted your call!\n", msg.source);
      printf("Voice/Video connection established. (Audio + Video)\n");
      printf("Type '/hangup' to end call.\n");
      in_call = 1;
      strcpy(incoming_call_user, msg.source); // Ensure target is set
      start_call_threads();
      break;
    case MSG_VOICE_END:
      printf("\n[CALL] Call ended by %s.\n", msg.source);
      in_call = 0;
      stop_call_threads();
      incoming_call_user[0] = '\0';
      break;
    default:
      break;
    }
  }
  return 0;
}

// Global storage for pending actions
char pending_file_path[256] = "";
char pending_file_target[NAME_LEN] = "";

int main(int argc, char *argv[]) {
#ifdef _WIN32
  WSADATA wsa;
  WSAStartup(MAKEWORD(2, 2), &wsa);
  sock_mutex = CreateMutex(NULL, FALSE, NULL);
#endif

  struct sockaddr_in serv_addr;
  char buffer[BUFFER_SIZE];

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("Socket creation error\n");
    return 1;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);
  // Default to localhost if no arg
  char *server_ip = "127.0.0.1";
  if (argc > 1)
    server_ip = argv[1];

  if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
    printf("Invalid address\n");
    return 1;
  }

  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    printf("Connection Failed. Is the server running?\n");
    return 1;
  }

  // --- UDP Setup ---
  if ((udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("UDP Socket creation failed");
  }
  udp_serv_addr.sin_family = AF_INET;
  udp_serv_addr.sin_port = htons(UDP_PORT);
  if (inet_pton(AF_INET, server_ip, &udp_serv_addr.sin_addr) <= 0) {
    printf("Invalid UDP address\n");
  }

  // Prompts user for credentials and sends MSG_LOGIN packet securely.
  printf("--- Chat Login ---\n");
  printf("Username: ");
  scanf("%s", my_username);
  char password[PASS_LEN];
  printf("Password: ");
  scanf("%s", password);

  Message msg;
  msg.type = MSG_LOGIN;
  strcpy(msg.source, my_username);
  strcpy(msg.data, password);
  strcpy(msg.data,
         password); // Note: Keep existing redundancy if intentional, or
                    // cleanup
  send_msg_safe(&msg);

  // Send UDP Registration
  MediaPacket reg_pkt;
  reg_pkt.type = MEDIA_REGISTER;
  strcpy(reg_pkt.source, my_username);
  sendto(udp_sockfd, (char *)&reg_pkt, sizeof(MediaPacket), 0,
         (struct sockaddr *)&udp_serv_addr, sizeof(udp_serv_addr));

// Start receive thread
#ifdef _WIN32
  CreateThread(NULL, 0, receive_handler, NULL, 0, NULL);
#else
  pthread_t tid;
  pthread_create(&tid, NULL, receive_handler, NULL);
#endif

  // Wait for login
  while (logged_in == 0)
    SLEEP_MS(10);

  if (logged_in == -1) {
    printf("Login failed. Exiting.\n");
    return 1;
  }

  // Clear stdin
  while ((getchar()) != '\n')
    ;

  system(CLEAR_SCREEN);
  printf("--- Welcome %s ---\n", my_username);
  printf("Commands:\n");
  printf("  /msg <user> <text>    - Private message\n");
  printf("  /broadcast <text>     - Broadcast message\n");
  printf("  /list                 - List active users\n");
  printf("  /file <user> <path>   - Send file\n");
  printf("  /call <user>          - Voice/Video call\n");
  printf("  /accept [file|call]   - Accept request\n");
  printf("  /hangup               - End call\n");
  printf("  /exit                 - Quit\n");
  printf("------------------------\n");

  // Main loop that parses typed commands and executes features.
  while (1) {
    fgets(buffer, BUFFER_SIZE, stdin);
    buffer[strcspn(buffer, "\n")] = 0; // remove newline

    if (strncmp(buffer, "/exit", 5) == 0) {
      msg.type = MSG_EXIT;
      send_msg_safe(&msg);
      break;
    } else if (strncmp(buffer, "/list", 5) == 0) {
      msg.type = MSG_LIST_REQ;
      send_msg_safe(&msg);
    } else if (strncmp(buffer, "/broadcast ", 11) == 0) {
      msg.type = MSG_TEXT;
      strcpy(msg.source, my_username);
      strcpy(msg.data, buffer + 11);
      send_msg_safe(&msg);
    } else if (strncmp(buffer, "/msg ", 5) == 0) {
      char target[NAME_LEN];
      char text[BUFFER_SIZE];
      sscanf(buffer + 5, "%s %[^\n]", target, text);
      msg.type = MSG_PRIVATE;
      strcpy(msg.source, my_username);
      strcpy(msg.target, target);
      strcpy(msg.data, text);
      send_msg_safe(&msg);
    } else if (strncmp(buffer, "/file ", 6) == 0) {

      char target[NAME_LEN];
      char filepath[256];
      sscanf(buffer + 6, "%s %s", target, filepath);

      // Store pending
      strcpy(pending_file_target, target);
      strcpy(pending_file_path, filepath);

      msg.type = MSG_FILE_OFFER;
      strcpy(msg.source, my_username);
      strcpy(msg.target, target);
      strcpy(msg.data, filepath); // Send filename
      send_msg_safe(&msg);
      printf("File offer sent to %s. Waiting for acceptance...\n", target);
    } else if (strncmp(buffer, "/call ", 6) == 0) {
      char target[NAME_LEN];
      sscanf(buffer + 6, "%s", target);
      msg.type = MSG_VOICE_REQ;
      strcpy(msg.source, my_username);
      strcpy(msg.target, target);
      send_msg_safe(&msg);
      strcpy(incoming_call_user,
             target); // Set target so we know who we called
      printf("Calling %s...\n", target);
    } else if (strncmp(buffer, "/accept file", 12) == 0) {
      if (strlen(incoming_file_user) == 0) {
        printf("No pending file offers.\n");
        continue;
      }
      msg.type = MSG_FILE_ACCEPT;
      strcpy(msg.source, my_username);
      strcpy(msg.target, incoming_file_user);
      send_msg_safe(&msg);

      // Prepare file for writing (truncate)
      char fname[300];
      snprintf(fname, sizeof(fname), "received_%s",
               incoming_file_name[0] ? incoming_file_name : "file.dat");
      FILE *fp = fopen(fname, "wb");
      if (fp)
        fclose(fp);

      printf("Accepted file from %s. Receiving...\n", incoming_file_user);
      incoming_file_user[0] = '\0'; // Clear pending
    } else if (strncmp(buffer, "/accept call", 12) == 0) {
      if (strlen(incoming_call_user) == 0) {
        printf("No pending calls.\n");
        continue;
      }
      msg.type = MSG_VOICE_ACCEPT;
      strcpy(msg.source, my_username);
      strcpy(msg.target, incoming_call_user);
      send_msg_safe(&msg);
      printf("Call accepted. Connected to %s.\n", incoming_call_user);
      in_call = 1;
      start_call_threads();
    } else if (strncmp(buffer, "/hangup", 7) == 0) {
      if (!in_call && strlen(incoming_call_user) == 0) {
        printf("No active call.\n");
        continue;
      }
      char *target = in_call ? incoming_call_user : incoming_call_user;
      // If in call, we might not have stored the target if we were the
      // caller? Actually we didn't store target if we were caller. Let's
      // fix that. For simplicity, we just broadcast END to the other party
      // if we know them. But wait, if I called, I didn't set
      // incoming_call_user. Let's just send MSG_VOICE_END to the last
      // person we interacted with or just generic? Protocol needs target.
      // Let's assume user types /hangup <user> or we track current call
      // partner. Tracking is better. For now, let's just ask user to type
      // /hangup <user> if we want to be precise, or just use
      // incoming_call_user. If I initiated, I need to know who I called.
      // Let's just send to 'incoming_call_user' if set.
      msg.type = MSG_VOICE_END;
      strcpy(msg.source, my_username);
      strcpy(msg.target,
             incoming_call_user); // This might be empty if I was caller.
      send_msg_safe(&msg);
      send_msg_safe(&msg);
      in_call = 0;
      stop_call_threads();
      printf("Call ended.\n");
    } else {
      printf("Unknown command.\n");
    }
  }

  CLOSESOCKET(sockfd);
#ifdef _WIN32
  WSACleanup();
#endif
  return 0;
}
