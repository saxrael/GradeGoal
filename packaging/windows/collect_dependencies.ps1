param(
    [string]$BuildDir = "build/bin",
    [string]$DistDir = "dist"
)

$ErrorActionPreference = "Stop"

$MingwRoot = ""
$PossibleRoots = @(
    $env:MINGW_PREFIX,
    "C:\msys64\mingw64",
    "D:\a\_temp\msys64\mingw64",
    "$env:RUNNER_TEMP\msys64\mingw64",
    "$env:RUNNER_TEMP\_temp\msys64\mingw64",
    "$env:LOCALAPPDATA\msys64\mingw64"
)
foreach ($root in $PossibleRoots) {
    if ($root -and (Test-Path "$root\bin\libgtk-4-1.dll")) {
        $MingwRoot = $root
        break
    }
}

if (-not $MingwRoot) {
    $gtkCmd = Get-Command libgtk-4-1.dll -ErrorAction SilentlyContinue
    if ($gtkCmd) {
        $MingwRoot = Split-Path (Split-Path $gtkCmd.Source -Parent) -Parent
    }
}

if ($MingwRoot -and (Test-Path "$MingwRoot\bin")) {
    $env:PATH = "$MingwRoot\bin;$env:PATH"
}

New-Item -ItemType Directory -Force -Path "$DistDir/bin"

if (Test-Path "$BuildDir/GradeGoal.exe") {
    Copy-Item -Path "$BuildDir/GradeGoal.exe" -Destination "$DistDir/bin/" -Force
}

$DllList = @(
    "libgtk-4-1.dll",
    "libgio-2.0-0.dll",
    "libglib-2.0-0.dll",
    "libgobject-2.0-0.dll",
    "libgmodule-2.0-0.dll",
    "libpango-1.0-0.dll",
    "libpangocairo-1.0-0.dll",
    "libpangoft2-1.0-0.dll",
    "libcairo-2.dll",
    "libcairo-gobject-2.dll",
    "libgdk_pixbuf-2.0-0.dll",
    "libgraphene-1.0-0.dll",
    "libepoxy-0.dll",
    "libharfbuzz-0.dll",
    "libfontconfig-1.dll",
    "libfreetype-6.dll",
    "libfribidi-0.dll",
    "libintl-8.dll",
    "libffi-8.dll",
    "libsqlite3-0.dll",
    "zlib1.dll",
    "libexpat-1.dll",
    "libpng16-16.dll",
    "libwinpthread-1.dll",
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll"
)

foreach ($dll in $DllList) {
    if ($MingwRoot -and (Test-Path "$MingwRoot\bin\$dll")) {
        Copy-Item -Path "$MingwRoot\bin\$dll" -Destination "$DistDir/bin/" -Force
    } else {
        $found = Get-Command $dll -ErrorAction SilentlyContinue
        if ($found) {
            Copy-Item -Path $found.Source -Destination "$DistDir/bin/" -Force
        }
    }
}

if ($MingwRoot -and (Test-Path "$MingwRoot\share\glib-2.0\schemas")) {
    New-Item -ItemType Directory -Force -Path "$DistDir/share/glib-2.0/schemas"
    Copy-Item -Path "$MingwRoot\share\glib-2.0\schemas\*" -Destination "$DistDir/share/glib-2.0/schemas/" -Force -Recurse
}

if ($MingwRoot -and (Test-Path "$MingwRoot\share\icons")) {
    New-Item -ItemType Directory -Force -Path "$DistDir/share/icons"
    Copy-Item -Path "$MingwRoot\share\icons\*" -Destination "$DistDir/share/icons/" -Force -Recurse
}

if ($MingwRoot -and (Test-Path "$MingwRoot\lib\gdk-pixbuf-2.0")) {
    New-Item -ItemType Directory -Force -Path "$DistDir/lib/gdk-pixbuf-2.0"
    Copy-Item -Path "$MingwRoot\lib\gdk-pixbuf-2.0\*" -Destination "$DistDir/lib/gdk-pixbuf-2.0/" -Force -Recurse
}

$glibCompile = Get-Command glib-compile-schemas -ErrorAction SilentlyContinue
if (-not $glibCompile -and $MingwRoot -and (Test-Path "$MingwRoot\bin\glib-compile-schemas.exe")) {
    $glibCompileCmd = "$MingwRoot\bin\glib-compile-schemas.exe"
} elseif ($glibCompile) {
    $glibCompileCmd = $glibCompile.Source
} else {
    $glibCompileCmd = $null
}

if ($glibCompileCmd -and (Test-Path "$DistDir/share/glib-2.0/schemas")) {
    & $glibCompileCmd "$DistDir/share/glib-2.0/schemas"
}
