#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdio>     // popen(), pclose()
#include <cstdlib>    // atoi()
#include <iomanip>    // setw(), fixed, setprecision()
#include <algorithm>  // sort()

// "Modelo" de un proceso
struct Process {
    int pid = 0;               
    std::string user;          
    std::string state;         
    long rss_kb = 0;           // RSS = memoria residente (KB)
    std::string command;       // Nombre del ejecutable
};

// Quita espacios al inicio (ps alinea columnas y deja espacios al principio)
static std::string ltrim(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
    return s.substr(i);
}


// popen abre un "pipe": como si el comando fuera un archivo del que puedes leer.
static std::vector<std::string> run_command_lines(const std::string& cmd) {
    std::vector<std::string> lines;

    FILE* pipe = popen(cmd.c_str(), "r"); 
    if (!pipe) return lines;              // si falla, regresa vacío

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) { // lee línea por línea
        std::string line(buffer);
        if (!line.empty() && line.back() == '\n') line.pop_back(); 
        lines.push_back(line);
    }

    pclose(pipe); // aquí cierro el pipe
    return lines;
}

// Obtiene todos los procesos del sistema usando "ps" y los convierte a vector<Process>
static std::vector<Process> getProcesses() {
    std::vector<Process> processes;

 
    auto lines = run_command_lines("ps -axo pid=,user=,state=,rss=,comm=");

    for (const auto& raw : lines) {
        std::string line = ltrim(raw);
        if (line.empty()) continue;

        std::istringstream iss(line);
        Process p;

        // Parseamos 5 columnas exactas: pid user state rss comm
        if (!(iss >> p.pid >> p.user >> p.state >> p.rss_kb >> p.command)) {
            continue;
        }

        processes.push_back(p);
    }

    return processes;
}

// Imprime el encabezado para que se vea como tablita
static void printHeader() {
    std::cout << std::left
              << std::setw(8)  << "PID"
              << std::setw(14) << "USER"
              << std::setw(8)  << "STATE"
              << std::setw(10) << "RSS_MB"
              << "COMMAND\n";

    std::cout << std::string(60, '-') << "\n";
}

// Lista procesos ordenados por memoria (Top 25)
static void listProcesses() {
    auto processes = getProcesses();

    // Ordenamos DESC por rss_kb (los que más RAM usan primero)
    std::sort(processes.begin(), processes.end(),
              [](const Process& a, const Process& b) {
                  return a.rss_kb > b.rss_kb;
              });

    printHeader();

    int count = 0;
    for (const auto& p : processes) {
        if (count == 25) break;

        // Convertimos KB -> MB para que sea más entendible
        double rss_mb = p.rss_kb / 1024.0;

        std::cout << std::left
                  << std::setw(8)  << p.pid
                  << std::setw(14) << p.user
                  << std::setw(8)  << p.state
                  << std::setw(10) << std::fixed << std::setprecision(1) << rss_mb
                  << p.command
                  << "\n";
        count++;
    }
}

// Busca procesos cuyo nombre (command) contenga cierto texto
static void findProcess(const std::string& needle) {
    auto processes = getProcesses();
    std::vector<Process> filtered;

    for (const auto& p : processes) {
        if (p.command.find(needle) != std::string::npos) {
            filtered.push_back(p);
        }
    }

    // se muestran los más pesados
    std::sort(filtered.begin(), filtered.end(),
              [](const Process& a, const Process& b) {
                  return a.rss_kb > b.rss_kb;
              });

    if (filtered.empty()) {
        std::cout << "No encontré procesos que contengan: " << needle << "\n";
        return;
    }

    printHeader();

    for (const auto& p : filtered) {
        double rss_mb = p.rss_kb / 1024.0;

        std::cout << std::left
                  << std::setw(8)  << p.pid
                  << std::setw(14) << p.user
                  << std::setw(8)  << p.state
                  << std::setw(10) << std::fixed << std::setprecision(1) << rss_mb
                  << p.command
                  << "\n";
    }
}

// Muestra detalles de un proceso específico (por PID)
static void infoProcess(int pid) {

    std::string cmd = "ps -p " + std::to_string(pid) +
                      " -o pid=,user=,state=,rss=,etime=,command=";

    auto lines = run_command_lines(cmd);

    // Si no sale nada: no existe o no tienes permisos
    if (lines.empty()) {
        std::cout << "No pude leer el PID " << pid
                  << " (puede que no exista o no tengas permisos).\n";
        return;
    }

    std::string line = ltrim(lines[0]);
    std::istringstream iss(line);

    int out_pid = 0;
    std::string user, state, etime;
    long rss_kb = 0;

    // Leemos las columnas "seguras"
    if (!(iss >> out_pid >> user >> state >> rss_kb >> etime)) {
        std::cout << "No pude parsear la info del PID.\n";
        return;
    }

    // Todo lo demás (con espacios) es el comando completo
    std::string rest;
    std::getline(iss, rest);
    rest = ltrim(rest);

    std::cout << "PID:     " << out_pid << "\n";
    std::cout << "USER:    " << user << "\n";
    std::cout << "STATE:   " << state << "\n";
    std::cout << "RSS:     " << std::fixed << std::setprecision(1) << (rss_kb / 1024.0) << " MB\n";
    std::cout << "ETIME:   " << etime << " (tiempo corriendo)\n";
    std::cout << "COMMAND: " << rest << "\n";
}

static void usage() {
    std::cout <<
        "Uso:\n"
        "  ./procwatch list\n"
        "  ./procwatch find <texto>\n"
        "  ./procwatch info <pid>\n\n"
        "Ejemplos:\n"
        "  ./procwatch list\n"
        "  ./procwatch find Safari\n"
        "  ./procwatch info 1\n";
}

int main(int argc, char* argv[]) {
    // argc = cuántos argumentos llegaron, argv = los argumentos como strings

    if (argc < 2) {
        usage();
        return 1;
    }

    std::string cmd = argv[1];

    // Modo 1: listar top procesos por memoria
    if (cmd == "list") {
        listProcesses();
        return 0;
    }

    // Modo 2: buscar por texto
    if (cmd == "find") {
        if (argc < 3) { usage(); return 1; } // falta el texto a buscar
        findProcess(argv[2]);
        return 0;
    }

    // Modo 3: info detallada por PID
    if (cmd == "info") {
        if (argc < 3) { usage(); return 1; } // falta el pid
        int pid = std::atoi(argv[2]);         // string -> int
        if (pid <= 0) {
            std::cout << "PID inválido.\n";
            return 1;
        }
        infoProcess(pid);
        return 0;
    }

    // Si el usuario puso un comando desconocido
    usage();
    return 1;
}

// g++ main.cpp -o procwatch
// ./procwatch list
// ./procwatch find Safari
// ./procwatch info 1