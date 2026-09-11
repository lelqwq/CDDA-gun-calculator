<#
  scripts\screenshot_gui.ps1
  ---------------------------------------------------------------------------
  启动 gunlab_gui.exe，把窗口截图存成 PNG，然后关掉进程。

  为什么需要：工具的执行环境本身看不见窗口，但「界面到底渲染成什么样」
  又是改 GUI 时唯一该看的证据（中文有没有变方块、列有没有被切掉、某块
  内容有没有真的画出来）。这个脚本就是那条捷径。

  用法：
    powershell -File scripts\screenshot_gui.ps1 -AppArg "AKM" -Out shot.png

  两个坑（都踩过，别再踩）：
    1. 本文件必须存成带 UTF-8 BOM。Windows PowerShell 5.1 对无 BOM 的
       脚本按系统代码页（936）解读，中文注释被解坏后会吃掉 here-string
       的结束符，报一堆 The string is missing the terminator。
       重新存一遍：
         $c = Get-Content -Raw -Encoding UTF8 <文件>
         [System.IO.File]::WriteAllText(<文件>, $c, (New-Object System.Text.UTF8Encoding($true)))

    2. 必须先声明 DPI 感知。屏幕缩放 125% 时，不声明的话 GetWindowRect
       返回的是被系统虚拟化过的坐标（1400x900 的窗口会报成 1134x758），
       位图开小，PrintWindow 只截到左上角 —— 看起来像界面右边被切了，
       其实界面好得很，是截图工具自己的问题。
#>
param(
    [string]$Exe     = (Join-Path $PSScriptRoot "..\build-gui\gunlab_gui.exe"),
    [string]$Out     = "gui.png",
    [int]   $WaitSec = 7,      # 启动后等多久再截图（等字体加载和首帧渲染）
    [string]$AppArg  = "",     # 透传给 exe 的关键词，例：-AppArg "AKM"
    [int]   $Scroll  = 0       # 滚轮格数，正数向下。窗口没焦点时无效
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

# ---- 1. 声明 DPI 感知（必须在任何窗口/DC 操作之前）---------------------------
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Dpi {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr value);
}
"@
# -4 = DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
[void][Dpi]::SetProcessDpiAwarenessContext([IntPtr](-4))

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Cap {
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, int d, IntPtr e);
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

# ---- 2. 启动 ----------------------------------------------------------------
# 先收拾掉上一次可能没关干净的进程，否则残留窗口会干扰按进程找窗口。
#
# ★ 杀完必须等它真的退出再启动新的。不等的话，旧窗口还没销毁，
#   新进程的 MainWindowHandle 有可能拿到旧窗口 —— 结果是截出来一张
#   上一版程序的图，看起来像「代码没生效」。这个假象真的骗到过我一次。
Get-Process gunlab_gui -ErrorAction SilentlyContinue | ForEach-Object {
    $_ | Stop-Process -Force
    [void]$_.WaitForExit( 5000 )
}

if ($AppArg -ne "") {
    $p = Start-Process -FilePath $Exe -ArgumentList $AppArg -PassThru `
                       -WorkingDirectory (Split-Path $Exe)
} else {
    $p = Start-Process -FilePath $Exe -PassThru -WorkingDirectory (Split-Path $Exe)
}
Start-Sleep -Seconds $WaitSec

if ($p.HasExited) { Write-Host "进程已退出，退出码 $($p.ExitCode)"; exit 1 }

$p.Refresh()
$h = $p.MainWindowHandle
if ($h -eq 0) { Write-Host "没拿到窗口句柄"; $p | Stop-Process -Force; exit 2 }
Write-Host "hwnd=$h title='$($p.MainWindowTitle)'"

# 报错对话框（比如缺 DLL）也会被当成主窗口，标题能区分出来
if ($p.MainWindowTitle -ne "gunlab — Cataclysm 枪械计算器") {
    Write-Host "警告：窗口标题不是预期值，可能弹的是报错对话框"
}

[void][Cap]::ShowWindow( $h, 5 )          # SW_SHOW
[void][Cap]::SetForegroundWindow( $h )
Start-Sleep -Seconds 2

# ---- 3. 可选：滚轮 ----------------------------------------------------------
$r = New-Object 'Cap+RECT'
[void][Cap]::GetWindowRect( $h, [ref]$r )
$w  = $r.Right - $r.Left
$ht = $r.Bottom - $r.Top
Write-Host "size=${w}x${ht} at ($($r.Left),$($r.Top))"

if ($w -le 0 -or $ht -le 0) { Write-Host "窗口尺寸为 0"; $p | Stop-Process -Force; exit 3 }

if ($Scroll -ne 0) {
    # 光标移到详情面板上，滚轮才滚得动那个子区域
    [void][Cap]::SetCursorPos( $r.Left + [int]($w * 0.72), $r.Top + [int]($ht * 0.55) )
    Start-Sleep -Milliseconds 300
    $delta = if ($Scroll -gt 0) { -120 } else { 120 }   # 负 delta = 向下滚
    for ($i = 0; $i -lt [Math]::Abs($Scroll); $i++) {
        [Cap]::mouse_event( 0x0800, 0, 0, $delta, [IntPtr]::Zero )
        Start-Sleep -Milliseconds 40
    }
    Start-Sleep -Milliseconds 600
    Write-Host "scrolled $Scroll"
}

# ---- 4. 截图 ----------------------------------------------------------------
$bmp = New-Object System.Drawing.Bitmap( $w, $ht )
$g   = [System.Drawing.Graphics]::FromImage( $bmp )
$hdc = $g.GetHdc()
# 标志位 2 = PW_RENDERFULLCONTENT，没它截 D3D 渲染的窗口会是一片黑
$ok  = [Cap]::PrintWindow( $h, $hdc, 2 )
$g.ReleaseHdc( $hdc )
$g.Dispose()
Write-Host "PrintWindow=$ok"

$bmp.Save( $Out, [System.Drawing.Imaging.ImageFormat]::Png )
$bmp.Dispose()

$p | Stop-Process -Force
Write-Host "已保存 $Out"
