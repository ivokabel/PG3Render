#! /bin/sh

QT4IMAGE="/cygdrive/c/Program Files/Cornell PCG/HDRITools/bin/qt4Image.exe"
PG3_TRAINING_DIR="/cygdrive/c/Users/Ivo/Creativity/Programming/05 PG3 Training"
PG3_TRAINING_DIR_WIN="C:\\Users\\Ivo\\Creativity\\Programming\\05 PG3 Training"
PG3RENDER_BASE_DIR="$PG3_TRAINING_DIR/PG3 Training/PG3Render"
PG3RENDER32="$PG3RENDER_BASE_DIR/Win32/Release/PG3Render.exe"
PG3RENDER64="$PG3RENDER_BASE_DIR/x64/Release/PG3Render.exe"
PG3RENDER=$PG3RENDER64

DIFF_TOOL_BASE_DIR="$PG3_TRAINING_DIR/perceptual-diff/Bin/Win32"
DIFF_TOOL=PerceptualDiff.exe

IMAGES_BASE_DIR="$PG3_TRAINING_DIR/PG3 Training/PG3Render/output images"
IMAGES_BASE_DIR_WIN="$PG3_TRAINING_DIR_WIN\\PG3 Training\\PG3Render\\output images"

###################################################################################################

PATH="$DIFF_TOOL_BASE_DIR:$PATH"
#echo $PATH
#echo

#echo
#pwd
#cd "$DIFF_TOOL_BASE_DIR"
#pwd
#echo

###################################################################################################

OUT_FILE_BASE="$IMAGES_BASE_DIR/splitting/img_diff_0"
OUT_GNUPLOT_FILE="$OUT_FILE_BASE.gnuplot"
OUT_IMAGE_FILE="${OUT_FILE_BASE}.png"

DO_COMPARE=true

###################################################################################################

export DO_COMPARE

if [ "$DO_COMPARE" != "true" ]; then
    ./splitting_img_diff.sh
else
    export CVS_OUTPUT=true
    export CVS_SEPAR=" "
    export CVS_DATASETS_IN_COLUMNS=true         # Transpose the dataset

    ./splitting_img_diff.sh > "$OUT_GNUPLOT_FILE"

    # Gnuplot
    echo " 
    set terminal pngcairo size 900,900 enhanced font 'Verdana,10'
    set output '$OUT_IMAGE_FILE'
    set title \"Gnuplot test\" 
    unset border 
    set yrange [0:]
    plot for [IDX=2:5] '$OUT_GNUPLOT_FILE' using 1:IDX title columnheader with lines
    " | gnuplot
fi
