# Multi-Container Runtime with Kernel Memory Monitor

## 1. Team Information

- Name 1: **Prakruti Prasanna Bhat**
- SRN 1: **PES1UG24CS330**

- Name 2: **Prachi Ganesh Joshi**
- SRN 2: **PES1UG24CS325**

---

## 2. Build, Load, and Run Instructions

### Requirements
- Ubuntu 22.04 / 24.04 VM
- gcc, make
- Linux headers installed

---

### Step 1: Build the Project

```bash
make
```

### Step 2: Load Kernel Module

```bash
sudo insmod monitor.ko
```

Verify device:

```bash
ls -l /dev/container_monitor
```

### Step 3: Start Supervisor

```bash
sudo ./engine supervisor ./rootfs-base
```

### Step 4: Create Container RootFS Copies

```bash
cp -a ./rootfs-base ./rootfs-alpha
cp -a ./rootfs-base ./rootfs-beta
```

### Step 5: Run Containers

In another terminal:

```bash
sudo ./engine start alpha ./rootfs-alpha "/io_pulse"
sudo ./engine start beta ./rootfs-beta "/io_pulse"
```

### Step 6: CLI Commands

```bash
sudo ./engine ps
sudo ./engine logs alpha
sudo ./engine stop alpha
```

### Step 7: Unload Module

```bash
sudo rmmod monitor
```

---

## 3. Demo with Screenshots

### 3.1 Multi-container Supervision
Shows two containers running under a single supervisor process.

### 3.2 Metadata Tracking
Displays container metadata using `engine ps`.

### 3.3 Bounded-buffer Logging
Shows logs captured through producer-consumer pipeline.

### 3.4 CLI and IPC
Demonstrates CLI communicating with supervisor via UNIX socket.

### 3.5 Soft-limit Warning
Kernel logs showing memory soft limit exceeded.

### 3.6 Hard-limit Enforcement
Kernel kills container when hard limit is exceeded.

### 3.7 Scheduling Experiment
Comparison of containers with different nice values.

### 3.8 Clean Teardown
Shows no zombie processes after shutdown.

---

## 4. Engineering Analysis

### 4.1 Isolation Mechanisms
Containers are created using Linux namespaces (`CLONE_NEWPID`, `CLONE_NEWUTS`, `CLONE_NEWNS`) to isolate processes, hostname, and mount points. Each container uses `chroot()` to operate within its own filesystem.

### 4.2 Supervisor and Process Lifecycle
A long-running supervisor manages all containers. It tracks metadata, handles CLI commands, and reaps child processes using `waitpid()` to prevent zombies.

### 4.3 IPC, Threads, and Synchronization
Two IPC mechanisms are used:
- UNIX domain sockets for CLI communication
- Pipes for log transfer

Logging uses a bounded buffer with producer-consumer threads synchronized via mutexes and condition variables.

### 4.4 Memory Management and Enforcement
The kernel module tracks container PIDs and periodically checks RSS usage.
- **Soft limit:** generates warning
- **Hard limit:** kills process using `SIGKILL`

Kernel-space enforcement ensures reliable control over memory usage.

### 4.5 Scheduling Behavior
Containers are used to test Linux scheduling by varying nice values and workload types. CPU-bound and I/O-bound behaviors are observed to understand scheduler decisions.

---

## 5. Design Decisions and Tradeoffs

| Component | Choice | Tradeoff | Justification |
|---|---|---|---|
| Namespace Isolation | Linux namespaces + chroot | Lightweight but shares host kernel | Efficient and sufficient for isolation requirements |
| Supervisor Architecture | Single long-running supervisor | Central dependency point | Simplifies lifecycle and metadata management |
| IPC and Logging | Separate IPC paths (socket + pipe) | Increased complexity | Avoids interference between control and log traffic |
| Kernel Monitor | Kernel module for enforcement | Requires kernel-level debugging | Accurate and reliable memory control |
| Scheduling Experiments | CPU-bound and I/O-bound workloads | Requires tuning workload duration | Clearly demonstrates scheduler behavior |

---

## 6. Scheduler Experiment Results

### Experiment 1: CPU vs CPU (Different Nice Values)

| Container | Nice | Observation |
|---|---|---|
| cpuA | 0 | Higher priority |
| cpuB | 10 | Lower priority |

On multi-core systems, both received similar CPU since they ran on separate cores.

### Experiment 2: CPU vs I/O

| Workload | Behavior |
|---|---|
| CPU-bound | Continuous CPU usage |
| I/O-bound | Periodic bursts |

The scheduler favors responsiveness of I/O-bound processes.

### Conclusion
Linux scheduler distributes tasks across cores efficiently. Nice values impact scheduling only under CPU contention, while workload type influences responsiveness.

---

## Final Notes

All required tasks have been implemented:
- Multi-container runtime
- CLI + IPC
- Logging system
- Kernel memory monitor
- Scheduling experiments
- Clean teardown

The project demonstrates key OS concepts including process management, synchronization, memory control, and scheduling.
