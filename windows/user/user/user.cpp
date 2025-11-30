#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>

bool RunProcess(const std::string&, bool, DWORD* = nullptr);
DWORD StartProcess(const std::string&);
bool RunKiller(const std::string&);
bool ExistsProcessById(DWORD);

int main()
{
    setlocale(LC_ALL, "Russian");

    // запускаем процессы:
    // 3 chrome, 2 блокнота, 2 пэинта, 1 телеграм
    DWORD chrome1 = StartProcess("C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe");
    DWORD chrome2 = StartProcess("C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe");
    DWORD chrome3 = StartProcess("C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe");

    DWORD note1 = StartProcess("notepad.exe");
    DWORD note2 = StartProcess("notepad.exe");

    DWORD mspaint1 = StartProcess("mspaint.exe");
    DWORD mspaint2 = StartProcess("mspaint.exe");

    DWORD tg = StartProcess("C:\\Users\\Пользователь\\AppData\\Roaming\\Telegram Desktop\\Telegram.exe");

    std::string cmd;

    std::cout << "\n----- Состояние сразу после создания процессов -----\n";
    ExistsProcessById(chrome1);
    ExistsProcessById(chrome2);
    ExistsProcessById(chrome3);
    ExistsProcessById(note1);
    ExistsProcessById(note2);
    ExistsProcessById(mspaint1);
    ExistsProcessById(mspaint2);
    ExistsProcessById(tg);

    // убиваем блокноты по имени
    std::cout << "\n----- Убиваем все notepad.exe по имени -----\n";
    RunKiller("Killer.exe --name notepad.exe");

    // убиваем mspaint по PID
    std::cout << "\n----- Убиваем один mspaint.exe по PID (т.к. по PID, то второй остался) -----\n";

    cmd = "Killer.exe --id " + std::to_string(mspaint1);
    RunKiller(cmd);


    // проверяем актуальное состояние всех процессов
    std::cout << "\n----- Состояние после убийства notepad.exe (2 шт. по имени) и mspaint.exe(1 шт. по айди) -----\n";
    ExistsProcessById(chrome1);
    ExistsProcessById(chrome2);
    ExistsProcessById(chrome3);
    ExistsProcessById(note1);
    ExistsProcessById(note2);
    ExistsProcessById(mspaint1);
    ExistsProcessById(mspaint2);
    ExistsProcessById(tg);

    // все, кроме mspaint, добавляем в PROC_TO_KILL
    std::cout << "\n----- Устанавливаем PROC_TO_KILL для оставшихся процессов -----\n";
    SetEnvironmentVariableA("PROC_TO_KILL", "chrome, Telegram");
    std::cout << "[User] PROC_TO_KILL установлена\n";

    // убиваем один chrome по айди через Killer
    // а также автоматически добиваются и оставшиеся по PROC_TO_KILL (кроме ворда)
    std::cout << "\n----- Убиваем один chrome.exe по айди (остальные сами автоматом по PROC_TO_KILL) -----\n";
    cmd = "Killer.exe --id " + std::to_string(chrome1);
    RunKiller(cmd);

    // проверка существования всех процессов
    std::cout << "\n----- Состояние всех процессов после PROC_TO_KILL-----\n";
    ExistsProcessById(chrome1);
    ExistsProcessById(chrome2);
    ExistsProcessById(chrome3);
    ExistsProcessById(note1);
    ExistsProcessById(note2);
    ExistsProcessById(mspaint1);
    ExistsProcessById(mspaint2);
    ExistsProcessById(tg);

    // кладем mspaint в PROC_TO_KILL и делаем вызов killer.exe без параметров
    std::cout << "\n----- Устанавливаем PROC_TO_KILL для mspaint.exe -----\n";
    SetEnvironmentVariableA("PROC_TO_KILL", "mspaint");
    std::cout << "[User] PROC_TO_KILL установлена\n";
    RunKiller("Killer.exe");

    // финальное состояние всех процессов
    std::cout << "\n----- Финальное состояние всех процессов после PROC_TO_KILL без параметров-----\n";
    ExistsProcessById(chrome1);
    ExistsProcessById(chrome2);
    ExistsProcessById(chrome3);
    ExistsProcessById(note1);
    ExistsProcessById(note2);
    ExistsProcessById(mspaint1);
    ExistsProcessById(mspaint2);
    ExistsProcessById(tg);

    // чистим переменную окружения
    SetEnvironmentVariableA("PROC_TO_KILL", nullptr);
    std::cout << "\n[User] PROC_TO_KILL удалена\n";

    return 0;
}

// общий метод создания и для обычного начала процесса, и для киллера
// для удобства вводим флаг ожидания завершения (случай киллер) и возвращение id процесса
bool RunProcess(const std::string& cmdLine, bool wait, DWORD* pidOut = nullptr)
{
    STARTUPINFOA si{}; //структура для стартовой информации процесса
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{}; //структура для управления процессом

    std::string buf = cmdLine; // просто копирую строку

    BOOL ok = CreateProcessA(
        nullptr, // имя exe берём из командной строки
        buf.data(), // передаём именно char* на внутренность строки, т.к. нужно передавать изменяемый обьект
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    if (!ok) {
        std::cout << "[User] Ошибка CreateProcess для: " << cmdLine << ", код: " << GetLastError() << "\n";
        return false;
    }

    if (pidOut) {
        *pidOut = pi.dwProcessId;
    }

    if (wait) {
        WaitForSingleObject(pi.hProcess, INFINITE);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return true;
}

// для удобства при создании процесса возвращаю сразу его pid, а не bool, но поэтому значение 0 буду считать как False 
// и в дальнейшем обрабатывать как некорректно созданный процесс
DWORD StartProcess(const std::string& exeName)
{
    std::cout << "[User] Запуск процесса: " << exeName << "\n";
    DWORD pid = 0;
    if (!RunProcess(exeName, false, &pid)) {
        return 0;
    }
    std::cout << "[User] Процесс " << exeName << " создан, PID=" << pid << "\n";
    return pid;
}

bool RunKiller(const std::string& cmdLine)
{
    std::cout << "[User] Killer начался\n";
    bool ok = RunProcess(cmdLine, true, nullptr);
    std::cout << "[User] Killer завершился\n";
    return ok;
}


bool ExistsProcessById(DWORD pid)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        return false;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);

    bool exists = false;
    // логика аналогична KillProcessesByName, но сравниваем PID
    if (Process32First(hSnap, &pe)) {
        do {
            if (pe.th32ProcessID == pid) {
                exists = true;
                break;
            }
        } while (Process32Next(hSnap, &pe));
    }

    CloseHandle(hSnap);
    std::cout << "[User] Процесс с PID=" << pid << " найден с результатом " << exists << "\n";
    return exists;
}
