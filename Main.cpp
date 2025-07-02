// ---------- Standard Library Includes ----------

#include <format>
#include <iostream>
#include <stacktrace>
#include <stdexcept>

// ---------- Assertions ----------

#define Crash(msg) throw std::runtime_error{ std::format("[CRASH]: {}\n{}", msg, std::stacktrace::current()) };
#define Check(p) do { if (!(p)) Crash("Assertion failed: " #p); } while (false)

// ---------- Entry Point ----------

static void Entry()
{
    Check(false);
}

// ---------- Main ----------

int main()
{
    try
    {
        Entry();
    }
    catch (const std::runtime_error& e)
    {
        std::cout << e.what() << "\n";
    }

    return 0;
}
