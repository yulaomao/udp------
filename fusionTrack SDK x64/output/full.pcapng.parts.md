# full.pcapng 分片说明

仓库中保存的是以下 3 个分片文件：

- full.pcapng.part01
- full.pcapng.part02
- full.pcapng.part03

在 PowerShell 中可使用下面的脚本恢复原文件：

```powershell
$parts = @(
  "full.pcapng.part01",
  "full.pcapng.part02",
  "full.pcapng.part03"
)

$output = "full.pcapng"
$stream = [System.IO.File]::Create($output)
try {
  foreach ($part in $parts) {
    $bytes = [System.IO.File]::ReadAllBytes($part)
    $stream.Write($bytes, 0, $bytes.Length)
  }
}
finally {
  $stream.Dispose()
}
```