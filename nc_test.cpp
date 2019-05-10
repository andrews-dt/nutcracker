#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include <nc_test.h>

std::vector<Test>* tests;

inline NcStatus::NcStatus(const NcStatus& rhs) 
{
    m_state_ = (rhs.m_state_ == nullptr) ? nullptr : copyState(rhs.m_state_);
}

inline NcStatus& NcStatus::operator=(const NcStatus& rhs) 
{
    if (m_state_ != rhs.m_state_) 
    {
        delete[] m_state_;
        m_state_ = (rhs.m_state_ == nullptr) ? nullptr : copyState(rhs.m_state_);
    }

    return *this;
}

inline NcStatus& NcStatus::operator=(NcStatus&& rhs) noexcept 
{
    std::swap(m_state_, rhs.m_state_);
    return *this;
}

bool NcTester::registerTest(const char* base, const char* name, void (*func)()) 
{
    if (tests == nullptr) 
    {
        tests = new std::vector<Test>;
    }
    Test t;
    t.base = base;
    t.name = name;
    t.func = func;
    tests->push_back(t);

    return true;
}

int NcTester::runAllTests() 
{
    const char* matcher = getenv("LEVELDB_TESTS");

    int num = 0;
    if (tests != nullptr) 
    {
        for (size_t i = 0; i < tests->size(); i++) 
        {
            const Test& t = (*tests)[i];
            if (matcher != nullptr) 
            {
                std::string name = t.base;
                name.push_back('.');
                name.append(t.name);
                if (strstr(name.c_str(), matcher) == nullptr) 
                {
                    continue;
                }
            }
            fprintf(stderr, "==== Test %s.%s\n", t.base, t.name);
            (*t.func)();
            ++num;
        }
    }
    fprintf(stderr, "==== PASSED %d tests\n", num);

    return 0;
}

int NcTester::randomSeed() 
{
    const char* env = getenv("TEST_RANDOM_SEED");
    int result = (env != nullptr ? atoi(env) : 301);
    if (result <= 0) 
    {
        result = 301;
    }

    return result;
}
