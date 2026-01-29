# ⚛️ Atom Warehouse & Drinks Bar Server

A comprehensive **System Programming** project implemented in **C** on Linux. This repository demonstrates the evolution of a networked server application through 6 stages of increasing complexity, focusing on **Inter-Process Communication (IPC)**, **Socket Programming**, and **Concurrency**.

## 🚀 Project Evolution

The project simulates a resource management system (Atoms -> Molecules -> Drinks), evolving from a simple TCP server to a robust, concurrent system.

### 🔹 Stage 1: The Atom Warehouse (TCP)
* **Goal:** A server that stores atomic elements (Carbon, Oxygen, Hydrogen).
* **Tech:** Basic **TCP Sockets** implementation.
* **Architecture:**
    * `atom_warehouse`: Listens on a TCP port using **I/O Multiplexing** to handle multiple connections.
    * `atom_supplier`: A client that connects and sends `ADD` commands.

### 🔹 Stage 2-4: Molecules & Logic Expansion
* **Expansion:** Introduction of new entities (`molecule_requester`, `molecule_supplier`) interacting with the system.
* **Logic:** Creating complex molecules from available atoms.

### 🔹 Stage 5: Unix Domain Sockets (UDS)
* **Upgrade:** The server is enhanced to support local IPC via **Unix Domain Sockets**.
* **Features:**
    * Support for **Stream** UDS (`-s` flag).
    * Support for **Datagram** UDS (`-d` flag).
    * Hybrid operation: Handling TCP and UDS clients simultaneously.

### 🔹 Stage 6: Concurrency & Persistence (The Drinks Bar)
* **Concurrency:** Upgraded to handle multiple "Bartenders" (Clients) simultaneously interacting with the inventory.
* **Persistence:** Implements state saving/loading from a file using the `-f` flag.
* **Robustness:** Handles conflicting arguments and ensures data integrity.

## 🛠️ Tech Stack
* **Language:** C (System Calls)
* **OS:** Linux
* **Networking:** TCP/IP, Unix Domain Sockets (Stream & Dgram).
* **System APIs:** `socket`, `bind`, `listen`, `accept`, `select`/`poll` (Multiplexing).
* **Tools:** Make, Gcov (Code Coverage), Bash Testing Scripts.

## 📂 Project Structure
```text
├── EX1/            # Initial TCP Atom Warehouse
├── EX2-4/          # Logic expansion (Molecule suppliers/requesters)
├── EX5/            # Integration of UDS (Unix Domain Sockets)
├── EX6/            # Final Version: Drinks Bar with Persistence & Concurrency
├── makefile        # Global build script
└── testcase.sh     # Automated testing scripts
