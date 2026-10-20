[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0,
               ValueFromRemainingArguments = $true)]
    [ValidateNotNullOrEmpty()]
    [string[]]$Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = [System.IO.Path]::GetFullPath(
    (Join-Path -Path $PSScriptRoot -ChildPath '..\..'))
$formatterPath = Join-Path -Path $projectRoot -ChildPath 'clang-format.exe'
$allowedRoots = @(
    'Bsp',
    'Services',
    'Protocols',
    'App',
    'Config'
)

if(-not (Test-Path -LiteralPath $formatterPath -PathType Leaf))
{
    throw "clang-format.exe was not found at: $formatterPath"
}

foreach($item in $Path)
{
    $resolvedPath = (Resolve-Path -LiteralPath $item).Path
    $extension = [System.IO.Path]::GetExtension($resolvedPath).ToLowerInvariant()
    $relativePath = [System.IO.Path]::GetRelativePath($projectRoot,
                                                       $resolvedPath)
    $topDirectory = $relativePath.Split(
        [System.IO.Path]::DirectorySeparatorChar)[0]

    if('.c' -ne $extension -and '.h' -ne $extension)
    {
        throw "Only C/H files may be formatted: $relativePath"
    }
    if($relativePath.StartsWith('..'))
    {
        throw "The file is outside the project: $resolvedPath"
    }
    if($allowedRoots -notcontains $topDirectory)
    {
        throw "The file is outside the self-developed code roots: $relativePath"
    }

    # Preserve the initial template comment; format the code below it.
    $content = [System.IO.File]::ReadAllText($resolvedPath)
    $header = [regex]::Match($content, '\A(?:\uFEFF)?/\*\*[\s\S]*?\*/')
    $firstLine = 1
    if($header.Success)
    {
        $firstLine = ($header.Value -split "`n").Count + 1
    }
    $lastLine = ($content -split "`n").Count
    if($firstLine -gt $lastLine)
    {
        Write-Output "No code after template header: $relativePath"
        continue
    }

    & $formatterPath -i --style=file --fallback-style=none `
        "--lines=${firstLine}:${lastLine}" $resolvedPath
    if(0 -ne $LASTEXITCODE)
    {
        throw "clang-format failed for: $relativePath"
    }
    Write-Output "Formatted: $relativePath"
}
