#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <signal.h>
#include <errno.h>
#include <string>
#include <cstdlib>
#include <vector>
#include <iostream>
#include <cstring>

bool RunProcess(const std::string& cmdLine, bool wait, pid_t* pidOut = nullptr);
pid_t StartProcess(const std::string& exeAndArgs);
bool RunKiller(const std::string& cmdLine);
bool ExistsProcessById(pid_t pid);
void CollectZombies();

int main()
{
    setlocale(LC_ALL, "Russian");

    // 3 процесса gedit и 2 процесса xcalc
    pid_t g1 = StartProcess("gedit");
    pid_t g2 = StartProcess("gedit");
    pid_t g3 = StartProcess("gedit");

    pid_t x1 = StartProcess("xcalc");
    pid_t x2 = StartProcess("xcalc");

    std::cout << "\n----- Состояние сразу после создания процессов -----\n";
    CollectZombies();
    ExistsProcessById(g1);
    ExistsProcessById(g2);
    ExistsProcessById(g3);
    ExistsProcessById(x1);
    ExistsProcessById(x2);

    // убиваем все gedit по имени
    std::cout << "\n----- Убиваем все gedit по имени -----\n";
    RunKiller("./killer --name gedit");

    // убиваем один xcalc по PID (второй останется)
    std::cout << "\n----- Убиваем один xcalc по PID (второй останется) -----\n";
    std::string cmd = std::string("./killer --id ") + std::to_string(x1);
    RunKiller(cmd);

    std::cout << "\n----- Состояние после убийства gedit (по имени) и одного xcalc (по PID) -----\n";
    CollectZombies();
    ExistsProcessById(g1);
    ExistsProcessById(g2);
    ExistsProcessById(g3);
    ExistsProcessById(x1);
    ExistsProcessById(x2);

    // кладем xcalc в PROC_TO_KILL и делаем вызов killer без параметров
    std::cout << "\n----- Устанавливаем PROC_TO_KILL для xcalc -----\n";
    setenv("PROC_TO_KILL", "xcalc", 1);
    std::cout << "[User] PROC_TO_KILL установлена\n";
    RunKiller("./killer");

    std::cout << "\n----- Финальное состояние всех процессов -----\n";
    CollectZombies();
    ExistsProcessById(g1);
    ExistsProcessById(g2);
    ExistsProcessById(g3);
    ExistsProcessById(x1);
    ExistsProcessById(x2);

    unsetenv("PROC_TO_KILL");
    std::cout << "\n[User] PROC_TO_KILL удалена\n";

    return 0;
}

bool RunProcess(const std::string& cmdLine, bool wait, pid_t* pidOut)
{
    // создаём строку нужного формата для линукса, т.к. в отличии от винды нужно передать
    // массив укзателей charов с nullptr в конце
    std::vector<char*> args;
    std::string copy = cmdLine + '\0';
    char* cstr = const_cast<char*>(copy.c_str());
    char* token = strtok(cstr, " ");
    while (token)
    {
        args.push_back(token);
        token = strtok(nullptr, " ");
    }

    if (args.empty())
    {
        return false;
    }
    args.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0)
    {
        return false;
    }

    // выполняется в ребенке
    if (pid == 0)
    {
        execvp(args[0], args.data()); // запускаем программу
        std::cerr << "[User] Ошибка execvp для " << args[0]
                  << ": " << strerror(errno) << "\n";
        _exit(1);
    }

    // выполянется в родителе
    if (pidOut)
    {
        *pidOut = pid; // переписываем pid ребенка
    }

    if (wait)
    {
        int status = 0;
        waitpid(pid, &status, 0);
    }

    return true;
}

pid_t StartProcess(const std::string& exeAndArgs)
{
    std::cout << "[User] Запуск процесса: " << exeAndArgs << "\n";
    pid_t pid = 0;
    if (!RunProcess(exeAndArgs, false, &pid))
    {
        return 0;
    }
    std::cout << "[User] Процесс " << exeAndArgs << " создан, PID=" << pid << "\n";
    return pid;
}

bool RunKiller(const std::string& cmdLine)
{
    std::cout << "[User] Killer начался\n";
    bool ok = RunProcess(cmdLine, true, nullptr);
    std::cout << "[User] Killer завершился\n";
    return ok;
}

bool ExistsProcessById(pid_t pid)
{
    bool exists = false;
    errno = 0;
    if (kill(pid, 0) == 0)
    {
        exists = true;
    }
    else if (errno == EPERM)
    {
        // процесс есть, но нет прав послать сигнал
        exists = true;
    }
    else
    {
        exists = false;
    }

    std::cout << "[User] Процесс с PID=" << pid
              << " найден с результатом " << exists << "\n";
    return exists;
}

// сбор зомби-процессов, т.е. тех, которые завершились, но не убраны
void CollectZombies()
{
    int status = 0;
    while (true) {
        pid_t pid = waitpid(-1, &status, WNOHANG); // -1 - любой ребенок, WNOHANG - не блокироваться, если зомби нет
        if (pid > 0) {
            std::cout << "[User] Собран зомби-процесс PID=" << pid << "\n";
            continue; // пробуем, нет ли еще
        }
        // pid == 0 - нет завершившихся детей сейчас, pid < 0  - детей нет вообще или ошибка
        break;
    }
}
