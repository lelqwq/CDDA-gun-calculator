# =============================================================================
#  scripts\copy_gui_deps.ps1
# -----------------------------------------------------------------------------
#  把 gunlab_gui.exe 依赖的 MinGW 运行库拷到 exe 旁边。
#
#  为什么需要：SDL3.dll 是 msys64 里编译出来的共享库，它自己不静态 —— 后面
#  还拖着 libiconv-2.dll 这类 msys64 的 DLL。CMakeLists 里的 -static 只作用于
#  我们自己编译的 exe，管不到 SDL3.dll 的依赖链。不拷的话双击 exe 会弹
#  「由于找不到 libiconv-2.dll，无法继续执行代码」。
#
#  依赖清单不写死，而是构建时用 objdump 递归解析 PE 导入表算出来 ——
#  msys64 升级 SDL3 后依赖变了也不用改这里。只在 %MINGW%\bin 里找得到名字
#  才拷（找不到的就是 KERNEL32 这类系统 DLL，不用管）。
#
#  用法：
#    powershell -File copy_gui_deps.ps1 -Exe <exe路径> -Dest <目标目录> [-Mingw <msys64根>]
# =============================================================================
param(
    [Parameter(Mandatory = $true)][string]$Exe,
    [Parameter(Mandatory = $true)][string]$Dest,
    [string]$Mingw = "C:\msys64\mingw64"
)

$ErrorActionPreference = "Stop"

$objdump = Join-Path $Mingw "bin\objdump.exe"
if (-not (Test-Path $objdump)) { throw "找不到 objdump：$objdump" }
if (-not (Test-Path $Exe))     { throw "找不到 exe：$Exe" }
if (-not (Test-Path $Dest))    { throw "找不到目标目录：$Dest" }

$binDir = Join-Path $Mingw "bin"

# 解析一个 PE 文件的直接依赖 DLL 名
function Get-Imports([string]$path) {
    & $objdump -p $path 2>$null |
        Select-String '^\s*DLL Name:\s*(\S+)' |
        ForEach-Object { $_.Matches[0].Groups[1].Value }
}

$seen  = New-Object 'System.Collections.Generic.HashSet[string]'
$queue = New-Object 'System.Collections.Generic.Queue[string]'

$exePath = (Resolve-Path $Exe).Path
[void]$seen.Add([System.IO.Path]::GetFileName($exePath))
$queue.Enqueue($exePath)

$copied = 0
while ($queue.Count -gt 0) {
    foreach ($dep in (Get-Imports $queue.Dequeue())) {
        if (-not $seen.Add($dep)) { continue }

        $src = Join-Path $binDir $dep
        if (Test-Path $src) {
            Copy-Item $src -Destination $Dest -Force
            Write-Host "      + $dep"
            $copied++
            $queue.Enqueue($src)
        }
    }
}

Write-Host "      共 $copied 个 MinGW 运行库"
