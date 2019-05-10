#include <nc_test.h>

TEST(Status, TEST1) 
{
    ASSERT_OK(1==1);
    ASSERT_OK("aa"=="bb");
    ASSERT_EQ("abc", "abc");
    ASSERT_EQ("abc1", "abc2");
}

TEST(Status, TEST2) 
{
    ASSERT_OK(1==1);
    ASSERT_OK("aa"=="bb");
    ASSERT_EQ("abc", "abc");
    ASSERT_EQ("abc1", "abc2");
}

int main(int argc, char** argv) 
{
    return Tester::RunAllTests();
}