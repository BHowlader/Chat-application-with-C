#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define CLOSESOCKET closesocket
#define SLEEP_MS(x) Sleep(x)
#define CLEAR_SCREEN "cls"
#define popen _popen
#define pclose _pclose
// Thread wrappers
typedef HANDLE Thread;
#define THREAD_FUNC DWORD WINAPI
#define THREAD_RET 0
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define CLOSESOCKET close
#define SLEEP_MS(x) usleep((x) * 1000)
#define CLEAR_SCREEN "clear"
// Thread wrappers
typedef pthread_t Thread;
#define THREAD_FUNC void *
#define THREAD_RET NULL
#endif

#define PORT 9999
#define BUFFER_SIZE 4096
#define NAME_LEN 32
#define PASS_LEN 32
#define MAX_CLIENTS 10

// Message Types
typedef enum {
  MSG_LOGIN,
  MSG_LOGIN_SUCCESS,
  MSG_LOGIN_FAIL,
  MSG_TEXT,         // Broadcast or General
  MSG_PRIVATE,      // One-to-One
  MSG_LIST_REQ,     // Request active user list
  MSG_LIST_RES,     // Response with user list
  MSG_FILE_OFFER,   // Sender offers file
  MSG_FILE_ACCEPT,  // Receiver accepts file
  MSG_FILE_DATA,    // File content chunk
  MSG_VOICE_REQ,    // Call request
  MSG_VOICE_ACCEPT, // Call accepted
  MSG_VOICE_END,    // Call ended
  MSG_EXIT
} MsgType;

// Protocol Structure
typedef struct {
  MsgType type;
  char source[NAME_LEN];
  char target[NAME_LEN];  // For private msg, file, or call
  char data[BUFFER_SIZE]; // Text content or file chunk
  int data_len;           // For file transfer
} Message;

// User credentials for auth
typedef struct {
  char username[NAME_LEN];
  char password[PASS_LEN];
} UserCred;

#define UDP_PORT 8889
#define MEDIA_CHUNK_SIZE 1024

// UDP Media Encapsulation
typedef enum {
  MEDIA_AUDIO,
  MEDIA_VIDEO,
  MEDIA_REGISTER // To associate UDP address with username
} MediaType;

typedef struct {
  MediaType type;
  char source[NAME_LEN];
  char target[NAME_LEN];
  int data_len;
  unsigned char data[MEDIA_CHUNK_SIZE];
} MediaPacket;

#endif
