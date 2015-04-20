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


###################################################################################################

"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 2 -i 100 -sm  1 -slb 1 -sl 1.0
echo
"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 3 -i 100 -sm  1 -slb 1 -sl 1.0
echo
"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 4 -i 100 -sm  1 -slb 1 -sl 1.0
echo
"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 5 -i 100 -sm  1 -slb 1 -sl 1.0
echo
"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 6 -i 100 -sm  1 -slb 1 -sl 1.0
echo
"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 7 -i 100 -sm  1 -slb 1 -sl 1.0
echo

###################################################################################################

#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 4 -t 300 -sm  1 -slb 1 -sl 1.0
#echo

#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 4 -t 300 -sm  4 -slb 1 -sl 0.4
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 4 -t 300 -sm  4 -slb 1 -sl 0.7
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 4 -t 300 -sm  4 -slb 1 -sl 1.0
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 4 -t 300 -sm  4 -slb 2 -sl 0.4
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 4 -t 300 -sm  4 -slb 2 -sl 0.7
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 4 -t 300 -sm  4 -slb 2 -sl 1.0
#echo

#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 4 -t 300 -sm 16 -slb 1 -sl 0.4
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 4 -t 300 -sm 16 -slb 1 -sl 0.7
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 4 -t 300 -sm 16 -slb 1 -sl 1.0
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 4 -t 300 -sm 16 -slb 2 -sl 0.4
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 4 -t 300 -sm 16 -slb 2 -sl 0.7
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 4 -t 300 -sm 16 -slb 2 -sl 1.0
#echo



###################################################################################################

#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 5 -t 300 -sm  1 -slb 1 -sl 0.8
#echo

#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 5 -t 300 -sm  4 -slb 1 -sl 0.4
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 5 -t 300 -sm  4 -slb 1 -sl 0.6
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 5 -t 300 -sm  4 -slb 1 -sl 0.8
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 5 -t 300 -sm  4 -slb 2 -sl 0.4
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 5 -t 300 -sm  4 -slb 2 -sl 0.6
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 5 -t 300 -sm  4 -slb 2 -sl 0.8
#echo

#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 5 -t 300 -sm 16 -slb 1 -sl 0.4
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 5 -t 300 -sm 16 -slb 1 -sl 0.6
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 5 -t 300 -sm 16 -slb 1 -sl 0.8
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 5 -t 300 -sm 16 -slb 2 -sl 0.4
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 5 -t 300 -sm 16 -slb 2 -sl 0.6
#echo
#"$PG3RENDER" -od ".\output images" -e hdr -a pt -s 5 -t 300 -sm 16 -slb 2 -sl 0.8
#echo

echo
echo "The script has finished."
read
