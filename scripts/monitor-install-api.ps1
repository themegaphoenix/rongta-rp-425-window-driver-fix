$ErrorActionPreference = 'Stop'
if (-not ('RpMonitorInstall' -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class RpMonitorInstall {
 [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] public struct MonitorInfo {
  [MarshalAs(UnmanagedType.LPWStr)] public string Name,Environment,Dll;
 }
 [StructLayout(LayoutKind.Sequential)] struct Luid { public uint Low; public int High; }
 [StructLayout(LayoutKind.Sequential)] struct Privileges { public uint Count; public Luid Id; public uint Attributes; }
 [StructLayout(LayoutKind.Sequential)] struct Defaults { public IntPtr DataType,DevMode; public uint Access; }
 [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
 [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
 [DllImport("advapi32.dll",SetLastError=true)] static extern bool OpenProcessToken(IntPtr process,uint access,out IntPtr token);
 [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool LookupPrivilegeValue(string system,string name,out Luid id);
 [DllImport("advapi32.dll",SetLastError=true)] static extern bool AdjustTokenPrivileges(IntPtr token,bool disable,ref Privileges privileges,uint bytes,IntPtr previous,IntPtr needed);
 [DllImport("winspool.drv",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool AddMonitor(string server,uint level,ref MonitorInfo info);
 [DllImport("winspool.drv",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool DeleteMonitor(string server,string environment,string name);
 [DllImport("winspool.drv",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool OpenPrinter(string name,out IntPtr printer,ref Defaults defaults);
 [DllImport("winspool.drv",SetLastError=true)] static extern bool ClosePrinter(IntPtr printer);
 [DllImport("winspool.drv",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool GetPrinter(IntPtr printer,uint level,IntPtr data,uint bytes,out uint needed);
 [DllImport("winspool.drv",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool SetPrinter(IntPtr printer,uint level,IntPtr data,uint command);
 static void Check(bool success) { if(!success) throw new Win32Exception(Marshal.GetLastWin32Error()); }
 static IntPtr EnableLoadPrivilege() {
  IntPtr token; Check(OpenProcessToken(GetCurrentProcess(),0x28,out token));
  try {
   var privileges=new Privileges { Count=1,Attributes=2 };
   Check(LookupPrivilegeValue(null,"SeLoadDriverPrivilege",out privileges.Id));
   Check(AdjustTokenPrivileges(token,false,ref privileges,0,IntPtr.Zero,IntPtr.Zero));
   if(Marshal.GetLastWin32Error()==1300) throw new Win32Exception(1300);
   return token;
  } catch { CloseHandle(token); throw; }
 }
 public static void Install() {
  var token=EnableLoadPrivilege();
  try {
   var info=new MonitorInfo { Name="RP425 Bluetooth Native",Environment="Windows x64",Dll="RongtaRp425Bluetooth.dll" };
   Check(AddMonitor(null,2,ref info));
  } finally { CloseHandle(token); }
 }
 public static void Uninstall() {
  var token=EnableLoadPrivilege();
  try { Check(DeleteMonitor(null,"Windows x64","RP425 Bluetooth Native")); }
  finally { CloseHandle(token); }
 }
 public static uint Bidirectional(string name, bool enabled) {
  if(IntPtr.Size!=8) throw new Exception("Use x64 PowerShell.");
  var defaults=new Defaults { Access=12 }; IntPtr printer;
  Check(OpenPrinter(name,out printer,ref defaults));
  IntPtr data=IntPtr.Zero;
  try {
   uint needed;
   bool queried=GetPrinter(printer,2,IntPtr.Zero,0,out needed);
   if(!queried && Marshal.GetLastWin32Error()!=122) throw new Win32Exception(Marshal.GetLastWin32Error());
   if(needed<136 || needed>16*1024*1024) throw new Exception("Invalid printer information size.");
   data=Marshal.AllocHGlobal((int)needed);
   Check(GetPrinter(printer,2,data,needed,out needed));
   uint old=unchecked((uint)Marshal.ReadInt32(data,104));
   uint next=enabled ? old|0x800U : old&~0x800U;
   Marshal.WriteInt32(data,104,unchecked((int)next));
   Marshal.WriteIntPtr(data,96,IntPtr.Zero);
   Check(SetPrinter(printer,2,data,0));
   return old;
  } finally { if(data!=IntPtr.Zero) Marshal.FreeHGlobal(data); ClosePrinter(printer); }
 }
}
'@
}
