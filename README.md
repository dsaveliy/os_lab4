# Лабораторная работа: Процессы (Windows и Linux)

## Общее описание
Реализованы два консольных приложения: **Killer** и **User** для Windows и Linux.
**Killer** завершает процессы по PID (`--id`), по имени (`--name`) и по списку имён из переменной окружения `PROC_TO_KILL`, а **User** создаёт тестовый набор процессов, настраивает `PROC_TO_KILL`, запускает Killer с разными параметрами и демонстрирует корректность работы.

---

## Структура и функции
| Функция | Платформа | Назначение |
| :-- | :-- | :-- |
| `main` (Killer) | Win / Linux | Разбирает аргументы командной строки (`--id`, `--name`), читает `PROC_TO_KILL`, поочерёдно вызывает `KillProcessById`, `KillProcessesByName`, `KillFromEnvList` и формирует код возврата по суммарному результату |
| `GetEnvVar` | Win / Linux | Обёртка над `GetEnvironmentVariableA` / `std::getenv`, читает значение переменной |
| `KillProcessById` | Win / Linux | Завершает процесс по PID (`OpenProcess` + `TerminateProcess` на Windows или `kill(SIGTERM)` на Linux), печатает результат и возвращает `true` при успехе |
| `KillProcessesByName` | Win / Linux | Ищет процессы по имени (через snapshot `CreateToolhelp32Snapshot` на Windows или обход `/proc` на Linux), вызывает `KillProcessById` для каждого и ведёт счётчики найденных/завершённых процессов |
| `KillFromEnvList` | Win / Linux | Разбирает строку из `PROC_TO_KILL` в список имён, выводит его и по каждому имени вызывает `KillProcessesByName`, возвращая `true`, если был завершён хотя бы один процесс |
| `RunProcess` | Win / Linux | Универсальный запуск процесса: `CreateProcessA` на Windows и `fork` + `execvp` на Linux, опциональное ожидание завершения и возврат PID через доп. параметр |
| `StartProcess` | Win / Linux | Обёртка над `RunProcess` для запуска тестовых процессов, логирует запуск и возвращает PID либо 0 при ошибке |
| `RunKiller` | Win / Linux | Запускает Killer с нужной командной строкой, всегда ожидает его завершения и пишет сообщения о начале и конце работы |
| `ExistsProcessById` | Win / Linux | Проверяет существование процесса по PID (через snapshot и сравнение PID на Windows или `kill(pid, 0)`/`errno` на Linux) и выводит результат проверки |
| `CollectZombies` | Linux | Сбор «зомби»-процессов в User через `waitpid(-1, &status, WNOHANG)`, чтобы корректно очищать завершившихся детей |
| `main` (User) | Win / Linux | Формирует сценарий тестирования: создаёт процессы, выводит их состояние, вызывает Killer по имени и PID, затем через `PROC_TO_KILL`, и в конце очищает переменную окружения |

---

## Детали
1. **Отсутствие wide‑типов (`wchar_t`, `wstring`):** Для сохранения привычного вида типов и методов и для единообразия кода между Windows и Linux использую `char`, `std::string` и A‑версии (`CreateProcessA`, `SetEnvironmentVariableA` и т.п.)
2. **Возврат `true` при хотя бы одном успешном завершении:** Функции, работающие с несколькими процессами (`KillProcessesByName`, `KillFromEnvList`), возвращают `true`, если удалось завершить хотя бы один процесс, и `false`, если не завершён ни один.
3. **Вложенный вывод (`cout`) по глубине вызовов:** То есть при завершении по `PROC_TO_KILL` снаружи появляются сообщения `KillFromEnvList` (разбор переменной окружения и список имён), внутри — логи по каждому имени, и ещё глубже — логи по каждому PID.
4. **Работа с зомби‑процессами в Linux:** В линуксовском User реализована отдельная функция `CollectZombies`, которая собирает все завершившиеся дочерние процессы.

---

## Вывод
### Windows
<p align="center">
  <img width="30%" height="1029" alt="image" src="https://github.com/user-attachments/assets/ad9f3f23-1b62-47c4-96ce-502039b6d216" />
  <img width="30%" height="993" alt="image" src="https://github.com/user-attachments/assets/85a9e077-e967-45db-b25a-6c9d01f3d6d2" />
  <img width="30%" height="940" alt="image" src="https://github.com/user-attachments/assets/bfb0c700-1293-4bc5-a6f4-f4645558283c" />
</p>
*: На Windows видно, что при работе с chrome Killer находит и пытается завершить сразу много разных chrome.exe с разными PID, но часть попыток завершается ошибкой с кодом 5 - *Access is denied*, а часть процессов удаётся завершить успешно. 
Это связано с тем, что chrome сразу же запускает несколько разных процессов, причём большая часть из них не доступна для завершения из пользовательского процесса.​
Но все равно главные процессы остаются доступны пользователю (тут это 2 из 16) и в результате их удаления впоследствии удаляются и все остальные (причём, как видно из логов, 1 и 3 процессы, которыми я инициализировал запуск хрома, удалились сразу, а потом и 2-й). 

### Linux
<p align="center">
  <img width="40%" height="761" alt="image" src="https://github.com/user-attachments/assets/e95c1701-d451-47a1-ad78-85309c324a61" />
  <img width="40%" height="657" alt="image" src="https://github.com/user-attachments/assets/94e53d55-a0c4-48ae-940b-e0dd22dbaf37" />
</p>
