#! /bin/sh

QT4IMAGE="/cygdrive/c/Program Files/Cornell PCG/HDRITools/bin/qt4Image.exe"
PG3RENDER_BASE_DIR="/cygdrive/c/Users/Ivo/Creativity/Programming/05 PG3 Training/PG3 Training/PG3Render/"
PG3RENDER32=$PG3RENDER_BASE_DIR"Win32/Release/PG3Render.exe"
PG3RENDER64=$PG3RENDER_BASE_DIR"x64/Release/PG3Render.exe"
PG3RENDER=$PG3RENDER64

#echo
#pwd
cd "$PG3RENDER_BASE_DIR"
#pwd
#echo

"$PG3RENDER" -a pt         -i 1000 -s 9 -em 4 -od ".\output images" -e hdr
echo

#-od ".\output images" -e hdr -a pt -s 5 -t 180 -sm 1 -slb 1 -sl 1 -ot "SplitBudget8SplitLevel1"

echo
echo "The script has finished."
read
