Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public class WinUtil {
    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);
}
'@

[WinUtil]::EnumWindows({
    param($hwnd, $lparam)
    $sbClass = New-Object System.Text.StringBuilder 256
    [WinUtil]::GetClassName($hwnd, $sbClass, 256) | Out-Null
    if ($sbClass.ToString() -like "*DynamicIsland*") {
        $procId = [uint32]0
        [WinUtil]::GetWindowThreadProcessId($hwnd, [ref]$procId) | Out-Null
        $vis = [WinUtil]::IsWindowVisible($hwnd)
        Write-Host "Found Island Window! HWND: $hwnd, PID: $procId, Class: $($sbClass.ToString()), Visible: $vis"
    }
    return $true
}, [IntPtr]::Zero) | Out-Null
