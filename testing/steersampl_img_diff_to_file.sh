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

export OPERATION_MODE="render compare plot"     #"render compare plot"

export RENDERING_TIME=15                        #300?
export SCENES=20
export EMS="12 10 11"
export MAX_SUBDIV_LEVEL="7 8 9 10"
export MAX_ERRORS="0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0"   # The only parameter which changes in a graph (1D slice)

TEST_NAME="Scene 20, EMs 12 10"

export CVS_OUTPUT=true                          # false for debugging; ignored if rendering
export CVS_SEPAR=" "
export CVS_DATASETS_IN_COLUMNS=true             # Transpose for Gnuplot; ignored if rendering

###################################################################################################

OUT_FILE_BASE="$IMAGES_BASE_DIR/steer_sampl_tuning/${TEST_NAME}"
OUT_GNUPLOT_FILE="${OUT_FILE_BASE}.gnuplot"
OUT_IMAGE_FILE="${OUT_FILE_BASE}.png"

###################################################################################################

if [[ "$OPERATION_MODE" =~ (^| )"render"($| ) ]]; then
    ./steersampl_img_diff.sh
fi

if [[ "$OPERATION_MODE" =~ (^| )"compare"($| ) ]]; then
    ./steersampl_img_diff.sh | tee "$OUT_GNUPLOT_FILE"
fi

if [[ "$OPERATION_MODE" =~ (^| )"plot"($| ) ]]; then
    # Gnuplot
    echo " 
    set terminal pngcairo size 1200,1080 enhanced font 'Verdana,10'
    set output '$OUT_IMAGE_FILE'
    set title \"${TEST_NAME}\" 
    unset border 
    set yrange [0:]
    plot for [IDX=2:13] '$OUT_GNUPLOT_FILE' using 1:IDX title columnheader with lines
    " | gnuplot
fi
