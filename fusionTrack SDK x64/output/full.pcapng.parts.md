# full.pcapng 分卷说明

仓库中保存的是以下 3 个可独立读取和解析的 pcapng 文件：

- full_01.pcapng
- full_02.pcapng
- full_03.pcapng

这些文件由仓库根目录的 split_pcapng.py 生成，按抓包记录数均分。

重新生成时可在仓库根目录执行：

```powershell
python split_pcapng.py "fusionTrack SDK x64/output/full.pcapng" --parts 3
```