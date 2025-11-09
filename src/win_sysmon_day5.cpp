#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <conio.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <fstream>
#include <ctime>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "advapi32.lib")

using namespace std;

// -------------------- helpers --------------------
static string trim(const string& s){
    size_t a=s.find_first_not_of(" \t\r\n"), b=s.find_last_not_of(" \t\r\n");
    if(a==string::npos) return "";
    return s.substr(a,b-a+1);
}
static string lower(string s){ transform(s.begin(), s.end(), s.begin(), ::tolower); return s; }
static void cls(){ cout << "\x1b[2J\x1b[H"; }
static string humanBytesUL(ULONGLONG bytes){
    double v=(double)bytes; const char* u[]={"B","KiB","MiB","GiB","TiB"}; int i=0;
    while(v>=1024.0 && i<4){ v/=1024.0; ++i; }
    ostringstream o; o<<fixed<<setprecision(1)<<v<<" "<<u[i]; return o.str();
}
static string winErr(DWORD code){
    if(code==0) return "OK";
    LPSTR buf=nullptr;
    DWORD n=FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, code, MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPSTR)&buf,0,NULL);
    string s = (n && buf)? string(buf): "Unknown error";
    if(buf) LocalFree(buf);
    while(!s.empty() && (s.back()=='\r'||s.back()=='\n')) s.pop_back();
    return s;
}
static string timestamp(){
    std::time_t t=std::time(nullptr); std::tm tm{};
    localtime_s(&tm, &t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
    return buf;
}

// -------------------- system snapshots --------------------
struct SysTimes { ULONGLONG idle=0,kernel=0,user=0; };
static ULONGLONG FT2ULL(const FILETIME& ft){ ULARGE_INTEGER u; u.LowPart=ft.dwLowDateTime; u.HighPart=ft.dwHighDateTime; return u.QuadPart; }
static SysTimes getSysTimes(){
    FILETIME i,k,u; SysTimes st{}; if(GetSystemTimes(&i,&k,&u)){ st.idle=FT2ULL(i); st.kernel=FT2ULL(k); st.user=FT2ULL(u);} return st;
}
struct MemSummary { ULONGLONG totalPhys=0, availPhys=0; };
static MemSummary getMemSummary(){
    MEMORYSTATUSEX ms{sizeof(ms)}; MemSummary m{}; if(GlobalMemoryStatusEx(&ms)){ m.totalPhys=ms.ullTotalPhys; m.availPhys=ms.ullAvailPhys; } return m;
}
static string hostName(){ char h[256]{0}; DWORD sz=256; if(GetComputerNameA(h,&sz)) return string(h); return "HOST"; }
static string uptimeNice(){
    ULONGLONG s=GetTickCount64()/1000ULL, d=s/86400ULL; s%=86400ULL; ULONGLONG h=s/3600ULL; s%=3600ULL; ULONGLONG m=s/60ULL;
    ostringstream o; o<<d<<"d "<<h<<"h "<<m<<"m"; return o.str();
}

// -------------------- per-process --------------------
struct ProcTimes { ULONGLONG k=0,u=0; };
struct Row {
    DWORD pid=0;
    string name;
    double cpuPct=0.0;
    double memPct=0.0;
    SIZE_T ws=0, priv=0;
};

static vector<DWORD> listPids(){
    vector<DWORD> v; HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snap==INVALID_HANDLE_VALUE) return v;
    PROCESSENTRY32 pe{sizeof(pe)};
    if(Process32First(snap,&pe)){ do{ v.push_back(pe.th32ProcessID);} while(Process32Next(snap,&pe)); }
    CloseHandle(snap); return v;
}
static string procName(HANDLE h){
    char n[MAX_PATH]{0}; if(GetModuleBaseNameA(h,NULL,n,MAX_PATH)) return string(n);
    char img[MAX_PATH]{0}; DWORD sz=MAX_PATH; if(QueryFullProcessImageNameA(h,0,img,&sz)) return string(img);
    return "-";
}
static bool procMem(HANDLE h, SIZE_T& ws, SIZE_T& priv){
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    if(GetProcessMemoryInfo(h,(PROCESS_MEMORY_COUNTERS*)&pmc,sizeof(pmc))){ ws=pmc.WorkingSetSize; priv=pmc.PrivateUsage; return true; }
    return false;
}
static bool procTimes(HANDLE h, ProcTimes& t){ FILETIME ct,et,kt,ut; if(GetProcessTimes(h,&ct,&et,&kt,&ut)){ t.k=FT2ULL(kt); t.u=FT2ULL(ut); return true; } return false; }

