param(
    [string]$BuildDir = "build/bin",
    [string]$DistDir = "dist"
)

$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Force -Path "$DistDir/bin"
New-Item -ItemType Directory -Force -Path "$DistDir/share/glib-2.0/schemas"
New-Item -ItemType Directory -Force -Path "$DistDir/share/icons"
New-Item -ItemType Directory -Force -Path "$DistDir/lib/gdk-pixbuf-2.0"

Copy-Item -Path "$BuildDir/GradeGoal.exe" -Destination "$DistDir/bin/" -Force

$DllList = @(
    "libgtk-4-1.dll",
    "libgio-2.0-0.dll",
    "libglib-2.0-0.dll",
    "libgobject-2.0-0.dll",
    "libpango-1.0-0.dll",
    "libcairo-2.dll",
    "libgdk_pixbuf-2.0-0.dll",
    "libsqlite3-0.dll",
    "libxlsxwriter.dll",
    "libxlsxio_read.dll",
    "libhpdf.dll",
    "zlib1.dll",
    "libexpat-1.dll",
    "libpng16-16.dll",
    "libwinpthread-1.dll",
    "libgcc_s_seh-1.dll"
)

foreach ($dll in $DllList) {
    $found = Get-Command $dll -ErrorAction SilentlyContinue
    if ($found) {
        Copy-Item -Path $found.Source -Destination "$DistDir/bin/" -Force
    }
}

if (Get-Command glib-compile-schemas -ErrorAction SilentlyContinue) {
    & glib-compile-schemas "$DistDir/share/glib-2.0/schemas"
}
