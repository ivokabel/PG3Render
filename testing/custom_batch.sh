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

#"$PG3RENDER" -a ptnm -i 70000  -s 0 -od ".\output images" -e hdr -ot "WholeBrdfSampling"
#echo
#"$PG3RENDER" -a ptnm -i 20000  -s 6 -od ".\output images" -e hdr -ot "WholeBrdfSampling"
#echo
"$PG3RENDER" -a ptnm -i 50000  -s 7 -od ".\output images" -e hdr -ot "WholeBrdfSampling"
echo
"$PG3RENDER" -a ptnm -i 150000 -s 1 -od ".\output images" -e hdr -ot "WholeBrdfSampling"
echo
"$PG3RENDER" -a ptnm -i 80000  -s 2 -od ".\output images" -e hdr -ot "WholeBrdfSampling"
echo
"$PG3RENDER" -a ptnm -i 150000 -s 3 -od ".\output images" -e hdr -ot "WholeBrdfSampling"
echo
"$PG3RENDER" -a ptnm -i 80000  -s 4 -od ".\output images" -e hdr -ot "WholeBrdfSampling"
echo
"$PG3RENDER" -a ptnm -i 150000 -s 5 -od ".\output images" -e hdr -ot "WholeBrdfSampling"
echo

#"$PG3RENDER" -a dmis  -i 50000  -s 7 -od ".\output images" -e hdr -ot "WholeBrdfSampling"
#echo
#"$PG3RENDER" -a dbs   -i 50000  -s 7 -od ".\output images" -e hdr -ot "WholeBrdfSampling"
#echo
#"$PG3RENDER" -a dlss  -i 100000 -s 7 -od ".\output images" -e hdr -ot "WholeBrdfSampling"
#echo

echo
echo "The script has finished."
read
