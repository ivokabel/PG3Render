#! /bin/sh

QT4IMAGE="/cygdrive/c/Program Files/Cornell PCG/HDRITools/bin/qt4Image.exe"
PG3_TRAINING_DIR="/cygdrive/c/Users/Ivo/Creativity/Programming/05 PG3 Training"
PG3_TRAINING_DIR_WIN="C:\\Users\\Ivo\\Creativity\\Programming\\05 PG3 Training"
PG3RENDER_BASE_DIR="$PG3_TRAINING_DIR/PG3 Training/PG3Render"
PG3RENDER32="$PG3RENDER_BASE_DIR/Win32/Release/PG3Render.exe"
PG3RENDER64="$PG3RENDER_BASE_DIR/x64/Release/PG3Render.exe"
PG3RENDER=$PG3RENDER64

#echo
#pwd
cd "$PG3RENDER_BASE_DIR"
#pwd
#echo

###################################################################################################

IMAGES_BASE_DIR_WIN="$PG3_TRAINING_DIR_WIN\\PG3 Training\\PG3Render\\output images"

"$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -a pt -s 7 -t 120 -sm 8 -sl 1.0 -slbr  0.1
"$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -a pt -s 7 -t 120 -sm 8 -sl 1.0 -slbr  1.0
"$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -a pt -s 7 -t 120 -sm 8 -sl 1.0 -slbr 10.0

###################################################################################################

echo
echo "The script has finished."
read
