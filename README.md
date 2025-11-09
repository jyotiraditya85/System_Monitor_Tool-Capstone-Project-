# Windows System Monitor (C++) — README

A lightweight, Windows-native, terminal-based system monitor written in C++ (MinGW‑w64), with real‑time **CPU%**, **MEM%**, **Working Set**, **Private Bytes**, **sorting**, **process kill**, **live filter**, **pause/resume**, and **CSV export**. No admin rights required.

---

## ✨ Features (Day 1 → Day 5)
- **Day 1:** Header layout + data sources (hostname, uptime, CPU header %, memory totals)
- **Day 2:** Process table — **PID, NAME, MEM%, WS, Private Bytes** (CPU% placeholder)
- **Day 3:** **Per‑process CPU%** via time deltas + **runtime sorting** (`s cpu|mem|pid`)
- **Day 4:** **Kill action** `k <pid>` with safeguards (blocks Idle/System/Self), clear messages
- **Day 5:** **Interactive input** (no lag), **pause/resume**, **/ filter**, `+/-` delay adjust, **CSV export**, **help overlay**

---

## 🧰 Requirements
- **OS:** Windows 10/11
- **Compiler:** MinGW‑w64 (WinLibs standalone ZIP, no admin install)
- **Terminal:** Windows Terminal / PowerShell / cmd.exe (ANSI capable)

---

## 🛠️ Setup (no admin)
1. Download WinLibs (portable MinGW‑w64) and unzip, e.g.  
   `C:\Users\<You>\DevTools\winlibs\mingw64\bin\g++.exe`
2. Open **PowerShell** and add it to **PATH** for this window:
   ```powershell
   $env:Path = "C:\Users\<You>\DevTools\winlibs\mingw64\bin;" + $env:Path
   g++ --version
   ```
   You should see the compiler version.

---

## 🧪 Build
Place `win_sysmon_day5.cpp` in a project folder, e.g. `C:\Users\<You>\Desktop\sysmon`, then:
```powershell
cd "C:\Users\<You>\Desktop\sysmon"
g++ -std=c++20 -O2 -Wall -Wextra win_sysmon_day5.cpp -o win-sysmon-day5 -lpsapi -ladvapi32
```

> If you’re using **cmd.exe**:
> ```bat
> set "PATH=C:\Users\<You>\DevTools\winlibs\mingw64\bin;%PATH%"
> cd C:\Users\<You>\Desktop\sysmon
> g++ -std=c++20 -O2 -Wall -Wextra win_sysmon_day5.cpp -o win-sysmon-day5 -lpsapi -ladvapi32
> ```

---

## ▶️ Run
```powershell
.\win-sysmon-day5 --delay=2 --rows=200 --sort=cpu
```

---

## ⌨️ Controls (Day‑5)
- `h` — toggle help overlay  
- `p` — pause/resume auto‑refresh  
- `+` / `-` — increase / decrease delay by 1s  
- `d <sec>` — set delay precisely  
- `s cpu|mem|pid` — change sort mode  
- `/ text` — filter by process **name substring** (case‑insensitive)  
  - `/` alone clears the filter  
- `k <pid>` — attempt to terminate a process (you must have rights)  
- `x` — export current table to **CSV** (saved in the working folder)  
- `q` — quit  
- **Editing input:** live command line; **Enter** executes, **Backspace** edits, **ESC** clears

---

## 📊 Columns
- **PID** — process ID  
- **CPU%** — per‑process CPU over the last interval, normalized vs system delta  
- **MEM%** — Working Set / Total Physical Memory × 100  
- **Working Set** — resident bytes in RAM (live)  
- **Private Bytes** — committed private memory (may be 0 on some systems)  
- **NAME** — base module name or full image path (fallback)

---

## 🧱 Architecture overview
- **Enumeration:** `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS)` → PID list  
- **Open:** `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, …)`  
- **Times:** `GetProcessTimes` (per‑process), `GetSystemTimes` (header CPU%)  
- **Memory:** `GetProcessMemoryInfo` (WorkingSetSize, PrivateUsage), `GlobalMemoryStatusEx` (totals)  
- **UI:** ANSI clear + live, non‑blocking input (`_kbhit`, `_getch`) for instant commands  
- **Safety:** Kill blocks PIDs 0 (Idle), 4 (System), and self; error messages show `FormatMessage` text

---

## 🧷 Known limitations
- Protected/system processes may not open; they’re skipped with informative errors.  
- Private Bytes can be 0 depending on OS/privileges.  
- CPU% is relative to the system delta across the refresh interval (values vary from Task Manager but trend correctly).

---

## 🩺 Troubleshooting
- **`g++` not found:** PATH isn’t set in this terminal — run:
  ```powershell
  $env:Path = "C:\Users\<You>\DevTools\winlibs\mingw64\bin;" + $env:Path
  g++ --version
  ```
- **Weird screen redraw:** use Windows Terminal / PowerShell.  
- **Cannot kill process:** you likely don’t own it or lack rights; system PIDs are blocked by design.  
- **CSV missing:** check write permissions; filename printed like `sysmon_YYYYMMDD_HHMMSS.csv`.

---

## 🖼️ Suggested screenshots for submission
1. Main table (sorted by CPU) with header showing CPU and Mem lines  
2. Sorted by MEM (`s mem`) with visible memory‑heavy processes  
3. Kill flow (`k <pid>`) showing success/error message  
4. Filter (`/ chrome`) active; show the footer reflecting the filter  
5. Export confirmation and the generated CSV opened in Excel

---

## 📄 License
For coursework submission; adapt as needed.
