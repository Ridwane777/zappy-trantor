#include "test_framework.hpp"

int main()
{
    std::cout << std::unitbuf;
    int failedCases = 0;
    for (const auto &c : test::registry()) {
        int before = test::failures();
        c.body();
        bool ok = test::failures() == before;
        failedCases += ok ? 0 : 1;
        std::cout << (ok ? "[OK]   " : "[FAIL] ") << c.name << "\n";
    }
    std::cout << test::registry().size() - failedCases << "/" << test::registry().size() << " unit tests passed\n";
    return failedCases ? 1 : 0;
}
