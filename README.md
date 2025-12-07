# Chat Application with Cross-Platform C

A real-time chat application built in C, supporting text messaging, file transfer, and Audio/Video calling (via UDP and FFmpeg/SoX). Tested on macOS and Windows.

## 🚀 Quick Start Command Table

| Step | **macOS** (Terminal) | **Windows** (PowerShell / CMD) |
| :--- | :--- | :--- |
| **1. Install Compiler** | `xcode-select --install` | `winget install -e --id MinGW.MinGW` |
| **2. Install Media Tools** | `brew install sox ffmpeg` | `winget install Gyan.FFmpeg`<br>*(Also install [SoX](https://sourceforge.net/projects/sox/) & add to PATH)* |
| **3. Compile Code** | `gcc server.c -o server -pthread`<br>`gcc client.c -o client -pthread` | Double-click **`compile_windows.bat`** |
| **4. Check IP Address** | `ipconfig getifaddr en0` | `ipconfig` *(Look for IPv4)* |
| **5. Run Server** | `./server` | `server.exe` *(Allow Firewall)* |
| **6. Run Client** | `./client <SERVER_IP>` | `client.exe <SERVER_IP>` |

---

## 📂 Project Structure
- `server.c`: Handles TCP connections (login, chat) and UDP relay (media).
- `client.c`: The user app. Captures audio/video and accepts commands.
- `common.h`: Shared constants and protocol definitions.
- `compile_windows.bat`: One-click compile script for Windows.
- `INSTRUCTIONS.md`: Detailed user guide.

## 🔑 Demo Accounts
These users are pre-configured in `users.txt`. You can add more there.

| Username | Password |
| :--- | :--- |
| `admin` | `admin123` |
| `user1` | `pass1` |
| `user2` | `pass2` |
| `khan` | `1234` |

## 🎥 Video Calling Note
On **Windows**, you must replace `"Integrated Camera"` in `client.c` (line ~60) with your actual webcam name correctly found via `ffmpeg -list_devices true -f dshow -i dummy`.

## 🛠️ Installing Git on Windows

If you need to clone this repo on Windows, you must install Git first.

### Option 1: Command Line (Easiest)
Open PowerShell or CMD and run:
```powershell
winget install --id Git.Git -e --source winget
```

### Option 2: Manual Installer
1.  Download from [git-scm.com/download/win](https://git-scm.com/download/win).
2.  Run the installer and keep clicking "Next" (Default settings are fine).
3.  Ensure **"Git from the command line"** is selected during setup.

### Check Installation
Open a new terminal and run:
```bash
git --version
```
If you see a version number, you are ready to clone!
