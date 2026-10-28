// SPDX-License-Identifier: MIT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
int wmain(int argc,wchar_t** argv){
    if(argc!=2||std::wstring(argv[1])!=L"--intentional-debug-fixture")return 2;
    auto missing=reinterpret_cast<volatile unsigned long*>(static_cast<ULONG_PTR>(0x123));
    *missing=7;return 0;
}
