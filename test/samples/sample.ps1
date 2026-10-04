# lists big files
param([string]$Path = ".", [int]$Top = 5)

function Get-Big {
    param($Folder)
    Get-ChildItem -Path $Folder -Recurse -File |
        Sort-Object Length -Descending |
        Select-Object -First $Top
}

foreach ($f in Get-Big $Path) {
    if ($f.Length -gt 1MB) {
        Write-Host "$($f.Name) is big"
    }
}
