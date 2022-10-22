cd Release
strip ghost_installer.exe -s -x -X -M -o Taromati2_Installer.exe
upx --ultra-brute --no-align --best --compress-resources=1 Taromati2_Installer.exe
cd ..
