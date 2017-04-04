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

OPERATION_MODES="render compare plot"

export RENDERING_TIME=300
export SCENES=20
export EMS="12 10 11"
export MIN_SUBDIV_LEVEL=4
export MAX_SUBDIV_LEVELS="07 08 09 10 11"
export MAX_ERRORS="0.20 0.30 0.40 0.50 0.60 0.70 0.80 0.90 1.00 1.10 1.20"   # The only parameter which changes in a graph (1D slice)

export CVS_OUTPUT=true                          # false for debugging; ignored if rendering
export CVS_SEPAR=" "
export CVS_DATASETS_IN_COLUMNS=true             # Transpose for Gnuplot; ignored if rendering

###################################################################################################

TEST_NAME="Scene $SCENES, EMs $EMS"
OUT_FILE_BASE="$IMAGES_BASE_DIR/steer_sampl_tuning/${TEST_NAME}"
OUT_GNUPLOT_FILE="${OUT_FILE_BASE}.gnuplot"
OUT_IMAGE_FILE="${OUT_FILE_BASE}.png"

list_items_count() {
    echo $#;
}

###################################################################################################

if [[ "$OPERATION_MODES" =~ (^| )"render"($| ) ]]; then
    export OPERATION_MODE=render

    ./steersampl_img_diff.sh

    echo
    echo "Rendering has finished."
fi

if [[ "$OPERATION_MODES" =~ (^| )"compare"($| ) ]]; then
    export OPERATION_MODE=compare

    ./steersampl_img_diff.sh | tee "$OUT_GNUPLOT_FILE"

    echo
    echo "Comparing has finished."
fi

if [[ "$OPERATION_MODES" =~ (^| )"plot"($| ) ]]; then
    SCENE_COUNT=`list_items_count $SCENES`
    EM_COUNT=`list_items_count $EMS`
    MAX_SUBDIV_LEVEL_COUNT=`list_items_count $MAX_SUBDIV_LEVELS`
    SERIES_COUNT=`expr $SCENE_COUNT \* $EM_COUNT \* $MAX_SUBDIV_LEVEL_COUNT`

    echo " 
        set terminal pngcairo size 1200,1080 enhanced font 'Verdana,10'
        set output '$OUT_IMAGE_FILE'
        set title \"${TEST_NAME}\"
        unset border
        set yrange [0:]
        plot for [IDX=2:`expr 1 + $SERIES_COUNT`] '$OUT_GNUPLOT_FILE' using 1:IDX title columnheader with lines
    " | gnuplot

    #echo " 
    #    set terminal pngcairo size 1200,1080 enhanced font 'Verdana,10'
    #    set output '$IMAGES_BASE_DIR/steer_sampl_tuning/Scene 20, EMs 12.png'
    #    set title \"Scene 20, EMs 12\"
    #    unset border
    #    set yrange [0:]
    #    plot for [IDX=2:6] '$OUT_GNUPLOT_FILE' using 1:IDX title columnheader with lines
    #" | gnuplot

    echo
    echo "Generating graph has finished."
fi
