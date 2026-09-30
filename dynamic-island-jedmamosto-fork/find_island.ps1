Add-Type @'
using System;
using System.Runtime.InteropServices;
public class Win {
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
}
'@

$h = [Win]::FindWindow("Windhawk.DynamicIslandForWindows", $null)
if ($h -ne [IntPtr]::Zero) {
    $vis = [Win]::IsWindowVisible($h)
    $rc = New-Object Win+RECT
    [Win]::GetWindowRect($h, [ref]$rc) | Out-Null
    Write-Host "Found Window! HWND: $h, Visible: $vis, Rect: ($($rc.Left), $($rc.Top), $($rc.Right), $($rc.Bottom))"
} else {
    Write-Host "FindWindow returned NULL (no window found)."
}
