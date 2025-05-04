#include <iostream>

int main()
{
    int a = 42;
    int &a_ref = a;
    int b = 100;
    a_ref = b;
    std::cout << a_ref << std::endl;
}
