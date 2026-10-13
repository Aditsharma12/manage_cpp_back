# backend_fast (C++ Drogon Framework + PostgreSQL)

A high-performance C++ backend for the Class Manager application built using the **Drogon** web framework and **PostgreSQL (Neon DB)**.

---

## 🚀 Architecture Overview

This backend provides identical HTTP REST endpoints and JSON contract to the Flask backend, running natively compiled C++ with asynchronous, non-blocking I/O and Drogon's native PostgreSQL database client.

### Directory Structure
```
backend_fast/
├── CMakeLists.txt           # Build configuration & dependency linking
├── config.json              # Drogon runtime settings & Neon PostgreSQL client config
├── Dockerfile               # Multi-stage container build for instant deployment
├── main.cc                  # Application bootstrap, CORS advice, Drogon run
├── include/
│   ├── AuthController.h     # Auth & user profile endpoint definitions
│   ├── DirectoryController.h# Student directory endpoint definition
│   ├── GroupController.h    # Group CRUD endpoint definitions
│   ├── InviteController.h   # Invite management endpoint definitions
│   ├── JwtAuthFilter.h      # Drogon HttpFilter for JWT Bearer token validation
│   ├── DbService.h          # PostgreSQL formatting & client helpers
│   └── SecurityUtils.h      # SHA256 password hashing & HMAC-SHA256 JWT utilities
└── src/
    ├── AuthController.cc
    ├── DirectoryController.cc
    ├── GroupController.cc
    ├── InviteController.cc
    ├── JwtAuthFilter.cc
    ├── DbService.cc
    └── SecurityUtils.cc
```

---

## 📡 Endpoints Implemented

| Method | Path | Auth Filter | Description |
| :--- | :--- | :--- | :--- |
| `POST` | `/api/auth/register` | None | Registers student (validates phone number, hashes password) |
| `POST` | `/api/auth/login` | None | Authenticates credentials and returns 7-day JWT token |
| `PUT` | `/api/users/profile` | `JwtAuthFilter` | Updates student profile (`name`, `roll_number`, `phone_number`, `about`) |
| `GET` | `/api/directory` | `JwtAuthFilter` | Returns directory of registered students |
| `POST` | `/api/groups` | `JwtAuthFilter` | Creates a new group and joins creator |
| `GET` | `/api/groups/my_groups` | `JwtAuthFilter` | Lists all groups the authenticated user belongs to |
| `GET` | `/api/groups/{id}` | `JwtAuthFilter` | Returns group details with populated members list |
| `PUT` | `/api/groups/{id}` | `JwtAuthFilter` | Updates group name & purpose (creator only) |
| `DELETE`| `/api/groups/{id}` | `JwtAuthFilter` | Deletes group (if creator) or leaves group (if member) |
| `POST` | `/api/invites` | `JwtAuthFilter` | Sends group invite to another student |
| `GET` | `/api/invites/pending` | `JwtAuthFilter` | Returns user's pending invites sorted by date |
| `POST` | `/api/invites/{id}/respond`| `JwtAuthFilter` | Accepts or declines invitation |

---

## 🛠️ Building & Running

### Option 1: Docker (Recommended, zero host setup)
```bash
docker build -t backend_fast .
docker run -p 5000:5000 backend_fast
```

### Option 2: Windows via `vcpkg`
```powershell
vcpkg install drogon[postgresql]:x64-windows openssl:x64-windows

cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="[path-to-vcpkg]/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
./build/Release/backend_fast.exe
```

### Option 3: Ubuntu / Debian Native
```bash
sudo apt update
sudo apt install -y build-essential cmake libjsoncpp-dev uuid-dev zlib1g-dev libssl-dev libpq-dev

# Setup .env
cp .env.example .env
# Edit .env with your Neon PostgreSQL credentials

# Build & run
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
./backend_fast ../config.json
```

---

## 🔒 Environment Variables (`.env`)

Secrets are loaded automatically from `.env` on startup:
```bash
POSTGRES_URI="postgresql://username:password@ep-your-host.neon.tech/neondb?sslmode=require"
DATABASE_URL="postgresql://username:password@ep-your-host.neon.tech/neondb?sslmode=require"
JWT_SECRET_KEY="your-jwt-secret-key"
PORT=5000
```
`.env` is ignored by Git to keep your database credentials safe.

