// SPDX-License-Identifier: MIT
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using Microsoft.Win32;

internal sealed class MigratedFile { public string Location,Path,Backup,Hash; }
internal sealed class ModernState {
    public string Product="OBSPlayingNext",Layout="modern-v1",Version=Engine.Version,Target,ObsRoot;
    public List<OwnedFile> Files=new List<OwnedFile>();
    public List<MigratedFile> Migrated=new List<MigratedFile>();
}
internal static class ModernInstaller {
    internal static string CommonRoot=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),"obs-studio","plugins");
    internal static bool ForceUninstall;
    internal static bool IsPortable(string obs){return File.Exists(Engine.Child(obs,"portable_mode.txt"))||File.Exists(Engine.Child(obs,"config/obs-studio/global.ini"))||File.Exists(Engine.Child(obs,"plugins/obs-playing-next/setup/state.json"));}
    internal static bool Applies(string obs){
        string exe=Engine.Child(obs,"bin/64bit/obs64.exe");
        return File.Exists(exe)&&FileVersionInfo.GetVersionInfo(exe).FileMajorPart==33;
    }
    internal static string Root(string obs){return IsPortable(obs)?Engine.Child(obs,"plugins/obs-playing-next"):Engine.Child(CommonRoot,"obs-playing-next");}
    internal static string StateRoot(string obs){return Engine.Child(Root(obs),"setup");}
    internal static Dictionary<string,byte[]> Payload(){
        var result=new Dictionary<string,byte[]>(StringComparer.OrdinalIgnoreCase);
        const string binaries="obs-plugins\\64bit\\",data="data\\obs-plugins\\obs-playing-next\\";
        foreach(var pair in Engine.Payload("plugin")){
            string name=pair.Key.StartsWith(binaries,StringComparison.OrdinalIgnoreCase)?pair.Key.Substring(binaries.Length):pair.Key.StartsWith(data,StringComparison.OrdinalIgnoreCase)?"data\\"+pair.Key.Substring(data.Length):null;
            if(name==null)throw new InvalidDataException("Unexpected plugin payload path");
            Engine.Child(Path.GetTempPath(),name);result.Add(name,pair.Value);
        }
        return result;
    }
    internal static ModernState Read(string obs){
        string root=Root(obs),path=Engine.Child(StateRoot(obs),"state.json");
        if(!File.Exists(path))return null;
        var state=Engine.Json.Deserialize<ModernState>(File.ReadAllText(path));
        if(state==null||state.Product!="OBSPlayingNext"||state.Layout!="modern-v1"||state.Files==null||state.Migrated==null||!String.Equals(Engine.Full(root),Engine.Full(state.Target),StringComparison.OrdinalIgnoreCase))throw new InvalidDataException(L.T("Registro de instalación desconocido"));
        var allowed=new HashSet<string>(Payload().Keys,StringComparer.OrdinalIgnoreCase);allowed.Add("setup\\Uninstall.exe");var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach(var f in state.Files){
            Engine.Child(root,f.Path);
            if(!allowed.Contains(f.Path)||!seen.Add(f.Path)||f.Hash==null||f.Hash.Length!=64)throw new InvalidDataException(L.T("Registro inválido"));
            if(f.Original!=null){Engine.Child(StateRoot(obs),f.Original);if(f.OriginalHash==null||f.OriginalHash.Length!=64)throw new InvalidDataException(L.T("Copia original inválida"));}
        }
        foreach(var f in state.Migrated){Engine.Child(StateRoot(obs),f.Backup);if(f.Hash==null||f.Hash.Length!=64)throw new InvalidDataException(L.T("Copia original inválida"));}
        return state;
    }
    static void Gate(string obs,Action action){
        string name="Local\\OBSPlayingNext-"+Engine.Hash(Encoding.UTF8.GetBytes(Engine.Full(Root(obs)).ToUpperInvariant()));
        using(var mutex=new System.Threading.Mutex(false,name)){bool held=false;try{try{held=mutex.WaitOne(0);}catch(System.Threading.AbandonedMutexException){held=true;}if(!held)throw new IOException(L.T("Otra instalación está utilizando esta carpeta."));action();}finally{if(held)mutex.ReleaseMutex();}}
    }
    static void Restore(List<KeyValuePair<string,byte[]>> undo){foreach(var item in Enumerable.Reverse(undo)){if(item.Value==null){if(File.Exists(item.Key))File.Delete(item.Key);}else Engine.Atomic(item.Key,item.Value);}}
    static List<KeyValuePair<string,string>> LegacyCandidates(string obs){
        var result=new List<KeyValuePair<string,string>>();string module="obs-plugins\\64bit\\obs-playing-next.dll";
        string legacyState=Engine.Child(Engine.LegacyStateRoot("plugin",obs),"state.json");
        InstallState old=null;if(File.Exists(legacyState))old=Engine.ReadLegacyState("plugin",obs);
        string oldModule=Engine.Child(obs,module);
        if(File.Exists(oldModule)){
            var owned=old==null?null:old.Files.FirstOrDefault(f=>String.Equals(f.Path,module,StringComparison.OrdinalIgnoreCase));
            if(owned!=null&&Engine.HashFile(oldModule)!=owned.Hash)throw new IOException(L.T("Archivo modificado: ")+module);
            result.Add(new KeyValuePair<string,string>("obs:"+module,oldModule));
        }
        if(old!=null&&File.Exists(oldModule)){
            result.Add(new KeyValuePair<string,string>("obs:data\\obs-plugins\\obs-playing-next\\setup\\state.json",legacyState));
            string uninstall="data\\obs-plugins\\obs-playing-next\\setup\\Uninstall.exe";var owned=old.Files.FirstOrDefault(f=>String.Equals(f.Path,uninstall,StringComparison.OrdinalIgnoreCase));string oldUninstall=Engine.Child(obs,uninstall);
            if(owned!=null&&File.Exists(oldUninstall)){
                if(Engine.HashFile(oldUninstall)!=owned.Hash)throw new IOException(L.T("Archivo modificado: ")+uninstall);
                result.Add(new KeyValuePair<string,string>("obs:"+uninstall,oldUninstall));
            }
        }
        string oldCommon=Engine.Child(Root(obs),"bin/64bit/obs-playing-next.dll");
        if(File.Exists(oldCommon))result.Add(new KeyValuePair<string,string>("plugin:bin\\64bit\\obs-playing-next.dll",oldCommon));
        return result;
    }
    internal static void Register(string obs,bool remove){
        string root=Root(obs),key="Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\OBSPlayingNext-plugin-"+Engine.Hash(Encoding.UTF8.GetBytes(Engine.Full(root).ToUpperInvariant())).Substring(0,12);
        if(remove){using(var existing=Registry.CurrentUser.OpenSubKey(key)){if(existing!=null&&!String.Equals(existing.GetValue("InstallLocation") as string,root,StringComparison.OrdinalIgnoreCase))return;}Registry.CurrentUser.DeleteSubKeyTree(key,false);return;}
        using(var entry=Registry.CurrentUser.CreateSubKey(key)){
            entry.SetValue("DisplayName","OBS Playing NEXUS Plugin");entry.SetValue("DisplayVersion",Engine.Version);entry.SetValue("Publisher","OBS NEXUS / DjEmiMas");entry.SetValue("InstallLocation",root);
            entry.SetValue("UninstallString","\""+Engine.Child(StateRoot(obs),"Uninstall.exe")+"\" /uninstall /role plugin /modern /target \""+obs+"\"");entry.SetValue("NoModify",1);entry.SetValue("NoRepair",1);
        }
    }
    static void RetireLegacyRegistration(string obs){
        string key="Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\OBSPlayingNext-plugin-"+Engine.Hash(Encoding.UTF8.GetBytes(Engine.Full(obs).ToUpperInvariant())).Substring(0,12);
        using(var entry=Registry.CurrentUser.OpenSubKey(key)){if(entry==null||!String.Equals(entry.GetValue("InstallLocation") as string,obs,StringComparison.OrdinalIgnoreCase))return;}
        Registry.CurrentUser.DeleteSubKeyTree(key,false);
    }
    internal static void Install(string obs,bool registry,int failAfter){Gate(obs,()=>InstallCore(obs,registry,failAfter));}
    static void InstallCore(string obs,bool registry,int failAfter){
        obs=Engine.Full(obs);Engine.Validate("plugin",obs);Engine.Closed("plugin");
        string root=Root(obs),setup=StateRoot(obs),statePath=Engine.Child(setup,"state.json");Engine.Safe(root);
        var old=Read(obs);var payload=Payload();payload["setup\\Uninstall.exe"]=File.ReadAllBytes(Assembly.GetExecutingAssembly().Location);
        var migration=LegacyCandidates(obs);
        foreach(var pair in payload){string path=Engine.Child(root,pair.Key);var prior=old==null?null:old.Files.FirstOrDefault(f=>String.Equals(f.Path,pair.Key,StringComparison.OrdinalIgnoreCase));
            if(File.Exists(path)&&prior!=null&&Engine.HashFile(path)!=prior.Hash)throw new IOException(L.T("Archivo modificado: ")+pair.Key);
            if(prior==null&&File.Exists(path)&&pair.Key!="obs-playing-next.dll"&&pair.Key!="obs-playing-db.dll"&&pair.Key!="playing-qt-plugins\\tls\\qschannelbackend.dll"&&Engine.HashFile(path)!=Engine.Hash(pair.Value))throw new IOException(L.T("Destino ocupado por un archivo ajeno: ")+pair.Key);
        }
        byte[] previous=File.Exists(statePath)?File.ReadAllBytes(statePath):null;
        var state=new ModernState{Target=root,ObsRoot=obs};if(old!=null)state.Migrated.AddRange(old.Migrated);
        var undo=new List<KeyValuePair<string,byte[]>>();string generation="backups\\"+DateTime.UtcNow.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N");
        try{
            int count=0;
            foreach(var pair in payload){string path=Engine.Child(root,pair.Key);byte[] before=File.Exists(path)?File.ReadAllBytes(path):null;undo.Add(new KeyValuePair<string,byte[]>(path,before));var prior=old==null?null:old.Files.FirstOrDefault(f=>String.Equals(f.Path,pair.Key,StringComparison.OrdinalIgnoreCase));string original=prior==null?null:prior.Original,hash=prior==null?null:prior.OriginalHash;
                if(before!=null){string backup=generation+"\\payload\\"+pair.Key;Engine.Atomic(Engine.Child(setup,backup),before);if(prior==null){original=backup;hash=Engine.Hash(before);}}
                Engine.Atomic(path,pair.Value);if(Engine.HashFile(path)!=Engine.Hash(pair.Value))throw new IOException(L.T("Checksum de escritura incorrecto"));
                state.Files.Add(new OwnedFile{Path=pair.Key,Hash=Engine.Hash(pair.Value),Original=original,OriginalHash=hash});if(failAfter>0&&++count==failAfter)throw new IOException("Injected rollback test");
            }
            foreach(var pair in migration){
                Engine.Safe(pair.Value);byte[] before=File.ReadAllBytes(pair.Value);string backup=generation+"\\legacy\\"+state.Migrated.Count+".bin";
                Engine.Atomic(Engine.Child(setup,backup),before);if(Engine.HashFile(Engine.Child(setup,backup))!=Engine.Hash(before))throw new IOException("Migration backup mismatch");
                undo.Add(new KeyValuePair<string,byte[]>(pair.Value,before));if(pair.Key.EndsWith("obs-playing-next.dll",StringComparison.OrdinalIgnoreCase))File.Delete(pair.Value);
                int separator=pair.Key.IndexOf(':');state.Migrated.Add(new MigratedFile{Location=pair.Key.Substring(0,separator),Path=pair.Key.Substring(separator+1),Backup=backup,Hash=Engine.Hash(before)});
                if(failAfter>0&&++count==failAfter)throw new IOException("Injected migration rollback test");
            }
            Engine.Atomic(statePath,Encoding.UTF8.GetBytes(Engine.Json.Serialize(state)));if(registry){Engine.Register("plugin",obs,false);RetireLegacyRegistration(obs);}
        }catch{Restore(undo);if(previous==null){if(File.Exists(statePath))File.Delete(statePath);}else Engine.Atomic(statePath,previous);throw;}
    }
    internal static List<string> Uninstall(string obs,bool registry){List<string> result=null;Gate(obs,()=>result=UninstallCore(obs,registry));return result;}
    static List<string> UninstallCore(string obs,bool registry){
        Engine.Closed("plugin");string root=Root(obs),setup=StateRoot(obs);var state=Read(obs);var kept=new List<string>();if(state==null)return kept;
        foreach(var f in state.Files)if(f.Original!=null&&(!File.Exists(Engine.Child(setup,f.Original))||Engine.HashFile(Engine.Child(setup,f.Original))!=f.OriginalHash))throw new IOException(L.T("Falta copia original válida; no se desinstala."));
        foreach(var f in state.Migrated)if(!File.Exists(Engine.Child(setup,f.Backup))||Engine.HashFile(Engine.Child(setup,f.Backup))!=f.Hash)throw new IOException(L.T("Falta copia original válida; no se desinstala."));
        var undo=new List<KeyValuePair<string,byte[]>>();try{
            foreach(var f in state.Files){string path=Engine.Child(root,f.Path);if(File.Exists(path)&&Engine.HashFile(path)!=f.Hash){kept.Add(f.Path);continue;}
                undo.Add(new KeyValuePair<string,byte[]>(path,File.Exists(path)?File.ReadAllBytes(path):null));if(f.Original!=null)Engine.Atomic(path,File.ReadAllBytes(Engine.Child(setup,f.Original)));else if(File.Exists(path))File.Delete(path);
            }
            // Migrated versions stay in backups; uninstall must not resurrect a legacy plugin.
            if(registry)Engine.Register("plugin",obs,true);File.Delete(Engine.Child(setup,"state.json"));return kept;
        }catch{Restore(undo);throw;}
    }
}
