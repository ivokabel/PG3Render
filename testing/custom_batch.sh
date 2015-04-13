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

"$PG3RENDER" -e hdr -od ".\output images" -a pt -i   100 -s 2 -iic 20 -ot "1400x425,69fov,transl5"
echo
"$PG3RENDER" -e hdr -od ".\output images" -a pt -i  1000 -s 2 -iic 20 -ot "1400x425,69fov,transl5"
echo
"$PG3RENDER" -e hdr -od ".\output images" -a pt -i 10000 -s 2 -iic 20 -ot "1400x425,69fov,transl5"
echo

echo
echo "The script has finished."
read