// -------------------- configuration & UI state --------------------
struct Config {
    int delaySec = 2;
    size_t maxRows = 200;
    string sortKey = "cpu";  // cpu|mem|pid
    string nameFilter = "";  // case-insensitive substring
    bool paused = false;
    bool showHelp = false;
};

// -------------------- rendering --------------------
static void printHeader(double sysCpuPct, const MemSummary& mem){
    cout<<fixed<<setprecision(1);
    cout<<"Host: "<<hostName()<<"    Uptime: "<<uptimeNice()<<"    CPU: "<<sysCpuPct<<"%\n";
    cout<<"Mem:  "<<humanBytesUL(mem.totalPhys)<<" total, "
        <<humanBytesUL(mem.totalPhys - mem.availPhys)<<" used, "
        <<humanBytesUL(mem.availPhys)<<" free\n";
    cout<<"\nPID      CPU%   MEM%   Working Set        Private Bytes      NAME\n";
    cout<<"--------------------------------------------------------------------------\n";
}
static void printRows(const vector<Row>& rows){
    cout<<fixed<<setprecision(1);
    for(const auto& r: rows){
        cout<<left<<setw(8)<<r.pid<<" "
            <<right<<setw(5)<<r.cpuPct<<"  "
            <<right<<setw(5)<<r.memPct<<"  "
            <<right<<setw(14)<<humanBytesUL(r.ws)<<"  "
            <<right<<setw(14)<<humanBytesUL(r.priv)<<"  "
            <<left<<r.name.substr(0,70)<<"\n";
    }
}
static void printFooter(const Config& cfg, const string& cmdline){
    cout<<"\nCommands: q | h | p | + / - | s cpu|mem|pid | d <sec> | / <text> | k <pid> | x (CSV)\n";
    cout<<"Current: delay="<<cfg.delaySec<<"s  sort="<<cfg.sortKey<<"  rows="<<cfg.maxRows;
    if(cfg.paused) cout<<"  [PAUSED]";
    if(!cfg.nameFilter.empty()) cout<<"  filter='"<<cfg.nameFilter<<"'";
    cout<<"\n> "<<cmdline<<flush; // live command line
}
static void printHelp(){
    cout<<"\n=== HELP ===\n"
        <<"q: quit\n"
        <<"h: toggle this help\n"
        <<"p: pause/resume auto refresh\n"
        <<"+ / -: increase/decrease refresh delay by 1s\n"
        <<"d N: set delay to N seconds\n"
        <<"s cpu|mem|pid: sort mode\n"
        <<"/ text: filter by process NAME substring (case-insensitive); '/' alone clears\n"
        <<"k PID: terminate process PID (if permitted)\n"
        <<"x: export current table to CSV in program folder\n"
        <<"Enter: execute the command line; ESC: clear line; Backspace: edit\n"
        <<"============\n";
}

// -------------------- CSV export --------------------
static bool exportCSV(const vector<Row>& rows){
    string file = string("sysmon_") + timestamp() + ".csv";
    ofstream out(file, ios::out | ios::trunc);
    if(!out) return false;
    out<<"PID,CPU%,MEM%,WorkingSet,PrivateBytes,Name\n";
    out<<fixed<<setprecision(1);
    for(const auto& r: rows){
        out<<r.pid<<","<<r.cpuPct<<","<<r.memPct<<","
           <<r.ws<<","<<r.priv<<",\"";
        // escape quotes in name
        for(char c: r.name){ if(c=='"') out<<"\"\""; else out<<c; }
        out<<"\"\n";
    }
    out.close();
    cout<<"\n[EXPORTED] "<<file<<"\n";
    return true;
}

// -------------------- commands --------------------
static void handleKill(const string& arg){
    string s = trim(arg);
    if(s.empty()){ cout<<"\n[ERR] usage: k <pid>\n"; return; }
    DWORD pid=0; try{ pid=(DWORD)stoul(s); }catch(...){ cout<<"\n[ERR] invalid PID\n"; return; }
    DWORD self=GetCurrentProcessId();
    if(pid==0 || pid==4 || pid==self){ cout<<"\n[SAFEGUARD] Refusing to terminate PID "<<pid<<" (Idle/System/Self).\n"; return; }
    HANDLE h=OpenProcess(PROCESS_TERMINATE,FALSE,pid);
    if(!h){ cout<<"\n[ERR] Cannot open PID "<<pid<<" for terminate. "<<winErr(GetLastError())<<"\n"; return; }
    if(TerminateProcess(h,1)) cout<<"\n[SENT] Terminate request to PID "<<pid<<"\n";
    else cout<<"\n[ERR] TerminateProcess failed: "<<winErr(GetLastError())<<"\n";
    CloseHandle(h);
}

