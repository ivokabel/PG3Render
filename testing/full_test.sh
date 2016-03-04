#! /bin/sh

###################################################################################################
# Settings
###################################################################################################

SCENES=`seq 0 37`
ENVIRONMENT_MAPS="1 10"                 #`seq 0 12`
ITERATIONS_COUNT=4
SHORT_OUTPUT=true
COMPARISON_MODE="compare_to_reference"  #generate_references"  #"make_references_from_currents"  #"
ARCH_MODES="32 64"

SCENES_WITH_EM="6 7 8 9 10 12 13 14 15 20 21 22 23 24 25 26 27 28    35 36 37"

TESTING_DIR="./testing"
TESTING_DIR_WIN=".\\testing"
FULL_TEST_OUTPUT_DIR="${TESTING_DIR}/full_test_output_${ITERATIONS_COUNT}s"
FULL_TEST_OUTPUT_DIR_WIN="${TESTING_DIR_WIN}\\full_test_output_${ITERATIONS_COUNT}s"

###################################################################################################

QT4IMAGE="/cygdrive/c/Program Files/Cornell PCG/HDRITools/bin/qt4Image.exe"

PG3_TRAINING_DIR="/cygdrive/c/Users/Ivo/Creativity/Programming/05 PG3 Training"
PG3_TRAINING_DIR_WIN="C:\\Users\\Ivo\\Creativity\\Programming\\05 PG3 Training"

PG3RENDER_BASE_DIR="$PG3_TRAINING_DIR/PG3 Training/PG3Render/"
PG3RENDER32=$PG3RENDER_BASE_DIR"Win32/Release/PG3Render.exe"
PG3RENDER64=$PG3RENDER_BASE_DIR"x64/Release/PG3Render.exe"

DIFF_TOOL_BASE_DIR="$PG3_TRAINING_DIR/perceptual-diff/BinCMake/Release"
PATH="$DIFF_TOOL_BASE_DIR:$PATH"
DIFF_TOOL=perceptualdiff.exe

TEST_COUNT_TOTAL=0
TEST_COUNT_SUCCESSFUL=0

# $1 ... algorithm
# $2 ... scene idx
# $3 ... environment map (can be empty)
run_single_render () {
    if [ "$SHORT_OUTPUT" = "true" ]; then
        if [ "$3" = "" ]; then
            printf '[%s] Scene %d, %-4s, %d iteration(s)... ' $arch_mode $2 $1 $ITERATIONS_COUNT 
        else
            printf '[%s] Scene %d, em %2d, %-4s, %d iteration(s)... ' $arch_mode $2 $3 $1 $ITERATIONS_COUNT 
        fi
        QUIET_SWITCH="-q"
    else
        echo
        QUIET_SWITCH=""
    fi

    if [ "$COMPARISON_MODE" = "generate_references" ]; then
        OUTPUT_TRAIL="-ot 1Reference"
    else
        OUTPUT_TRAIL="-ot 2Current"
    fi

    # Generate image names and render
    if [ "$3" = "" ]; then
        REFERENCE_IMG=` "$PG3RENDER" -a $1 -s $2 -i $ITERATIONS_COUNT -e hdr -od "$FULL_TEST_OUTPUT_DIR_ARCH_WIN" $QUIET_SWITCH -ot 1Reference -opop`
        DIFF_IMG=`      "$PG3RENDER" -a $1 -s $2 -i $ITERATIONS_COUNT -e hdr -od "$FULL_TEST_OUTPUT_DIR_ARCH_WIN" $QUIET_SWITCH -ot 3Diff -opop`
        RENDERED_IMG=`  "$PG3RENDER" -a $1 -s $2 -i $ITERATIONS_COUNT -e hdr -od "$FULL_TEST_OUTPUT_DIR_ARCH_WIN" $QUIET_SWITCH $OUTPUT_TRAIL -opop`
                        "$PG3RENDER" -a $1 -s $2 -i $ITERATIONS_COUNT -e hdr -od "$FULL_TEST_OUTPUT_DIR_ARCH_WIN" $QUIET_SWITCH $OUTPUT_TRAIL
    else
        REFERENCE_IMG=` "$PG3RENDER" -a $1 -s $2 -i $ITERATIONS_COUNT -e hdr -od "$FULL_TEST_OUTPUT_DIR_ARCH_WIN" $QUIET_SWITCH -ot 1Reference -em $3 -opop`
        DIFF_IMG=`      "$PG3RENDER" -a $1 -s $2 -i $ITERATIONS_COUNT -e hdr -od "$FULL_TEST_OUTPUT_DIR_ARCH_WIN" $QUIET_SWITCH -ot 3Diff -em $3 -opop`
        RENDERED_IMG=`  "$PG3RENDER" -a $1 -s $2 -i $ITERATIONS_COUNT -e hdr -od "$FULL_TEST_OUTPUT_DIR_ARCH_WIN" $QUIET_SWITCH $OUTPUT_TRAIL -em $3 -opop`
                        "$PG3RENDER" -a $1 -s $2 -i $ITERATIONS_COUNT -e hdr -od "$FULL_TEST_OUTPUT_DIR_ARCH_WIN" $QUIET_SWITCH $OUTPUT_TRAIL -em $3
    fi
    RENDERING_RESULT=$?

    # Compare to reference
    if [ "$COMPARISON_MODE" = "compare_to_reference" ]; then
        if [ $RENDERING_RESULT = 0 ]; then
            "$DIFF_TOOL" -mode rmse -gamma 1.0 -rmsethreshold 0.60 -output "$DIFF_IMG" "$RENDERED_IMG" "$REFERENCE_IMG"
            DIFF_RESULT=$?
            if [ "$DIFF_RESULT" = "0" ]; then
                ((TEST_COUNT_SUCCESSFUL+=1))
                echo "Diff passed"
            else
                echo "           $RENDERED_IMG"
            fi
        else
            echo "Rendering FAILED"
        fi
    else
        if [ $RENDERING_RESULT = 0 ]; then
            ((TEST_COUNT_SUCCESSFUL+=1))
            echo "Reference generated"
        else
            echo "Reference generation FAILED"
        fi
    fi
    ((TEST_COUNT_TOTAL+=1))
}

