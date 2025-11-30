#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <iostream>
#include <string>

bool GetEnvVar(const char*, std::string&);
bool KillProcessById(DWORD);
bool KillProcessesByName(const std::string&);
bool KillFromEnvList(const std::string&);

int main(int argc, char* argv[])
{
    setlocale(LC_ALL, "Russian");
    DWORD pidValue = 0;
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
                pidValue = static_cast<DWORD>(std::stoul(argv[2]));
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

bool GetEnvVar(const char* name, std::string& value) {
    DWORD needed = GetEnvironmentVariableA(name, nullptr, 0); // получение размера
    if (needed == 0) {
        value.clear();
        return false;
    }

    std::string buffer(needed, '\0');
    GetEnvironmentVariableA(name, buffer.data(), needed); // кладем знаечние в буфер

    if (!buffer.empty() && buffer.back() == '\0')
        buffer.pop_back();

    value = std::move(buffer);
    return true;
}

bool KillProcessById(DWORD pid)
{
    if (pid == 0) {
        std::cout << "[Killer] Попытка удалить некорректно созданный процесс\n";
        return false;
    }
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid); // получаем дескриптор для обработки процесса
    if (!hProcess) {
        std::cout << "[Killer] Не удалось открыть процесс PID=" << pid << "\n";
        return false;
    }
    BOOL ok = TerminateProcess(hProcess, 0); // строго завершаем с кодом 0
    CloseHandle(hProcess); 
    if (ok) {
        std::cout << "[Killer] Успешнро завершён процесс PID=" << pid << "\n";
    }
    else {
        std::cout << "[Killer] Не удалось завершить процесс PID=" << pid << ", ошибка: " << GetLastError() << "\n";
    }
    return ok != 0; // что правильно вернуть тип bool, ибо логика bool и BOOL под капотом чуть отличается
}

bool KillProcessesByName(const std::string& name)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); // делаем "снимок" всех процессов в моменте
    if (hSnap == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32 pe; // структура для информации о процессе (будем поочередно использовать для каждого из процессов)
    pe.dwSize = sizeof(pe);
    bool anyKilled = false;
    int foundCount = 0;      // сколько процессов с таким именем нашли
    int killedCount = 0;     // сколько из них реально завершили

    if (Process32First(hSnap, &pe)) { // кладём в pe первый процесс и при наличии хотя бы одного заходим в цикл
        do { // далее циклом будем класть все остальные
            if (_stricmp(pe.szExeFile, name.c_str()) == 0) { // циклом сравниваем все процессы из снимка с искомым
                ++foundCount;
                std::cout << "[Killer] По имени " << name << " найден процесс " << pe.szExeFile
                    << " (PID=" << pe.th32ProcessID << ")\n";
                if (KillProcessById(pe.th32ProcessID)) {
                    std::cout << "[Killer] (успешно завершён процесс с названием " << pe.szExeFile << ")\n";
                    anyKilled = true;
                    ++killedCount;
                }
                else {
                    std::cout << "[Killer] (не удалось завершить процесс с названием " << pe.szExeFile << ")\n";
                }
            }

        } while (Process32Next(hSnap, &pe));
    }

    if (foundCount == 0) {
        std::cout << "[Killer] Не найдено процессов с именем " << name << "\n";
    }
    else {
        std::cout << "[Killer] Итог: завершено " << killedCount
            << " из " << foundCount << " процессов с именем " << name << "\n";
    }

    CloseHandle(hSnap);
    return anyKilled;
}

bool KillFromEnvList(const std::string& envValue)
{
    // сплитуем строку (упрощенный подход считая ввод корректным)
    std::vector<std::string> names;
    std::string cur;
    for (char ch : envValue) {
        if (ch == ',' || ch == ';') {
            if (!cur.empty())
                names.push_back(cur);
            cur.clear();
        }
        else if (ch != ' ' && ch != '\t') {
            cur.push_back(ch);
        }
    }
    if (!cur.empty())
        names.push_back(cur);

    if (names.empty()) {
        std::cout << "[Killer] В PROC_TO_KILL нет имён процессов\n";
        return false;
    }

    std::cout << "[Killer] Процессы из переменной окружения PROC_TO_KILL: ";
    for (size_t i = 0; i < names.size(); ++i) {
        std::cout << names[i];
        if (i + 1 < names.size())
            std::cout << ", ";
    }
    std::cout << "\n";

    bool anyKilled = false;
    for (const auto& n : names) {
        if (KillProcessesByName(n + ".exe")) {
            anyKilled = true;
        }
    }

    return anyKilled;
}
