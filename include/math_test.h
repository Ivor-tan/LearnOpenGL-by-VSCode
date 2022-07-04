
#ifndef MATH_TEST_H
#define MATH_TEST_H
#include <iostream>
#include <string>
using namespace std;

void test(int i);

template <class T>
int getArrSize(T &arr);
#endif
void test(int i)
{
    cout << "Hello World"; // 输出 Hello World
}

template <class T>
int getArrSize(T &arr)
{
    return sizeof(arr) / sizeof(arr[0]);
}