// SPDX-License-Identifier: MIT
// Developer-only debugger. Launches its own isolated child; never attaches.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
namespace fs=std::filesystem;
namespace {
bool within(const fs::path& path,const fs::path& root){auto p=fs::weakly_canonical(path).wstring(),r=fs::weakly_canonical(root).wstring()+L"\\";return p.size()>r.size()&&_wcsnicmp(p.c_str(),r.c_str(),r.size())==0;}
std::string hex(std::uint64_t value){std::ostringstream out;out<<"0x"<<std::hex<<value;return out.str();}
std::string pathOf(HANDLE file){wchar_t buffer[32768];const auto n=GetFinalPathNameByHandleW(file,buffer,32768,FILE_NAME_NORMALIZED);if(!n||n>=32768)return {};std::wstring path(buffer,n);if(path.rfind(L"\\\\?\\",0)==0)path.erase(0,4);const auto bytes=WideCharToMultiByte(CP_UTF8,0,path.data(),int(path.size()),nullptr,0,nullptr,nullptr);std::string result(static_cast<std::size_t>(bytes),'\0');WideCharToMultiByte(CP_UTF8,0,path.data(),int(path.size()),result.data(),bytes,nullptr,nullptr);return result;}
void exceptionEvidence(HANDLE process,const DEBUG_EVENT& event,const fs::path& out,unsigned number){
    const auto prefix=out/("exception-"+std::to_string(number));std::ofstream log(prefix.string()+".txt");
    const auto& record=event.u.Exception.ExceptionRecord;
    log<<"code="<<hex(record.ExceptionCode)<<" firstChance="<<event.u.Exception.dwFirstChance<<" thread="<<event.dwThreadId<<" address="<<record.ExceptionAddress<<'\n';
    HANDLE thread=OpenThread(THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,event.dwThreadId);CONTEXT context{};context.ContextFlags=CONTEXT_ALL;
    if(!thread||!GetThreadContext(thread,&context)){log<<"contextError="<<GetLastError()<<'\n';if(thread)CloseHandle(thread);return;}
    log<<"rip="<<hex(context.Rip)<<" rsp="<<hex(context.Rsp)<<" rcx="<<hex(context.Rcx)<<" rdx="<<hex(context.Rdx)<<" r8="<<hex(context.R8)<<" r9="<<hex(context.R9)<<'\n';
    IMAGEHLP_MODULE64 module{};module.SizeOfStruct=sizeof(module);const auto moduleOk=SymGetModuleInfo64(process,context.Rip,&module);log<<"moduleInfo="<<bool(moduleOk)<<" error="<<(moduleOk?0:GetLastError())<<" symbolType="<<module.SymType<<" base="<<hex(module.BaseOfImage)<<'\n';
    for(const auto& entry:std::vector<std::pair<std::string,DWORD64>>{{"rcx",context.Rcx},{"rdx",context.Rdx},{"r9",context.R9},{"stack",context.Rsp}}){
        unsigned char bytes[1024];SIZE_T read=0;if(ReadProcessMemory(process,reinterpret_cast<const void*>(entry.second),bytes,sizeof(bytes),&read)){std::ofstream data(prefix.string()+"-"+entry.first+".bin",std::ios::binary);data.write(reinterpret_cast<const char*>(bytes),static_cast<std::streamsize>(read));log<<entry.first<<"Bytes="<<read<<'\n';}
    }
    STACKFRAME64 frame{};frame.AddrPC.Offset=context.Rip;frame.AddrPC.Mode=AddrModeFlat;frame.AddrStack.Offset=context.Rsp;frame.AddrStack.Mode=AddrModeFlat;frame.AddrFrame.Offset=context.Rbp;frame.AddrFrame.Mode=AddrModeFlat;
    for(unsigned i=0;i<48;++i){
        if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,thread,&frame,&context,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr))break;
        if(!frame.AddrPC.Offset)break;alignas(SYMBOL_INFO) unsigned char storage[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};auto symbol=reinterpret_cast<SYMBOL_INFO*>(storage);symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=MAX_SYM_NAME;DWORD64 displacement=0;
        log<<"frame="<<hex(frame.AddrPC.Offset);IMAGEHLP_MODULE64 owner{};owner.SizeOfStruct=sizeof(owner);if(SymGetModuleInfo64(process,frame.AddrPC.Offset,&owner))log<<" module="<<owner.ModuleName<<" offset="<<hex(frame.AddrPC.Offset-owner.BaseOfImage);if(SymFromAddr(process,frame.AddrPC.Offset,&displacement,symbol))log<<' '<<symbol->Name<<'+'<<hex(displacement);else log<<" symbolError="<<GetLastError();log<<'\n';
    }
    // StackWalk mutates context: retrieve the original stopped-thread context.
    context.ContextFlags=CONTEXT_ALL;GetThreadContext(thread,&context);
    EXCEPTION_RECORD copy=record;EXCEPTION_POINTERS pointers{&copy,&context};MINIDUMP_EXCEPTION_INFORMATION info{event.dwThreadId,&pointers,FALSE};
    HANDLE dump=CreateFileW((prefix.wstring()+L".dmp").c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(dump!=INVALID_HANDLE_VALUE){const auto type=static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo|MiniDumpWithUnloadedModules|MiniDumpWithIndirectlyReferencedMemory);const auto ok=MiniDumpWriteDump(process,event.dwProcessId,dump,type,&info,nullptr,nullptr);log<<"dump="<<bool(ok)<<" error="<<(ok?0:GetLastError())<<'\n';CloseHandle(dump);}
    CloseHandle(thread);
}
}
int wmain(int argc,wchar_t** argv){
    if(argc<5){std::cerr<<"Usage: launcher workspace exe new-output timeout-seconds [fixture]\n";return 2;}
    const fs::path workspace=argv[1],exe=argv[2],out=argv[3];const bool fixture=argc==6&&std::wstring(argv[5])==L"fixture";const int seconds=_wtoi(argv[4]);
    if(seconds<10||seconds>180||!within(exe,workspace/L"work")||!within(out,workspace/L"work")||!fs::exists(exe)||fs::exists(out)){std::cerr<<"Require owned executable and new output under project work\n";return 2;}
    fs::create_directories(out);std::ofstream events(out/L"events.txt");
    std::wstring command=L"\""+exe.wstring()+L"\" "+(fixture?L"--intentional-debug-fixture":L"--portable --multi --disable-updater --disable-missing-files-check --only-bundled-plugins");
    std::vector<wchar_t> line(command.begin(),command.end());line.push_back(0);STARTUPINFOW start{};start.cb=sizeof(start);start.dwFlags=STARTF_USESHOWWINDOW;start.wShowWindow=SW_HIDE;PROCESS_INFORMATION owned{};
    if(!CreateProcessW(exe.c_str(),line.data(),nullptr,nullptr,FALSE,DEBUG_ONLY_THIS_PROCESS,nullptr,exe.parent_path().c_str(),&start,&owned)){events<<"launchError="<<GetLastError()<<'\n';return 2;}
    DebugSetProcessKillOnExit(TRUE);SymSetOptions(SYMOPT_UNDNAME|SYMOPT_IGNORE_CVREC|SYMOPT_NO_PROMPTS|SYMOPT_FAIL_CRITICAL_ERRORS);const auto symbolPath=exe.parent_path().string();const bool symbols=SymInitialize(owned.hProcess,symbolPath.c_str(),FALSE)!=FALSE;
    events<<"ownedPID="<<owned.dwProcessId<<" symbols="<<symbols<<'\n';unsigned exceptions=0;bool initialBreakpoint=false,exited=false,timedOut=false;DWORD exit=0;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
    while(!exited){
        if(std::chrono::steady_clock::now()>deadline){timedOut=true;TerminateProcess(owned.hProcess,0xe0000001);break;}
        DEBUG_EVENT event{};if(!WaitForDebugEvent(&event,100))continue;DWORD disposition=DBG_CONTINUE;
        switch(event.dwDebugEventCode){
        case CREATE_PROCESS_DEBUG_EVENT:if(event.u.CreateProcessInfo.hFile){const auto name=pathOf(event.u.CreateProcessInfo.hFile);events<<"processImage="<<name<<" base="<<event.u.CreateProcessInfo.lpBaseOfImage<<'\n';if(symbols){const auto loaded=SymLoadModuleEx(owned.hProcess,nullptr,name.c_str(),nullptr,reinterpret_cast<DWORD64>(event.u.CreateProcessInfo.lpBaseOfImage),0,nullptr,0);events<<"symbolsBase="<<hex(loaded)<<" error="<<(loaded?0:GetLastError())<<'\n';}CloseHandle(event.u.CreateProcessInfo.hFile);}break;
        case LOAD_DLL_DEBUG_EVENT:if(event.u.LoadDll.hFile){const auto name=pathOf(event.u.LoadDll.hFile);events<<"module="<<name<<" base="<<event.u.LoadDll.lpBaseOfDll<<'\n';if(symbols){const auto loaded=SymLoadModuleEx(owned.hProcess,nullptr,name.c_str(),nullptr,reinterpret_cast<DWORD64>(event.u.LoadDll.lpBaseOfDll),0,nullptr,0);events<<"symbolsBase="<<hex(loaded)<<" error="<<(loaded?0:GetLastError())<<'\n';}CloseHandle(event.u.LoadDll.hFile);}break;
        case UNLOAD_DLL_DEBUG_EVENT:if(symbols)SymUnloadModule64(owned.hProcess,reinterpret_cast<DWORD64>(event.u.UnloadDll.lpBaseOfDll));break;
        case EXCEPTION_DEBUG_EVENT:{const auto code=event.u.Exception.ExceptionRecord.ExceptionCode;if(code==EXCEPTION_BREAKPOINT&&!initialBreakpoint){initialBreakpoint=true;break;}disposition=DBG_EXCEPTION_NOT_HANDLED;if(code==EXCEPTION_ACCESS_VIOLATION||code==0xc0000374||code==0xc0000409||code==EXCEPTION_STACK_OVERFLOW){++exceptions;exceptionEvidence(owned.hProcess,event,out,exceptions);events<<"exception="<<hex(code)<<" firstChance="<<event.u.Exception.dwFirstChance<<" thread="<<event.dwThreadId<<'\n';events.flush();std::cout<<"Captured exception "<<exceptions<<'\n'<<std::flush;}break;}
        case EXIT_PROCESS_DEBUG_EVENT:exit=event.u.ExitProcess.dwExitCode;exited=true;break;
        default:break;
        }
        ContinueDebugEvent(event.dwProcessId,event.dwThreadId,disposition);
    }
    if(symbols)SymCleanup(owned.hProcess);CloseHandle(owned.hThread);CloseHandle(owned.hProcess);
    events<<"exit="<<exit<<" timeout="<<timedOut<<" exceptions="<<exceptions<<'\n';
    std::cout<<"Exit="<<exit<<" timeout="<<timedOut<<" exceptions="<<exceptions<<'\n';return exited&&!timedOut&&exit==0&&!exceptions?0:1;
}
