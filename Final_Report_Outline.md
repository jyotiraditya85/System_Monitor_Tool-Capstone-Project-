# Final Report Outline — Windows System Monitor (C++)
## 1. Title Page
- **Project Title:** Windows System Monitor (C++)
- **Course / Capstone:** Wipro Training Course
- **Student:** Jyotiraditya Mishra,2241013152
- **Date:** 9.11.25

---

## 2. Abstract (150–200 words)
A concise summary of problem, approach, environment, key features (CPU%, MEM%, sort, kill, filter), and results.

---

## 3. Objectives
- Build a terminal-based Windows system monitor without admin rights.
- Display reliable CPU% and memory statistics for processes.
- Provide real‑time interaction: sorting, filtering, kill, export.
- Deliver a minimal, dependency‑free C++ implementation.

---

## 4. Background & Rationale
- Overview of Windows process/memory concepts: Working Set, Private Bytes.
- Snapshot vs delta-based CPU measurement (`GetProcessTimes`, `GetSystemTimes`).
- Why a console app: portability, transparency, and learning OS APIs.

---

## 5. Environment & Tools
- OS: Windows 10/11
- Compiler: MinGW‑w64 (WinLibs standalone ZIP)
- Languages/APIs: C++20, Win32 APIs (`Toolhelp`, `Psapi`, `Kernel32`)
- Terminal: Windows Terminal / PowerShell / cmd.exe

Include a note on *no admin required* and how permissions affect protected processes.

---

## 6. System Design
### 6.1 Architecture Diagram (text or simple figure)
- **Input loop** (non‑blocking) → **Controller** → **Sampler** (system, per‑proc) → **View** (renderer)

### 6.2 Data Flow
1. Enumerate PIDs → open eligible processes  
2. Sample per‑process times & memory; sample system times & memory totals  
3. Compute CPU% deltas; compute MEM% (WS / total)  
4. Filter & sort rows; render

### 6.3 Modules
- **Sampler:** enumeration, times, memory  
- **Metrics:** CPU% and MEM% formulas  
- **UI:** renderer, interactive command line, help overlay  
- **Actions:** kill, export

---

## 7. Implementation (Day-wise)
### Day 1 — UI + System Sources
- Header layout; `GetSystemTimes`, `GlobalMemoryStatusEx`, uptime, hostname
- Verification screenshots

### Day 2 — Process Table + Memory
- PID enumeration; `GetProcessMemoryInfo`
- MEM% = WorkingSet / TotalPhys × 100
- Screenshot: table with MEM columns

### Day 3 — CPU% + Sorting
- Per‑process CPU via `GetProcessTimes` deltas vs `GetSystemTimes`
- Sort modes: cpu/mem/pid
- Screenshots: CPU spike test, different sort modes

### Day 4 — Kill + UX
- `k <pid>` with safeguards (Idle/System/Self) and error handling
- Screenshot: kill success + protected-process denial

### Day 5 — Polish
- Interactive input, pause, filter (`/`), `+/-`, CSV export (`x`), help (`h`)
- Screenshots: filter active, CSV generated

---

## 8. Testing & Evaluation
- **Functional tests:** appearance/disappearance of PIDs, MEM% reactions, CPU spikes, kill permissions.
- **Performance:** refresh latency with 150–200 rows; responsiveness of input.
- **Comparison:** trends vs Task Manager (explain acceptable variance).

Provide a small table of test cases, expected vs observed outcomes.

---

## 9. Results
- Summarize the working features.
- Include 4–6 annotated screenshots.
- Mention CSV sample rows (appendix).

---

## 10. Challenges & Resolutions
- Console input buffering (PowerShell) → solved with `_kbhit/_getch` interactive mode.
- Access to protected processes → graceful skip with error messages.
- CPU% normalization and differences vs Task Manager → rationale.

---

## 11. Limitations
- Protected/system processes may be inaccessible.
- Private Bytes availability varies.
- CPU% alignment with other tools is approximate but directionally correct.

---

## 12. Future Work
- Per‑user filters; colorized UI; process tree view; disk/net I/O.
- ncurses-style TUI or lightweight GUI.
- Logging & alert thresholds.

---

## 13. Conclusion
Reflect on objectives achieved, skills learned (Win32 APIs, performance monitoring, interactive CLI), and potential extensions.

---

## 14. References
- Microsoft Docs: `GetSystemTimes`, `GetProcessTimes`, `CreateToolhelp32Snapshot`, `GetProcessMemoryInfo`, `GlobalMemoryStatusEx`, `FormatMessage`.
- Any tutorials or articles consulted.

---

## 15. Appendices
- **A. Build & Run Commands**
- **B. CSV Sample Output** (paste a few lines)
- **C. Full CLI Help / Controls** (from README)
- **D. Additional Screenshots**
