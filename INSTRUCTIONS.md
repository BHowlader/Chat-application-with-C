# Chat Application with Audio/Video Call - User Guide

## 1. Prerequisites (Updated for Windows & Mac)

### Mac OS
```bash
brew install sox ffmpeg
```

### Windows
1.  **Download SoX**: Get the binaries from [SourceForge](https://sourceforge.net/projects/sox/).
    - Add the SoX folder to your **System PATH**.
    - Ensure `sox` command works in PowerShell/CMD.
2.  **Download FFmpeg**: Get the "Essentials" build from [Gyan.dev](https://www.gyan.dev/ffmpeg/builds/) or [BtbN](https://github.com/BtbN/FFmpeg-Builds/releases).
    - Add the `bin` folder to your **System PATH**.
    - Ensure `ffmpeg` and `ffplay` commands work.

## 2. Windows Video Setup (Important!)

On Windows, cameras have specific names (like "Logitech Webcam C920" or "Integrated Camera"). The code cannot guess this name.

1.  Run this command in terminal to find your camera name:
    ```cmd
    ffmpeg -list_devices true -f dshow -i dummy
    ```
2.  Look for the video device name (e.g., `"Integrated Camera"`).
3.  **Edit `client.c`**:
    - Go to line ~90 (in `send_video_handler`).
    - Change `"Integrated Camera"` to your actual camera name inside the quotes.
    - Recompile the client.

## 3. Compilation

**macOS/Linux:**
```bash
gcc server.c -o server -pthread
gcc client.c -o client -pthread
```

**Windows (MinGW/gcc):**
*Option 1: Double-click `compile_windows.bat` (Recommended)*
This script will automatically compile both `server.exe` and `client.exe` for you.

*Option 2: Manual Command*
```cmd
gcc server.c -o server -lws2_32
gcc client.c -o client -lws2_32
```
*(Winsock library `ws2_32` is required on Windows).*

## 4. Running the App

**Start Server:**
```bash
./server
```

**Start Client:**
```bash
./client <SERVER_IP>
```

## 5. Usage
- **Chat**: `/msg`, `/broadcast`
- **Call**: `/call <username>` -> Other user types `/accept call`.
- **File**: `/file <username> <path>` -> `/accept file`.
