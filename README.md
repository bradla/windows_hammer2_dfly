```
cmake -E rm -rf build
cmake -S . -B build -G "NMake Makefiles"
cmake --build build
```

```
fsutil file createnew data.img 268435456     :: 256 MB image
build\newfs_hammer2.exe  -L DATA  data.img
build\hammer2_mount.exe  data.img  M:          :: mount what you just created
```

```
build\hammer2.exe  volume-list  data.img
```
