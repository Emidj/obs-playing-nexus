// Windows PDH process engine counters. Test runner only, never loaded into OBS.
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public sealed class PlayingGpuCounters : IDisposable {
 [DllImport("pdh.dll",CharSet=CharSet.Unicode)] static extern uint PdhOpenQueryW(string source,UIntPtr data,out IntPtr query);
 [DllImport("pdh.dll",CharSet=CharSet.Unicode)] static extern uint PdhAddEnglishCounterW(IntPtr query,string path,UIntPtr data,out IntPtr counter);
 [DllImport("pdh.dll")] static extern uint PdhCollectQueryData(IntPtr query);
 [DllImport("pdh.dll")] static extern uint PdhCloseQuery(IntPtr query);
 [DllImport("pdh.dll",CharSet=CharSet.Unicode)] static extern uint PdhGetFormattedCounterArrayW(IntPtr counter,uint format,ref uint bytes,out uint count,IntPtr items);
 [StructLayout(LayoutKind.Explicit,Size=16)] struct Value {[FieldOffset(0)] public uint Status;[FieldOffset(8)] public double Number;}
 [StructLayout(LayoutKind.Sequential)] struct Item {public IntPtr Name; public Value Value;}
 IntPtr query,counter;
 public PlayingGpuCounters(){uint status=PdhOpenQueryW(null,UIntPtr.Zero,out query);if(status!=0)throw new InvalidOperationException("PdhOpenQuery: "+status.ToString("X"));status=PdhAddEnglishCounterW(query,@"\GPU Engine(*)\Utilization Percentage",UIntPtr.Zero,out counter);if(status!=0){Dispose();throw new InvalidOperationException("PdhAddEnglishCounter: "+status.ToString("X"));}PdhCollectQueryData(query);}
 public Dictionary<string,double> Read(int processId){uint status=PdhCollectQueryData(query);if(status!=0)throw new InvalidOperationException("PdhCollect: "+status.ToString("X"));uint bytes=0,count;status=PdhGetFormattedCounterArrayW(counter,0x200,ref bytes,out count,IntPtr.Zero);if(status!=0x800007D2&&status!=0)throw new InvalidOperationException("Pdh size: "+status.ToString("X"));var result=new Dictionary<string,double>();if(bytes==0)return result;IntPtr buffer=Marshal.AllocHGlobal(checked((int)bytes));try{status=PdhGetFormattedCounterArrayW(counter,0x200,ref bytes,out count,buffer);if(status!=0)throw new InvalidOperationException("Pdh array: "+status.ToString("X"));int stride=Marshal.SizeOf(typeof(Item));string prefix="pid_"+processId+"_";for(uint i=0;i<count;i++){var item=(Item)Marshal.PtrToStructure(IntPtr.Add(buffer,checked((int)i*stride)),typeof(Item));string name=Marshal.PtrToStringUni(item.Name);if(name!=null&&name.StartsWith(prefix,StringComparison.Ordinal)&&item.Value.Status<=1&&!Double.IsNaN(item.Value.Number)&&!Double.IsInfinity(item.Value.Number))result[name]=item.Value.Number;}}finally{Marshal.FreeHGlobal(buffer);}return result;}
 public void Dispose(){if(query!=IntPtr.Zero){PdhCloseQuery(query);query=IntPtr.Zero;}}
}
