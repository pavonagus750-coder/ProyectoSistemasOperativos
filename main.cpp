#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdio>      // popen(), pclose()
#include <cstdlib>     // atoi()
#include <iomanip>     // setw(), fixed, setprecision()
#include <algorithm>   // sort()
#include <csignal>     // kill, SIGTERM, SIGKILL
#include <unistd.h>    // sleep
#include <ctime>       // time, ctime

using namespace std;

// =========================
// COLORES ANSI
// =========================
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"

// =========================
// MODELO DE PROCESO
// =========================
struct Process {
    int pid = 0;
    string user;
    string state;
    long rss_kb = 0;      
    string command;
};

// Quita espacios al inicio
static string ltrim(const string& s) {
    size_t i = 0;
    while (i < s.size() && isspace(static_cast<unsigned char>(s[i]))) {
        i++;
    }
    return s.substr(i);
}

// Ejecuta un comando y regresa sus líneas
static vector<string> run_command_lines(const string& cmd) {
    vector<string> lines;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return lines;

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        string line(buffer);
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        lines.push_back(line);
    }

    pclose(pipe);
    return lines;
}

// Regresa color según el estado del proceso
static string getStateColor(const string& state) {
    if (state.find('R') != string::npos) return GREEN;   // Running
    if (state.find('S') != string::npos) return CYAN;    // Sleeping
    if (state.find('T') != string::npos) return YELLOW;  // Stopped
    if (state.find('Z') != string::npos) return RED;     // Zombie
    return WHITE;
}

// Banner 
static void printBanner() {
    cout << BOLD << BLUE;
    cout << "=============================================\n";
    cout << "               PROCWATCH                     \n";
    cout << "     Monitor de procesos para Unix/macOS    \n";
    cout << "=============================================\n";
    cout << RESET << "\n";
}

// Imprime fecha/hora actual
static void printTimestamp() {
    time_t now = time(nullptr);
    char* dt = ctime(&now);

    if (dt != nullptr) {
        cout << BOLD << MAGENTA << "Actualizado: " << RESET << dt;
    }
}


// OBTENER PROCESOS
static vector<Process> getProcesses() {
    vector<Process> processes;

    auto lines = run_command_lines("ps -axo pid=,user=,state=,rss=,comm=");

    for (const auto& raw : lines) {
        string line = ltrim(raw);
        if (line.empty()) continue;

        istringstream iss(line);
        Process p;

        if (!(iss >> p.pid >> p.user >> p.state >> p.rss_kb >> p.command)) {
            continue;
        }

        processes.push_back(p);
    }

    return processes;
}


// IMPRESIÓN
static void printHeader() {
    cout << BOLD << CYAN
         << left
         << setw(8)  << "PID"
         << setw(16) << "USER"
         << setw(10) << "STATE"
         << setw(12) << "RSS_MB"
         << "COMMAND\n"
         << RESET;

    cout << string(70, '-') << "\n";
}

// =========================
// FUNCIONALIDADES
// =========================

// Lista procesos ordenados por memoria
static void listProcesses() {
    auto processes = getProcesses();

    sort(processes.begin(), processes.end(),
         [](const Process& a, const Process& b) {
             return a.rss_kb > b.rss_kb;
         });

    printBanner();
    cout << BOLD << "Top 25 procesos por uso de memoria\n" << RESET;
    cout << "Total de procesos detectados: " << processes.size() << "\n\n";

    printHeader();

    int count = 0;
    for (const auto& p : processes) {
        if (count == 25) break;

        double rss_mb = p.rss_kb / 1024.0;
        string color = getStateColor(p.state);

        cout << color
             << left
             << setw(8)  << p.pid
             << setw(16) << p.user
             << setw(10) << p.state
             << setw(12) << fixed << setprecision(1) << rss_mb
             << p.command
             << RESET << "\n";

        count++;
    }

    cout << "\n";
    cout << GREEN  << "R = Running   " << RESET
         << CYAN   << "S = Sleeping   " << RESET
         << YELLOW << "T = Stopped   " << RESET
         << RED    << "Z = Zombie" << RESET << "\n";
}

// Busca procesos por texto
static void findProcess(const string& needle) {
    auto processes = getProcesses();
    vector<Process> filtered;

    for (const auto& p : processes) {
        if (p.command.find(needle) != string::npos) {
            filtered.push_back(p);
        }
    }

    sort(filtered.begin(), filtered.end(),
         [](const Process& a, const Process& b) {
             return a.rss_kb > b.rss_kb;
         });

    printBanner();
    cout << BOLD << "Búsqueda de procesos que contienen: " 
         << YELLOW << needle << RESET << "\n\n";

    if (filtered.empty()) {
        cout << RED << "No encontré procesos que contengan: "
             << needle << RESET << "\n";
        return;
    }

    cout << "Coincidencias encontradas: " << filtered.size() << "\n\n";
    printHeader();

    for (const auto& p : filtered) {
        double rss_mb = p.rss_kb / 1024.0;
        string color = getStateColor(p.state);

        cout << color
             << left
             << setw(8)  << p.pid
             << setw(16) << p.user
             << setw(10) << p.state
             << setw(12) << fixed << setprecision(1) << rss_mb
             << p.command
             << RESET << "\n";
    }
}

