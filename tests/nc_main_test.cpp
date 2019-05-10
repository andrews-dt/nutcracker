#include <stdlib.h>
#include <iostream>

class LibMat
{
public:
    LibMat(int a)
    {
        std::cout << "LibMat() constructor" << std::endl;
    }

    ~LibMat()
    {
        std::cout << "LibMat() destructor"<< std::endl;
    }
};

class Book : public LibMat
{
public:
    Book(int a) : LibMat(a)
    {
        std::cout << "Book() constructor:" << a << std::endl;
    }

    ~Book()
    {
        std::cout << "Book() destructor"<< std::endl;
    }
};

class LibBook : public Book
{
public:
    LibBook() : Book(1)
    { }

    ~LibBook()
    {
        std::cout << "LibBook() destructor"<< std::endl;
    }
};

int main(int argc, char *argv[])
{
    LibBook b;
    return 0;
}