// SPDX-License-Identifier: MIT
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include "playing/companion/profile.hpp"
#include "playing/companion/protected-store.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QFileInfo>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QThread>
#include <iostream>
#include <stdexcept>
using namespace playing::companion;
namespace {
unsigned checks=0;
void check(bool ok,const char *why) {++checks; if(!ok) throw std::runtime_error(why);}
struct Search {DWORD pid; HWND window=nullptr;};
BOOL CALLBACK findWindow(HWND handle,LPARAM value) {
    auto &search=*reinterpret_cast<Search *>(value); DWORD pid=0; GetWindowThreadProcessId(handle,&pid);
    wchar_t title[128]{}; GetWindowTextW(handle,title,128);
    if(pid==search.pid && IsWindowVisible(handle) && wcsstr(title,L"OBS Playing Companion")) {search.window=handle; return FALSE;}
    return TRUE;
}
HWND window(QProcess &process) {Search search{static_cast<DWORD>(process.processId())}; EnumWindows(findWindow,reinterpret_cast<LPARAM>(&search)); return search.window;}
template<class F> bool wait(F condition) {QElapsedTimer timer; timer.start(); while(!condition() && timer.elapsed()<7000) {QCoreApplication::processEvents(); QThread::msleep(10);} return condition();}
QByteArray read(const QString &path) {QFile file(path); if(!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Profile read"); return file.readAll();}
void close(QProcess &process) {check(window(process)!=nullptr,"Owned main window missing"); check(PostMessageW(window(process),WM_CLOSE,0,0)!=0,"Close request"); check(process.waitForFinished(7000),"Normal close deadline"); check(process.exitStatus()==QProcess::NormalExit && process.exitCode()==0,"Executable exit");}
struct Resources {quint64 cpu; SIZE_T working,privateBytes; DWORD handles;};
Resources resources(QProcess &process) {
    const auto handle=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,static_cast<DWORD>(process.processId()));
    if(!handle) throw std::runtime_error("Resource process access");
    FILETIME created{},exited{},kernel{},user{}; PROCESS_MEMORY_COUNTERS_EX memory{}; memory.cb=sizeof(memory); DWORD handles=0;
    const bool ok=GetProcessTimes(handle,&created,&exited,&kernel,&user)&&GetProcessMemoryInfo(handle,reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory),sizeof(memory))&&GetProcessHandleCount(handle,&handles);
    CloseHandle(handle); if(!ok) throw std::runtime_error("Resource sampling");
    const auto ticks=[](FILETIME value) {return (static_cast<quint64>(value.dwHighDateTime)<<32)|value.dwLowDateTime;};
    return {ticks(kernel)+ticks(user),memory.WorkingSetSize,memory.PrivateUsage,handles};
}
void report(const char *phase,const char *role,Resources begin,Resources end,qint64 elapsed) {
    std::cout<<"RESOURCE phase="<<phase<<" role="<<role<<" elapsed_ms="<<elapsed
        <<" cpu_ms="<<static_cast<double>(end.cpu-begin.cpu)/10000.0
        <<" cpu_one_core_percent="<<static_cast<double>(end.cpu-begin.cpu)/100.0/static_cast<double>(elapsed)
        <<" working_begin="<<begin.working<<" working_end="<<end.working
        <<" private_begin="<<begin.privateBytes<<" private_end="<<end.privateBytes
        <<" handles_begin="<<begin.handles<<" handles_end="<<end.handles<<std::endl;
}
void hold(QProcess &dj,QProcess &receiver,int milliseconds) {
    QElapsedTimer timer; timer.start(); while(timer.elapsed()<milliseconds) {
        QCoreApplication::processEvents(); QThread::msleep(10);
        if(dj.state()!=QProcess::Running||receiver.state()!=QProcess::Running) throw std::runtime_error("Soak process exited");
    }
}
void packagedModules(QProcess &process,const QString &directory) {
    const auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,static_cast<DWORD>(process.processId()));
    check(snapshot!=INVALID_HANDLE_VALUE,"Module snapshot");
    MODULEENTRY32W entry{}; entry.dwSize=sizeof(entry); bool core=false,windows=false,tls=false,crt=false;
    bool local=true;
    if(Module32FirstW(snapshot,&entry)) do {
        const auto name=QString::fromWCharArray(entry.szModule).toLower();
        const auto path=QDir::fromNativeSeparators(QString::fromWCharArray(entry.szExePath));
        if(name.startsWith("qt6")||name=="qwindows.dll"||name=="qschannelbackend.dll"||name.startsWith("msvcp140")||name.startsWith("vcruntime140")) {
            local=local&&path.startsWith(QDir::fromNativeSeparators(directory)+"/",Qt::CaseInsensitive);
            core=core||name=="qt6core.dll"; windows=windows||name=="qwindows.dll"; tls=tls||name=="qschannelbackend.dll"; crt=crt||name=="msvcp140.dll";
        }
    } while(Module32NextW(snapshot,&entry));
    CloseHandle(snapshot); check(local&&core&&windows&&tls&&crt,"Portable process used external Qt/CRT or missing plugins");
}
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv); QProcess dj,receiver; QTemporaryDir root(QCoreApplication::applicationDirPath()+"/gui-process-XXXXXX");
    try {
        const auto option=argc==4?QString::fromLocal8Bit(argv[3]):QString{};
        check((argc==3||(argc==4&&(option=="--portable"||option=="--soak"))) && root.isValid(),"Executable fixture arguments");
        const bool portable=option=="--portable",soak=option=="--soak";
        const auto djDir=root.filePath("dj"),rxDir=root.filePath("receiver"); QDir().mkpath(djDir); QDir().mkpath(rxDir);
        ProtectedStore djStore(djDir+"/profile.pcs"),rxStore(rxDir+"/profile.pcs");
        QTcpServer reserve; check(reserve.listen(QHostAddress::LocalHost,0),"Reserve isolated port"); const auto port=reserve.serverPort(); reserve.close();
        Profile profile; profile.identity=createIdentity(); profile.sender.port=port;
        const auto credential=newDeviceCredential(); profile.devices.push_back({"STREAM-PC",credential}); djStore.save(encodeProfile(profile));
        Profile remote; remote.identity=createIdentity(); remote.autoReconnect=true; remote.receiver=ReceiverSettings{"127.0.0.1",port,"OBS Playing Companion",certificatePin(profile.identity.certificate),credential}; rxStore.save(encodeProfile(remote));
        const auto djBytes=read(djDir+"/profile.pcs"),rxBytes=read(rxDir+"/profile.pcs");
        auto launch=[&](QProcess &process,const QString &directory) {
            if(portable) {auto env=QProcessEnvironment::systemEnvironment(); for(const auto &key:env.keys()) if(key.startsWith("QT_")||key=="QML2_IMPORT_PATH"||key=="QML_IMPORT_PATH") env.remove(key); const auto system=env.value("SystemRoot"); env.insert("PATH",system+"/System32;"+system); process.setProcessEnvironment(env); process.setWorkingDirectory(directory);}
            process.setProgram(QString::fromLocal8Bit(argv[1])); process.setArguments({"--profile-dir",directory,"--db-library",QString::fromLocal8Bit(argv[2])}); process.start(); check(process.waitForStarted(5000),"Executable launch"); check(wait([&] {return window(process)!=nullptr;}),"Qt Windows dashboard startup");};
        for(unsigned round=0;round<2;++round) {
            launch(dj,djDir); launch(receiver,rxDir);
            check(dj.processId()!=receiver.processId(),"Independent executable PIDs");
            QElapsedTimer timer; timer.start(); while(timer.elapsed()<1500) {QCoreApplication::processEvents(); QThread::msleep(10);}
            check(dj.state()==QProcess::Running && receiver.state()==QProcess::Running,"Both dashboards remain running");
            check(reserve.listen(QHostAddress::LocalHost,port),"Startup opened sender listener"); reserve.close();
            check(read(djDir+"/profile.pcs")==djBytes && read(rxDir+"/profile.pcs")==rxBytes,"Startup modified protected profiles");
            if(soak&&round==0) {
                const auto a=resources(dj),b=resources(receiver); QElapsedTimer sample; sample.start(); hold(dj,receiver,60000);
                report("idle-stopped","sender",a,resources(dj),sample.elapsed()); report("idle-stopped","receiver",b,resources(receiver),sample.elapsed());
                check(reserve.listen(QHostAddress::LocalHost,port),"Idle unexpectedly opened listener"); reserve.close();
                check(read(djDir+"/profile.pcs")==djBytes&&read(rxDir+"/profile.pcs")==rxBytes,"Idle changed profiles");
            }
            close(receiver); close(dj);
        }
        check(decodeProfile(*djStore.load()).identity.privateKey.toDer()==profile.identity.privateKey.toDer(),"DJ identity survived executable restart");
        const auto restored=decodeProfile(*rxStore.load());
        check(restored.autoReconnect && restored.receiver && restored.receiver->device.secret==credential.secret,"Receiver settings survived restart");
        check(certificatePin(restored.identity.certificate)!=certificatePin(profile.identity.certificate),"Independent identities");
        const auto history=root.filePath("automatic-history.txt"); QFile historyFile(history);
        check(historyFile.open(QIODevice::WriteOnly) && historyFile.write("22:01 : Artist - Automatic first\r\n")>0,"Automatic executable history"); historyFile.close();
        const auto observerCredential=newDeviceCredential(); profile.devices.push_back({"FIXTURE-OBSERVER",observerCredential});
        profile.startup=StartupMode::Sender; profile.sender.selection=playing::providers::Request{"virtualdj",playing::providers::Mode::History,history.toUtf8().toStdString()};
        remote.startup=StartupMode::Receiver; djStore.save(encodeProfile(profile)); rxStore.save(encodeProfile(remote));
        launch(dj,djDir); launch(receiver,rxDir);
        auto channel=std::make_shared<playing::providers::network::Channel>(); playing::providers::ProviderRegistry registry;
        check(playing::providers::network::registerProvider(registry,channel)==playing::providers::Registration::Registered,"Automatic observer registry");
        const playing::providers::Request request{"network",playing::providers::Mode::Automatic,""}; const auto token=registry.acquire(request).token;
        Receiver observer(channel); observer.changed=[&] {registry.refresh(request);}; auto settings=*remote.receiver; settings.device=observerCredential;
        check(observer.connectTo(settings),"Automatic executable observer connect");
        const auto title=[&](const char *expected) {const auto snapshot=registry.snapshot(token); return snapshot && snapshot->metadata && snapshot->metadata->title==expected;};
        check(wait([&] {return title("Automatic first");}),"Actual GUI sender starts and publishes without input");
        check(!reserve.listen(QHostAddress::LocalHost,port),"Automatic sender did not bind port"); reserve.close();
        check(historyFile.open(QIODevice::Append) && historyFile.write("22:02 : Artist - Automatic next\r\n")>0,"Automatic history update"); historyFile.close();
        check(wait([&] {return title("Automatic next");}),"Actual automatic sender publishes song change");
        if(soak) {
            observer.setAutoReconnect(true);
            const auto a=resources(dj),b=resources(receiver); QElapsedTimer sample; sample.start();
            for(unsigned change=0;change<60;++change) {
                const auto expected=QString("Soak %1 Jóga 🎵").arg(change).toUtf8();
                check(historyFile.open(QIODevice::Append),"Soak history append");
                const auto line=QByteArray("22:03 : Björk - ")+expected+"\r\n";
                check(historyFile.write(line)==line.size(),"Soak complete history write"); historyFile.close();
                check(wait([&] {return title(expected.constData());}),"Soak Unicode song delivery"); hold(dj,receiver,1000);
            }
            report("60-song-changes","sender",a,resources(dj),sample.elapsed()); report("60-song-changes","receiver",b,resources(receiver),sample.elapsed());
            const auto c=resources(dj),d=resources(receiver); sample.restart(); hold(dj,receiver,60000);
            report("connected-unchanged","sender",c,resources(dj),sample.elapsed()); report("connected-unchanged","receiver",d,resources(receiver),sample.elapsed());
            close(dj); check(wait([&] {return !observer.connected()&&!registry.snapshot(token)->metadata;}),"Sender close clears observer state");
            check(receiver.state()==QProcess::Running,"GUI receiver exited after sender close"); launch(dj,djDir);
            check(wait([&] {return observer.connected()&&title("Soak 59 Jóga 🎵");}),"Saved credentials reconnect after sender restart");
            hold(dj,receiver,3000); check(receiver.state()==QProcess::Running,"GUI receiver exited after sender restart");
            check(decodeProfile(*rxStore.load()).receiver->device.secret==credential.secret,"Soak changed receiver credentials");
            std::cout<<"SOAK PASS idle60s, Unicode60 changes, unchanged60s, sender restart; observer certifies sender payload, GUI receiver lifecycle only; no LAN/GPU certification\n";
        }
        if(portable) {const auto directory=QFileInfo(QString::fromLocal8Bit(argv[1])).absolutePath(); packagedModules(dj,directory); packagedModules(receiver,directory);}
        observer.changed={}; observer.stop(); registry.release(token); close(receiver); close(dj);
        std::cout<<"PASS "<<checks<<" actual GUI executable lifecycle checks; two processes, two starts, normal closes; no interactive transmission certification\n"; return 0;
    } catch(const std::exception &error) {
        // Only processes created by this fixture may be terminated; a kill is never PASS.
        for(auto process:{&dj,&receiver}) if(process->state()!=QProcess::NotRunning) {if(auto handle=window(*process)) PostMessageW(handle,WM_CLOSE,0,0); if(!process->waitForFinished(2000)) {process->kill(); process->waitForFinished(2000);}}
        root.setAutoRemove(false); std::cerr<<"FAIL "<<error.what()<<"; fixture "<<root.path().toStdString()<<'\n'; return 1;
    }
}