// Muestra detalles de un proceso específico
static void infoProcess(int pid) {
    string cmd = "ps -p " + to_string(pid) +
                 " -o pid=,user=,state=,rss=,etime=,command=";

    auto lines = run_command_lines(cmd);

    printBanner();
    cout << BOLD << "Información detallada del proceso\n\n" << RESET;

    if (lines.empty()) {
        cout << RED << "No pude leer el PID " << pid
             << " (puede que no exista o no tengas permisos)." 
             << RESET << "\n";
        return;
    }

    string line = ltrim(lines[0]);
    istringstream iss(line);

    int out_pid = 0;
    string user, state, etime;
    long rss_kb = 0;

    if (!(iss >> out_pid >> user >> state >> rss_kb >> etime)) {
        cout << RED << "No pude interpretar la información del PID." 
             << RESET << "\n";
        return;
    }

    string rest;
    getline(iss, rest);
    rest = ltrim(rest);

    string color = getStateColor(state);

    cout << BOLD << "PID:     " << RESET << out_pid << "\n";
    cout << BOLD << "USER:    " << RESET << user << "\n";
    cout << BOLD << "STATE:   " << RESET << color << state << RESET << "\n";
    cout << BOLD << "RSS:     " << RESET << fixed << setprecision(1)
         << (rss_kb / 1024.0) << " MB\n";
    cout << BOLD << "ETIME:   " << RESET << etime << " (tiempo corriendo)\n";
    cout << BOLD << "COMMAND: " << RESET << rest << "\n";
}

// NUEVO DE LA SEGUNDA ENTREGA
static void killProcess(int pid, bool force = false) {
    int signal_to_send = force ? SIGKILL : SIGTERM;

    printBanner();

    if (kill(pid, signal_to_send) == 0) {
        cout << GREEN << BOLD
             << "Señal enviada correctamente al PID " << pid
             << (force ? " (SIGKILL)" : " (SIGTERM)")
             << RESET << "\n";
    } else {
        cout << RED << BOLD;
        perror("Error al enviar la señal");
        cout << RESET;
    }
}

// NUEVO DE LA SEGUNDA ENTREGA
static void watchProcesses(int seconds = 2) {
    while (true) {
        cout << "\033[2J\033[H"; // limpia pantalla
        printBanner();
        cout << BOLD << MAGENTA
             << "Modo monitoreo en tiempo real\n"
             << RESET;
        cout << "Actualización cada " << seconds << " segundos\n";
        printTimestamp();
        cout << "\n";

        auto processes = getProcesses();

        sort(processes.begin(), processes.end(),
             [](const Process& a, const Process& b) {
                 return a.rss_kb > b.rss_kb;
             });

        cout << "Total de procesos detectados: " << processes.size() << "\n\n";
        printHeader();

        int count = 0;
        for (const auto& p : processes) {
            if (count == 25) break;

            double rss_mb = p.rss_kb / 1024.0;
            string color = getStateColor(p.state);

            cout << color
                 << left
                 << setw(8)  << p.pid
                 << setw(16) << p.user
                 << setw(10) << p.state
                 << setw(12) << fixed << setprecision(1) << rss_mb
                 << p.command
                 << RESET << "\n";

            count++;
        }

        cout << "\n";
        cout << YELLOW << "Presiona Ctrl + C para salir." << RESET << "\n";

        sleep(seconds);
    }
}


// AYUDA
static void usage() {
    printBanner();

    cout << BOLD << "Uso:\n" << RESET
         << "  ./procwatch list\n"
         << "  ./procwatch find <texto>\n"
         << "  ./procwatch info <pid>\n"
         << "  ./procwatch watch\n"
         << "  ./procwatch kill <pid>\n"
         << "  ./procwatch kill -9 <pid>\n\n";

    cout << BOLD << "Ejemplos:\n" << RESET
         << "  ./procwatch list\n"
         << "  ./procwatch find Safari\n"
         << "  ./procwatch info 1\n"
         << "  ./procwatch watch\n"
         << "  ./procwatch kill 1234\n"
         << "  ./procwatch kill -9 1234\n\n";

    cout << BOLD << "Leyenda de colores:\n" << RESET
         << GREEN  << "  Running\n"  << RESET
         << CYAN   << "  Sleeping\n" << RESET
         << YELLOW << "  Stopped\n"  << RESET
         << RED    << "  Zombie / Error\n" << RESET;
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage();
        return 1;
    }

    string cmd = argv[1];

    // Modo 1: listar top procesos por memoria
    if (cmd == "list") {
        listProcesses();
        return 0;
    }

    // Modo 2: buscar por texto
    if (cmd == "find") {
        if (argc < 3) {
            usage();
            return 1;
        }
        findProcess(argv[2]);
        return 0;
    }

    // Modo 3: info detallada por PID
    if (cmd == "info") {
        if (argc < 3) {
            usage();
            return 1;
        }

        int pid = atoi(argv[2]);
        if (pid <= 0) {
            cout << RED << "PID inválido.\n" << RESET;
            return 1;
        }

        infoProcess(pid);
        return 0;
    }

    // Modo 4: monitoreo en tiempo real
    if (cmd == "watch") {
        watchProcesses();
        return 0;
    }

    // Modo 5: matar proceso
    if (cmd == "kill") {
        if (argc < 3) {
            usage();
            return 1;
        }

        // Caso: kill -9 <pid>
        if (string(argv[2]) == "-9") {
            if (argc < 4) {
                usage();
                return 1;
            }

            int pid = atoi(argv[3]);
            if (pid <= 0) {
                cout << RED << "PID inválido.\n" << RESET;
                return 1;
            }

            killProcess(pid, true);
            return 0;
        }

        // Caso normal: kill <pid>
        int pid = atoi(argv[2]);
        if (pid <= 0) {
            cout << RED << "PID inválido.\n" << RESET;
            return 1;
        }

        killProcess(pid, false);
        return 0;
    }

    usage();
    return 1;
}

// Compilar:
// g++ main.cpp -o procwatch

// Ejemplos para usarse:
// ./procwatch list
// ./procwatch find Safari
// ./procwatch info 1
// ./procwatch watch
// ./procwatch kill 1234
// ./procwatch kill -9 1234