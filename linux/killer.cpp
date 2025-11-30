#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <cctype>

bool GetEnvVar(const char* name, std::string& value);
bool KillProcessById(pid_t pid);
bool KillProcessesByName(const std::string& name);
bool KillFromEnvList(const std::string& envValue);

int main(int argc, char* argv[])
{
    setlocale(LC_ALL, "Russian");

    pid_t pidValue = 0;
    std::string nameValue;
    std::string envValue;
    bool hasId = false;
    bool hasName = false;
    bool hasEnv = GetEnvVar("PROC_TO_KILL", envValue);

    if (argc >= 3)
    {
        std::string arg1 = argv[1];
        if (arg1 == "--id")
        {
            try
            {
                pidValue = static_cast<pid_t>(std::stol(argv[2]));
                hasId = true;
            }
            catch (...)
            {
                std::cerr << "[Killer] Некорректный PID: " << argv[2] << std::endl;
                return 1;
            }
        }
        else if (arg1 == "--name")
        {
            nameValue = argv[2];
            hasName = true;
        }
    }

    bool ok = true;

    if (hasId)
    {
        if (!KillProcessById(pidValue))
        {
            std::cerr << "[Killer] Не удалось завершить процесс по id " << pidValue << std::endl;
            ok = false;
        }
    }

    if (hasName)
    {
        if (!KillProcessesByName(nameValue))
        {
            std::cerr << "[Killer] Не удалось завершить процессы с именем " << nameValue << std::endl;
            ok = false;
        }
    }

    if (hasEnv)
    {
        if (!KillFromEnvList(envValue))
        {
            std::cerr << "[Killer] Не удалось завершить все процессы из PROC_TO_KILL" << std::endl;
            ok = false;
        }
    }

    if (!hasId && !hasName && !hasEnv)
    {
        return 0;
    }

    return ok ? 0 : 1;
}

bool GetEnvVar(const char* name, std::string& value)
{
    const char* v = std::getenv(name);
    if (!v)
    {
        value.clear();
        return false;
    }
    value = v;
    return true;
}

bool KillProcessById(pid_t pid)
{
    if (pid <= 0)
    {
        std::cout << "[Killer] Попытка удалить некорректный PID\n";
        return false;
    }

    // можно использовать и SIGKILL для более жётского убийства
    if (kill(pid, SIGTERM) == -1)
    {
        std::cout << "[Killer] Не удалось послать SIGTERM PID=" << pid
                  << ", ошибка: " << errno << "\n";
        return false;
    }

    std::cout << "[Killer] Отправлен SIGTERM процессу PID=" << pid << "\n";
    return true;
}

bool KillProcessesByName(const std::string& name)
{
    // открываем каталог процессов, в какой-то степени аналог снимка
    DIR* d = opendir("/proc");
    if (!d)
    {
        std::cerr << "[Killer] Не удалось открыть /proc\n";
        return false;
    }

    bool anyKilled = false;
    int foundCount = 0;
    int killedCount = 0;

    dirent* ent;
    while ((ent = readdir(d)) != nullptr)
    {
        if (!std::isdigit(static_cast<unsigned char>(ent->d_name[0]))) // проверяем на числа - это процессы
            continue;

        pid_t pid = static_cast<pid_t>(std::strtol(ent->d_name, nullptr, 10)); // забираем pid
        if (pid <= 0)
            continue;

        // проверяем внутри каждого процесса имя
        std::string commPath = std::string("/proc/") + ent->d_name + "/comm"; 
        std::ifstream comm(commPath);
        if (!comm.is_open())
            continue;

        std::string procName;
        std::getline(comm, procName);

        if (procName == name)
        {
            ++foundCount;
            std::cout << "[Killer] Найден процесс " << procName
                      << " (PID=" << pid << ")\n";

            if (KillProcessById(pid))
            {
                anyKilled = true;
                ++killedCount;
            }
            else
            {
                std::cout << "[Killer] (не удалось завершить процесс " << procName << ")\n";
            }
        }
    }

    closedir(d);

    if (foundCount == 0)
    {
        std::cout << "[Killer] Не найдено процессов с именем " << name << "\n";
    }
    else
    {
        std::cout << "[Killer] Итог: завершено " << killedCount
                  << " из " << foundCount << " процессов с именем " << name << "\n";
    }

    return anyKilled;
}

bool KillFromEnvList(const std::string& envValue)
{
    std::vector<std::string> names;
    std::string cur;
    for (char ch : envValue)
    {
        if (ch == ',' || ch == ';')
        {
            if (!cur.empty())
                names.push_back(cur);
            cur.clear();
        }
        else if (ch != ' ' && ch != '\t')
        {
            cur.push_back(ch);
        }
    }
    if (!cur.empty())
        names.push_back(cur);

    if (names.empty())
    {
        std::cout << "[Killer] В PROC_TO_KILL нет имён процессов\n";
        return false;
    }

    std::cout << "[Killer] Процессы из переменной окружения PROC_TO_KILL: ";
    for (size_t i = 0; i < names.size(); ++i)
    {
        std::cout << names[i];
        if (i + 1 < names.size())
            std::cout << ", ";
    }
    std::cout << "\n";

    bool anyKilled = false;
    for (const auto& n : names)
    {
        if (KillProcessesByName(n))
        {
            anyKilled = true;
        }
    }

    return anyKilled;
}

