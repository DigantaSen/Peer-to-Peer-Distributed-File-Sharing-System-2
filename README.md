# AOS Assignment 3 - Interim Submission

## Overview

This project is part of **AOS Assignment 3: Peer-to-Peer Distributed File Sharing System** (IIIT Hyderabad, Monsoon 2025).  
The current interim submission implements the **Tracker synchronization mechanism** and **basic Client commands**.

Two trackers coordinate to provide redundancy and maintain metadata consistency across the distributed file-sharing system.  
Clients can connect to trackers, register/login, create groups, join groups, and perform other user/group management actions.

---

## Implemented Components (Interim Stage)

### 1. Tracker System
- Two trackers (`tracker.cpp`, `sync.cpp`, `SystemState.cpp`) maintain **synchronized state**.
- Tracks users, groups, memberships, and session states.
- Synchronization ensures that updates to one tracker (e.g., user creation, group creation) propagate to the other tracker.
- Roles:
  - **Primary Tracker**: Handles client requests and sends updates to the backup.
  - **Backup Tracker**: Receives updates from the primary and can be promoted if the primary fails.

### 2. Synchronization
- Implemented in `sync.cpp`:
  - Primary listens for backup connections.
  - Backup sends **SYNC_REQUEST** and loads state from primary.
  - Updates are propagated via **UPDATE** messages.
  - Handles disconnection and **failover promotion** to primary.
- Full system state (users, groups, requests, file metadata) is serialized/deserialized for synchronization.

### 3. SystemState
- `SystemState.cpp` manages:
  - User creation, login, logout.
  - Group creation, join requests, request acceptance.
  - Membership management (leave group, ownership transfer).
- Ensures thread-safe state handling with mutex locking.

### 4. Client System
- The client (`client.cpp`, `commands.cpp`) interacts with trackers.
- Supported commands:
  - `create user <user_id> <password>`
  - `login <user_id> <password>`
  - `create group <group_id>`
  - `list groups`
  - `join group <group_id>`
  - `list requests <group_id>`
  - `accept request <group_id> <user_id>`
  - `leave group <group_id>`
  - `logout`
- Basic client loop implemented in `main.cpp`.

---

## Compilation

The project includes a `Makefile`. To build:

```bash
make
```

This generates executables for both **tracker** and **client**.

---

## Execution

### Tracker
Start two tracker servers:

```bash
./tracker tracker_info.txt 1
./tracker tracker_info.txt 2
```

- `tracker_info.txt` contains tracker addresses and ports:
  ```
  127.0.0.1 5000
  127.0.0.1 5001
  ```

- One tracker assumes **PRIMARY** role, the other becomes **BACKUP**.

### Client
Run a client and connect to tracker:

```bash
./client <IP>:<PORT> tracker_info.txt
```

Example:
```bash
./client 127.0.0.1:5000 tracker_info.txt
```

---

## Progress Summary (Interim Submission)

- ✅ Tracker synchronization mechanism (primary-backup, failover, state sync).  
- ✅ User and group management in tracker.  
- ✅ Client basic commands (user/group operations).  
- ⏳ File upload/download not implemented yet (to be completed in final submission).  
- ⏳ Advanced client-to-client communication pending.  
- ⏳ Piece-level file integrity (SHA1) not yet integrated.  

---

## Next Steps (Final Submission Goals)

- Implement **file upload/download** with SHA1-based integrity checks.  
- Support **multi-peer downloads** with concurrency.  
- Extend synchronization to handle file-sharing metadata fully.  
- Implement download progress reporting (`show downloads`).  

---
