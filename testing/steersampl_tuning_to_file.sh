#! /bin/sh

QT4IMAGE="/cygdrive/c/Program Files/Cornell PCG/HDRITools/bin/qt4Image.exe"
PG3_TRAINING_DIR="/cygdrive/c/Users/Ivo/Creativity/Programming/05 PG3 Training"
PG3_TRAINING_DIR_WIN="C:\\Users\\Ivo\\Creativity\\Programming\\05 PG3 Training"
PG3RENDER_BASE_DIR="$PG3_TRAINING_DIR/PG3 Training/PG3Render"
PG3RENDER32="$PG3RENDER_BASE_DIR/Win32/Release/PG3Render.exe"
PG3RENDER64="$PG3RENDER_BASE_DIR/x64/Release/PG3Render.exe"
PG3RENDER=$PG3RENDER64

DIFF_TOOL_BASE_DIR="$PG3_TRAINING_DIR/perceptual-diff/BinCMake/Release"
PATH="$DIFF_TOOL_BASE_DIR:$PATH"
DIFF_TOOL=perceptualdiff.exe

IMAGES_BASE_DIR="$PG3_TRAINING_DIR/PG3 Training/PG3Render/output images"
IMAGES_BASE_DIR_WIN="$PG3_TRAINING_DIR_WIN\\PG3 Training\\PG3Render\\output images"

###################################################################################################

#echo
#pwd
#cd "$DIFF_TOOL_BASE_DIR"
#pwd
#echo

###################################################################################################

OPERATION_MODES="render compare plot"            #"render compare plot"

SCENES="20 22"
EMS="12 10 11"
declare -A SCENE_ALG_MAP=(
    ["20"]="dlsa dmis"
    ["22"]="           pt"          #"     dmis"
)
export RENDERING_TIME=120
export MIN_SUBDIV_LEVEL=04
export MAX_SUBDIV_LEVELS="08 09 10"
export MAX_ERRORS="0.10 0.20 0.30 0.40 0.50 0.75 1.00"      #"0.10 0.20 0.30 0.40 0.50 0.75 1.00 1.25 1.50 1.75 2.00 2.50 3.00 3.50 4.00 5.00"   # The only parameter which changes in a graph (1D slice)

#NAME_TRAIL="Debug"

###################################################################################################

export CVS_OUTPUT=true                          # false for debugging; ignored if rendering
export CVS_SEPAR=" "
export CVS_DATASETS_IN_COLUMNS=true             # Transpose for Gnuplot; ignored if rendering

list_items_count() {
    echo $#;
}

###################################################################################################

export SCENE
export EM
export ALG

for SCENE in $SCENES; do
    for EM in $EMS; do
        for ALG in ${SCENE_ALG_MAP[$SCENE]}; do

            TEST_NAME="Scn$SCENE em$EM ${ALG}${RENDERING_TIME}sec"
            if [ "$NAME_TRAIL" != "" ]; then
                TEST_NAME="${TEST_NAME} - $NAME_TRAIL"
            fi
        
            OUT_FILE_BASE="$IMAGES_BASE_DIR/steer_sampl_tuning/${TEST_NAME}"
            OUT_GNUPLOT_FILE="${OUT_FILE_BASE}.gnuplot"
            OUT_IMAGE_FILE="${OUT_FILE_BASE}.png"

            echo
            echo "========================================"
            echo "   \"$TEST_NAME\""
            echo "========================================"

            if [[ "$OPERATION_MODES" =~ (^| )"render"($| ) ]]; then
                echo

                export OPERATION_MODE=render
                ./steersampl_tuning.sh

                echo "Rendering has finished."
            fi

            if [[ "$OPERATION_MODES" =~ (^| )"compare"($| ) ]]; then
                echo

                export OPERATION_MODE=compare
                ./steersampl_tuning.sh | tee "$OUT_GNUPLOT_FILE"

                echo
                echo "Comparing has finished."
            fi

            if [[ "$OPERATION_MODES" =~ (^| )"plot"($| ) ]]; then
                MAX_SUBDIV_LEVEL_COUNT=`list_items_count $MAX_SUBDIV_LEVELS`
                SERIES_COUNT=$MAX_SUBDIV_LEVEL_COUNT

                echo " 
                    set terminal pngcairo size 1200,1080 enhanced font 'Verdana,10'
                    set output '$OUT_IMAGE_FILE'
                    set title \"${TEST_NAME}\"
                    unset border
                    set yrange [0:]
                    plot for [IDX=2:`expr 1 + $SERIES_COUNT`] '$OUT_GNUPLOT_FILE' using 1:IDX title columnheader with lines
                " | gnuplot

                echo
                echo "Generating graph has finished."
            fi

        done
    done
done
