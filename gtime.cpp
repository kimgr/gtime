#include <windows.h>
#include <string>

/// Maintains wall, user and kernel times.
struct times_t
{
  times_t(double real, double user, double sys)
    : real(real)
    , user(user)
    , sys(sys)
  {
  }

  double real;
  double user;
  double sys;
};

/// Convert a FILETIME to a double.
double cast_time(const FILETIME& ft)
{
  UINT64 ft64 = (ft.dwLowDateTime | (UINT64)ft.dwHighDateTime << 32);
  return ((double)ft64 * 0.0000001);
}

/// Calculate times for process.
struct times_t get_times(HANDLE process)
{
  // Get process times
  FILETIME created = {0}, killed = {0}, sys, user;
  if (!GetProcessTimes(process,
    &created,
    &killed,
    &sys,
    &user))
  {
    wprintf(L"gtime: Failed to get process times, error: %d\n", GetLastError());
    exit(1);
  }

  // If we're asking for the current process,
  // it will never have exited, and we force real to zero.
  if (process == GetCurrentProcess())
    killed = created;

  // Calculate wall time
  double real = cast_time(killed) - cast_time(created);
  return times_t(real, cast_time(user), cast_time(sys));
}

/// Build a string forming the command to be timed.
std::wstring build_child_command(int argc, wchar_t* argv[])
{
  std::wstring buffer;
  for (int i = 1; i < argc; ++i)
  {
    if (!buffer.empty())
      buffer += L" ";

    buffer += argv[i];
  }

  return buffer;
}

/// Entry point.
int wmain(int argc, wchar_t* argv[])
{
  // Default to the gtime process itself.
  HANDLE process = GetCurrentProcess();
  int exit_code = 0;

  std::wstring child_command = build_child_command(argc, argv);
  if (!child_command.empty())
  {
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    if (!CreateProcessW(NULL, &child_command[0], NULL, NULL,
                        TRUE, 0, NULL, NULL, &si, &pi))
    {
      wprintf(L"gtime: Failed to launch child command, error: %d\n",
              GetLastError());
      exit(1);
    }

    ::WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);

    process = pi.hProcess;
  }

  times_t times = get_times(process);

  if (process != GetCurrentProcess())
  {
    unsigned long process_exit_code = 0;
    GetExitCodeProcess(process, &process_exit_code);
    exit_code = process_exit_code;
    CloseHandle(process);
  }

  wprintf(L"real    %.3fs\n", times.real);
  wprintf(L"user    %.3fs\n", times.user);
  wprintf(L"sys     %.3fs\n", times.sys);

  return exit_code;
}

