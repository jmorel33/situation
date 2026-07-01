$situationPath = "C:\Users\User\Desktop\hobby\_kiro\situation"
$cutoff = (Get-Date).AddHours(-2)
Write-Host "Today: $($today.ToString('yyyy-MM-dd'))"
Write-Host "Folder: $situationPath"
Write-Host ""

Get-ChildItem -Path $situationPath -Recurse -File -ErrorAction SilentlyContinue |
  Where-Object { $_.LastWriteTime -ge $cutoff } |
  Sort-Object LastWriteTime -Descending |
  ForEach-Object {
    "{0:yyyy-MM-dd HH:mm:ss} | {1}" -f $_.LastWriteTime, $_.FullName.Replace("$situationPath\", "")
  }