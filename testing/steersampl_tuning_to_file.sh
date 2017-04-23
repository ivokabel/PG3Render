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

OPERATION_MODES="plot"           #"render compare plot"

SCENES="07 09"          #"07 09 20 22"
declare -A SCENE_ALG_MAP=(
    ["07"]="           pt"
    ["09"]="           pt"
    ["20"]="dlsa dmis    "
    ["22"]="     dmis  pt"
)
EMS="12 10 11"

export ALG_PARAMS="-iic 1"
export RENDERING_TIME=120
export MIN_SUBDIV_LEVEL=04
export MAX_SUBDIV_LEVELS="08 09 10"
export MAX_ERRORS="0.10 0.20 0.30 0.40 0.50 0.75 1.00 1.25 1.50 1.75 2.00 2.50 3.00 3.50 4.00 5.00"   # The only parameter which changes in a graph (1D slice)

#NAME_TRAIL="00 - SSS Reference"
#NAME_TRAIL="01 - Binary"
 NAME_TRAIL="02 - 6 Weights in Parent"

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

            TEST_NAME_BASE="Scn$SCENE em$EM ${ALG}${RENDERING_TIME}sec"
            TEST_NAME=$TEST_NAME_BASE
            if [ "$NAME_TRAIL" != "" ]; then
                TEST_NAME="${TEST_NAME} - $NAME_TRAIL"
            fi
        
            OUT_FILE_BASE="$IMAGES_BASE_DIR/steer_sampl_tuning/${TEST_NAME}"
            OUT_GNUPLOT_FILE="${OUT_FILE_BASE}.gnuplot"
            OUT_IMAGE_FILE="${OUT_FILE_BASE}.png"
            OUT_GNUPLOT_FILE_SSS="$IMAGES_BASE_DIR/steer_sampl_tuning/${TEST_NAME_BASE} - 00 - SSS Reference.gnuplot"

            echo
            echo "================================================================================"
            echo "\"$TEST_NAME\""
            echo "================================================================================"

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
                    # Setup environment
                    set terminal pngcairo size 1200,1080 enhanced font 'Verdana,10'
                    set output '$OUT_IMAGE_FILE'
                    set title \"${TEST_NAME}\"
                    unset border
                    set yrange [0:]

                    # Simple spherical sampler performance for reference
                    stats '$OUT_GNUPLOT_FILE_SSS' every ::1::1 using 2 nooutput
                    sssRefVal = STATS_min

                    # Whole graph
                    plot sssRefVal title \"Simple Spherical Sampler\" dashtype 2 lc rgb \"red\" lw 2, \
                         for [IDX=2:`expr 1 + $SERIES_COUNT`] '$OUT_GNUPLOT_FILE' using 1:IDX title columnheader with lines
                " | gnuplot

                echo
                echo "Generating graph has finished."
            fi

        done
    done
done