# $1 ... scene idx
# $2 ... environment map (optional)
run_rendering_set () {
    if [ "$SHORT_OUTPUT" != "true" ]; then
        echo "===================================================================="
    fi

    if [ "$SHORT_OUTPUT" != "true" ]; then
        echo "Rendering scene $1"
    fi

    run_single_render "dbs " $1 $2
    run_single_render "dlss" $1 $2
    run_single_render "dlsa" $1 $2
    run_single_render "dmis" $1 $2
    run_single_render "ptn " $1 $2
    run_single_render "pt  " $1 $2
    echo
}

display_time () {
    local T=$1
    local D=$((T/60/60/24))
    local H=$((T/60/60%24))
    local M=$((T/60%60))
    local S=$((T%60))
    (( $D > 0 )) && printf '%d days ' $D
    (( $H > 0 )) && printf '%d hours ' $H
    (( $M > 0 )) && printf '%d minutes ' $M
    (( $D > 0 || $H > 0 || $M > 0 )) && printf 'and '
    printf '%d seconds\n' $S
}

###################################################################################################

START_TIME=`date +%s`

#echo
#pwd
cd "$PG3RENDER_BASE_DIR"
#pwd
#echo

for arch_mode in $ARCH_MODES;
do
    if [ "$arch_mode" = "32" ]; then
        PG3RENDER=$PG3RENDER32
    else if [ "$arch_mode" = "64" ]; then
        PG3RENDER=$PG3RENDER64
    else
        echo "Unknown architecture was specified!"
        exit
    fi fi

    FULL_TEST_OUTPUT_DIR_ARCH="${FULL_TEST_OUTPUT_DIR}/${arch_mode}"
    FULL_TEST_OUTPUT_DIR_ARCH_WIN="${FULL_TEST_OUTPUT_DIR_WIN}\\${arch_mode}"

    echo "============================="
    echo "Running for architecture: $arch_mode"
    echo "============================="
    echo

    mkdir -p "$FULL_TEST_OUTPUT_DIR_ARCH"

    if [ "$COMPARISON_MODE" = "generate_references" ]; then
        rm -f "$FULL_TEST_OUTPUT_DIR_ARCH"/*_1Reference.hdr "$FULL_TEST_OUTPUT_DIR_ARCH"/*_1Reference.bmp
        rm -f "$FULL_TEST_OUTPUT_DIR_ARCH"/*_2Current.hdr   "$FULL_TEST_OUTPUT_DIR_ARCH"/*_2Current.bmp
        rm -f "$FULL_TEST_OUTPUT_DIR_ARCH"/*_3Diff.hdr      "$FULL_TEST_OUTPUT_DIR_ARCH"/*_3Diff.bmp
    else if [ "$COMPARISON_MODE" = "compare_to_reference" ]; then
        rm -f "$FULL_TEST_OUTPUT_DIR_ARCH"/*_2Current.hdr "$FULL_TEST_OUTPUT_DIR_ARCH"/*_2Current.bmp
        rm -f "$FULL_TEST_OUTPUT_DIR_ARCH"/*_3Diff.hdr    "$FULL_TEST_OUTPUT_DIR_ARCH"/*_3Diff.bmp
    else if [ "$COMPARISON_MODE" = "make_references_from_currents" ]; then
        rm -f "$FULL_TEST_OUTPUT_DIR_ARCH"/*_1Reference.hdr "$FULL_TEST_OUTPUT_DIR_ARCH"/*_1Reference.bmp
        rm -f "$FULL_TEST_OUTPUT_DIR_ARCH"/*_3Diff.hdr      "$FULL_TEST_OUTPUT_DIR_ARCH"/*_3Diff.bmp
        rename '2Current' '1Reference' "$FULL_TEST_OUTPUT_DIR_ARCH/"*_2Current.*
    fi fi fi

    if [ "$COMPARISON_MODE" != "make_references_from_currents" ]; then
        for scene in $SCENES;
        do
            if [[ $SCENES_WITH_EM =~ (^| )$scene($| ) ]]; then
                for em in $ENVIRONMENT_MAPS;
                do
                    run_rendering_set $scene $em
                done
            else
                run_rendering_set $scene
            fi
        done
    fi
done

echo
echo "The script has finished."

TEST_COUNT_UNSUCCESSFUL=`expr $TEST_COUNT_TOTAL - $TEST_COUNT_SUCCESSFUL`
echo "Successful: $TEST_COUNT_SUCCESSFUL, unsuccessful: $TEST_COUNT_UNSUCCESSFUL"

END_TIME=`date +%s`
TOTAL_TIME=`expr $END_TIME - $START_TIME`
echo "Total execution time:" `display_time $TOTAL_TIME`.

read