// -------------------- main --------------------
int main(int argc, char** argv){
    ios::sync_with_stdio(false); cin.tie(nullptr);

    Config cfg;
    for(int i=1;i<argc;i++){
        string a=argv[i];
        if(a.rfind("--delay=",0)==0) cfg.delaySec=max(1,stoi(a.substr(8)));
        else if(a.rfind("--rows=",0)==0) cfg.maxRows=(size_t)max(10,stoi(a.substr(7)));
        else if(a.rfind("--sort=",0)==0) cfg.sortKey=a.substr(7);
    }

    SysTimes prevSys=getSysTimes();
    unordered_map<DWORD,ProcTimes> prevPT; prevPT.reserve(4096);
    string cmdline; // interactive command buffer

    auto nextFrame = chrono::steady_clock::now();

    while(true){
        // --- process keystrokes continuously between frames
        while(_kbhit()){
            int ch=_getch();
            if(ch==13){ // Enter => execute
                string line=trim(cmdline);
                if(line=="q"||line=="Q") { cout<<"\nSuccessfully exit!\n"; return 0; }
                else if(line=="h"||line=="H") cfg.showHelp = !cfg.showHelp;
                else if(line=="p"||line=="P") cfg.paused = !cfg.paused;
                else if(line=="+") cfg.delaySec = min(60, cfg.delaySec+1);
                else if(line=="-") cfg.delaySec = max(1, cfg.delaySec-1);
                else if(line=="x"||line=="X") { /* handled after render with current rows */ }
                else if(line=="/") { cfg.nameFilter.clear(); }
                else if(line.rfind("d ",0)==0){ string n=trim(line.substr(2)); if(!n.empty()) try{ cfg.delaySec=max(1,stoi(n)); }catch(...){} }
                else if(line.rfind("s ",0)==0){ string k=trim(line.substr(2)); if(k=="cpu"||k=="mem"||k=="pid") cfg.sortKey=k; }
                else if(line.rfind("k ",0)==0){ handleKill(line.substr(1)); }
                else if(line.rfind("/ ",0)==0 || (line.size()>1 && line[0]=='/' && line[1]!=' ')){
                    // support "/ foo" or "/foo"
                    string f = (line.size()>1 && line[1]!=' ') ? line.substr(1) : line.substr(2);
                    cfg.nameFilter = lower(trim(f));
                }
                // clear after exec
                cmdline.clear();
            } else if(ch==27){ // ESC
                cmdline.clear();
            } else if(ch==8){ // Backspace
                if(!cmdline.empty()) cmdline.pop_back();
            } else if(ch==9){ // Tab ignore
            } else if(ch==224){ // arrow prefix ignore next
                (void)_getch();
            } else if(ch=='+'){ cfg.delaySec = min(60, cfg.delaySec+1); }
            else if(ch=='-'){ cfg.delaySec = max(1, cfg.delaySec-1); }
            else if(ch=='h'||ch=='H'){ cfg.showHelp = !cfg.showHelp; }
            else if(ch=='p'||ch=='P'){ cfg.paused = !cfg.paused; }
            else if(ch=='x'||ch=='X'){ /* mark for export after we have rows */ cmdline="__export__"; }
            else { // printable
                if(ch>=32 && ch<127) cmdline.push_back((char)ch);
            }
        }

        // --- decide whether to draw a new frame
        auto now = chrono::steady_clock::now();
        if(now < nextFrame) { this_thread::sleep_for(chrono::milliseconds(15)); continue; }
        nextFrame = now + chrono::seconds(cfg.delaySec);

        // --- gather data (or freeze if paused)
        SysTimes curSys = prevSys;
        MemSummary mem{};
        vector<Row> rows;

        double sysCpuPct = 0.0;

        if(!cfg.paused){
            curSys = getSysTimes();
            ULONGLONG idleD  = (curSys.idle   >= prevSys.idle  ) ? (curSys.idle   - prevSys.idle  ) : 0;
            ULONGLONG kernD  = (curSys.kernel >= prevSys.kernel) ? (curSys.kernel - prevSys.kernel) : 0;
            ULONGLONG userD  = (curSys.user   >= prevSys.user  ) ? (curSys.user   - prevSys.user  ) : 0;
            ULONGLONG kernBusy = (kernD>idleD)? (kernD-idleD):0;
            ULONGLONG sysBusy = kernBusy + userD;
            if(sysBusy + idleD > 0) sysCpuPct = (double)sysBusy / (double)(sysBusy + idleD) * 100.0;

            mem = getMemSummary();

            vector<DWORD> pids = listPids();
            rows.reserve(pids.size());
            string filt = lower(cfg.nameFilter);

            for(DWORD pid: pids){
                if(pid==0) continue;
                HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ,FALSE,pid);
                if(!h) continue;
                ProcTimes pt{}; procTimes(h, pt);
                SIZE_T ws=0, priv=0; procMem(h, ws, priv);
                string name=procName(h);
                CloseHandle(h);

                // filter by name
                if(!filt.empty()){
                    string ln = lower(name);
                    if(ln.find(filt)==string::npos) continue;
                }

                // per-process CPU via deltas
                double pCpu=0.0;
                auto it=prevPT.find(pid);
                if(it!=prevPT.end()){
                    ULONGLONG dk=(pt.k>=it->second.k)? (pt.k-it->second.k):0;
                    ULONGLONG du=(pt.u>=it->second.u)? (pt.u-it->second.u):0;
                    ULONGLONG pD=dk+du;
                    ULONGLONG idleD  = (curSys.idle   >= prevSys.idle  ) ? (curSys.idle   - prevSys.idle  ) : 0;
                    ULONGLONG kernD2 = (curSys.kernel >= prevSys.kernel) ? (curSys.kernel - prevSys.kernel) : 0;
                    ULONGLONG userD2 = (curSys.user   >= prevSys.user  ) ? (curSys.user   - prevSys.user  ) : 0;
                    ULONGLONG kernBusy2 = (kernD2>idleD)? (kernD2-idleD):0;
                    ULONGLONG sysBusy2 = kernBusy2 + userD2;
                    if(sysBusy2 + idleD > 0) pCpu = (double)pD / (double)(sysBusy2 + idleD) * 100.0;
                }
                prevPT[pid]=pt;

                double mPct=(mem.totalPhys>0)? (double)ws/(double)mem.totalPhys*100.0 : 0.0;

                Row r; r.pid=pid; r.name=name; r.cpuPct=pCpu; r.memPct=mPct; r.ws=ws; r.priv=priv;
                rows.push_back(std::move(r));
            }

            // prune dead baselines
            if(prevPT.size()>rows.size()*2){
                unordered_map<DWORD,ProcTimes> alive; alive.reserve(rows.size());
                for(auto& r: rows) alive[r.pid]=prevPT[r.pid];
                prevPT.swap(alive);
            }

            // sort
            if(cfg.sortKey=="mem"){
                stable_sort(rows.begin(), rows.end(), [](const Row&a,const Row&b){
                    if(a.memPct==b.memPct) return a.pid<b.pid; return a.memPct>b.memPct; });
            } else if(cfg.sortKey=="pid"){
                stable_sort(rows.begin(), rows.end(), [](const Row&a,const Row&b){ return a.pid<b.pid; });
            } else {
                stable_sort(rows.begin(), rows.end(), [](const Row&a,const Row&b){
                    if(a.cpuPct==b.cpuPct) return a.pid<b.pid; return a.cpuPct>b.cpuPct; });
            }
            if(rows.size()>cfg.maxRows) rows.resize(cfg.maxRows);
        } else {
            // paused: still show memory totals and keep last rows empty (or previous would require caching)
            mem = getMemSummary();
        }

        // render
        cls();
        printHeader(sysCpuPct, mem);
        if(!rows.empty()) printRows(rows);
        if(cfg.showHelp) printHelp();
        printFooter(cfg, (cmdline=="__export__" ? "" : cmdline));

        // CSV export executes after we have rows on screen
        if(cmdline=="__export__"){
            if(rows.empty()){ cout<<"\n[INFO] Nothing to export (no rows after filter).\n"; }
            else exportCSV(rows);
            cmdline.clear();
        }

        // update baseline for next frame
        if(!cfg.paused) prevSys=curSys;
    }
}
