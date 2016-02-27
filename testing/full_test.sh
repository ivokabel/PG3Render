#! /bin/sh

###################################################################################################
# Settings
###################################################################################################

SCENES="25"    #`seq 0 37`               #`seq 0 30`
ENVIRONMENT_MAPS="1 10"         #`seq 0 12`
ITERATIONS_COUNT=4      #256    #32
SHORT_OUTPUT=true
COMPARISON_MODE="generate_references"              # "generate_references", "compare_to_reference"

SCENES_WITH_EM="6 7 8 9 10 12 13 14 15 20 21 22 23 24 25 26 27 28    35 36 37"

TESTING_DIR="./testing"
TESTING_DIR_WIN=".\\testing"
FULL_TEST_OUTPUT_DIR=$TESTING_DIR"/full_test_output"
FULL_TEST_OUTPUT_DIR_WIN=$TESTING_DIR_WIN"\\full_test_output"

###################################################################################################

QT4IMAGE="/cygdrive/c/Program Files/Cornell PCG/HDRITools/bin/qt4Image.exe"

PG3_TRAINING_DIR="/cygdrive/c/Users/Ivo/Creativity/Programming/05 PG3 Training"
PG3_TRAINING_DIR_WIN="C:\\Users\\Ivo\\Creativity\\Programming\\05 PG3 Training"

PG3RENDER_BASE_DIR="$PG3_TRAINING_DIR/PG3 Training/PG3Render/"
PG3RENDER32=$PG3RENDER_BASE_DIR"Win32/Release/PG3Render.exe"
PG3RENDER64=$PG3RENDER_BASE_DIR"x64/Release/PG3Render.exe"
PG3RENDER=$PG3RENDER32 #$PG3RENDER64

#DIFF_TOOL_BASE_DIR="$PG3_TRAINING_DIR/perceptual-diff/Bin/Win32"
 DIFF_TOOL_BASE_DIR="$PG3_TRAINING_DIR/perceptual-diff/BinCMake/Release"
PATH="$DIFF_TOOL_BASE_DIR:$PATH"
#DIFF_TOOL=PerceptualDiff.exe
 DIFF_TOOL=perceptualdiff.exe

TEST_COUNT_TOTAL=0
TEST_COUNT_SUCCESSFUL=0

# $1 ... algorithm
# $2 ... scene idx
# $3 ... environment map (can be empty)
run_single_render () {
    if [ "$SHORT_OUTPUT" = "true" ]; then
        if [ "$3" = "" ]; then
            printf 'Testing scene %d, %-4s, %d iteration(s)... ' $2 $1 $ITERATIONS_COUNT 
        else
            printf 'Testing scene %d, em %d, %-4s, %d iteration(s)... ' $2 $3 $1 $ITERATIONS_COUNT 
        fi
        QUIET_SWITCH="-q"
    else
        echo
        QUIET_SWITCH=""
    fi

    if [ "$COMPARISON_MODE" = "generate_references" ]; then
        OUTPUT_TRAIL="-ot Reference"
    else
        OUTPUT_TRAIL="-ot Current"
    fi

    if [ "$3" = "" ]; then
        REFERENCE_IMG=`"$PG3RENDER" -a $1 -s $2 -i $ITERATIONS_COUNT -e hdr -od "$FULL_TEST_OUTPUT_DIR_WIN" $QUIET_SWITCH -ot Reference -opop`
        RENDERED_IMG=` "$PG3RENDER" -a $1 -s $2 -i $ITERATIONS_COUNT -e hdr -od "$FULL_TEST_OUTPUT_DIR_WIN" $QUIET_SWITCH $OUTPUT_TRAIL -opop`
                       "$PG3RENDER" -a $1 -s $2 -i $ITERATIONS_COUNT -e hdr -od "$FULL_TEST_OUTPUT_DIR_WIN" $QUIET_SWITCH $OUTPUT_TRAIL
    else
        REFERENCE_IMG=`"$PG3RENDER" -a $1 -s $2 -i $ITERATIONS_COUNT -e hdr -od "$FULL_TEST_OUTPUT_DIR_WIN" $QUIET_SWITCH -ot Reference -em $3 -opop`
        RENDERED_IMG=` "$PG3RENDER" -a $1 -s $2 -i $ITERATIONS_COUNT -e hdr -od "$FULL_TEST_OUTPUT_DIR_WIN" $QUIET_SWITCH $OUTPUT_TRAIL -em $3 -opop`
                       "$PG3RENDER" -a $1 -s $2 -i $ITERATIONS_COUNT -e hdr -od "$FULL_TEST_OUTPUT_DIR_WIN" $QUIET_SWITCH $OUTPUT_TRAIL -em $3
    fi

    RENDERING_RESULT=$?

    #echo "Rendering result: $RENDERING_RESULT"

    # Compare to reference
    if [ "$COMPARISON_MODE" = "compare_to_reference" ] && [ $RENDERING_RESULT = 0 ]; then
        "$DIFF_TOOL" -mode rmse -gamma 1.0 -rmsethreshold 0.60 "$RENDERED_IMG" "$REFERENCE_IMG"
        DIFF_RESULT=$?
    fi

    if [ "$RENDERING_RESULT" = "0" ] && [ "$DIFF_RESULT" = "0" ]; then
        ((TEST_COUNT_SUCCESSFUL+=1))
        echo "Passed"
    else
        echo "           $RENDERED_IMG"
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

if [ "$COMPARISON_MODE" = "compare_to_reference" ] then
    rm -f "$FULL_TEST_OUTPUT_DIR"/*_Current.hdr "$FULL_TEST_OUTPUT_DIR"/*_Current.bmp
else
    if [ "$COMPARISON_MODE" = "generate_references" ]; then
        rm -f "$FULL_TEST_OUTPUT_DIR"/*_Reference.hdr "$FULL_TEST_OUTPUT_DIR"/*_Reference.bmp
    fi
fi
#read

mkdir -p "$FULL_TEST_OUTPUT_DIR"

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

echo
echo "The script has finished."
echo "Success: $TEST_COUNT_SUCCESSFUL/$TEST_COUNT_TOTAL"

END_TIME=`date +%s`
TOTAL_TIME=`expr $END_TIME - $START_TIME`
echo "Total execution time:" `display_time $TOTAL_TIME`.

read
