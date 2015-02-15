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

"$PG3RENDER" -a dmis -i 100 -s 5         -od ".\output images" -ot "non_cached_contribs_x64" -e hdr
echo
"$PG3RENDER" -a dmis -i 100 -s 7  -em 9  -od ".\output images" -ot "non_cached_contribs_x64" -e hdr
echo
"$PG3RENDER" -a dmis -i 100 -s 7  -em 11 -od ".\output images" -ot "non_cached_contribs_x64" -e hdr
echo
"$PG3RENDER" -a dmis -i 100 -s 17 -em 9  -od ".\output images" -ot "non_cached_contribs_x64" -e hdr
echo
"$PG3RENDER" -a dmis -i 100 -s 18 -em 9  -od ".\output images" -ot "non_cached_contribs_x64" -e hdr
echo

echo
echo "The script has finished."

read
