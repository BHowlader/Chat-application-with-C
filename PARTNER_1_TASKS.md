# Project Tasks & Code Explanation: Partner 1

This document outlines the responsibilities and code logic for **Partner 1**, focusing on the **Client-Side** and **User Interaction** components of the Chat Application.

## 1. Client-Side Implementation
**File:** `client.c`

Your primary responsibility is the `client.c` file, which acts as the user's entry point to the system.
- **Startup & Connection**: You handle the `socket()` creation and `connect()` system call to establish a TCP connection with the server.
- **Threading**: The client uses **multithreading** to send and receive simultaneously.
  - **Main Thread**: Handles user input (keyboard) and sends commands to the server.
  - **Receive Thread (`receive_handler`)**: constantly listens for incoming messages from the server (chat, file offers, call requests) and updates the UI.

## 2. User Interface Design (CLI)
**File:** `client.c` (Main Loop)

Since this is a terminal-based app, the "UI" is a Command Line Interface (CLI).
- **Command Parsing**: Inside the `main()` function's `while(1)` loop, your code parses user input strings.
  - It looks for headers like `/msg`, `/broadcast`, or `/list`.
  - It uses `strncmp` to detect the command and `sscanf` to extract arguments (like target username or message text).
- **Feedback**: You use `printf` to display prompts, incoming messages, and system notifications (e.g., "[Private] User1: ...").

## 3. File Transfer Protocol (Client)
**File:** `client.c` (`send_file` function & `MSG_FILE_*` cases)

You implemented a 3-step handshake protocol for transferring files:
1.  **Offer (`/file`)**: User types `/file <user> <path>`. Your code verifies the file exists and sends a `MSG_FILE_OFFER` packet containing the filename.
2.  **Accept (`/accept file`)**: The receiver (in their `receive_handler`) sees the offer. When they type `/accept file`, your code sends `MSG_FILE_ACCEPT`.
3.  **Transfer (`MSG_FILE_DATA`)**:
    - Sender's client waits for the "Accept" signal.
    - Inside `send_file`, you read the file in binary chunks (1024 bytes) and send them as `MSG_FILE_DATA` packets.
    - Receiver's client appends these chunks to a new file (e.g., `received_image.png`).

## 4. Audio/Video Call Signaling (Client)
**File:** `client.c` (Call Logic & `common.h` structs)

This task involves setting up the "phone call" before the actual media flows.
- **Request (`/call`)**: When the user types `/call <user>`, you send a `MSG_VOICE_REQ` over TCP. This alerts the other user's client.
- **Acceptance (`/accept call`)**: When the user accepts, you send `MSG_VOICE_ACCEPT`.
- **Activation**: This "Signaling" phase triggers the UDP threads (`start_call_threads`).
- **Media Streaming**:
    - **Audio**: Your code actively captures microphone input (via `rec`) and streams it over UDP.
    - **Video**: The *protocol* (`MEDIA_VIDEO` tag) and *signaling* are in place, but actual **Video Capture** (Webcam) is not implemented in standard C without external libraries (like OpenCV). The current system establishes the connection for both, but purely streams Audio.

## 5. User Authentication Module
**Files:** `client.c` (Login Block) & `server.c` (Validation)

You handle the secure entry into the system.
- **Input**: Before the main loop starts, you prompt the user for `Username` and `Password`.
- **Protocol**: You package these into a `MSG_LOGIN` struct and send it immediately upon connecting.
- **Verification**: The client then **blocks/waits** (using `while(logged_in == 0)`) until the server replies with `MSG_LOGIN_SUCCESS` or `MSG_LOGIN_FAIL`.
- **Security Check**: This ensures no user can send or receive chat messages without valid credentials stored in `users.txt`.
