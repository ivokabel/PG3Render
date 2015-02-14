#! /bin/sh

QT4IMAGE="/cygdrive/c/Program Files/Cornell PCG/HDRITools/bin/qt4Image.exe"
PG3RENDER_BASE_DIR="/cygdrive/c/Users/Ivo/Creativity/Programming/05 PG3 Training/PG3 Training/PG3Render/"
PG3RENDER64=$PG3RENDER_BASE_DIR"x64/Release/PG3Render.exe"
PG3RENDER=$PG3RENDER64

TESTING_DIR="./testing"
TESTING_DIR_WIN=".\\testing"
FULL_TEST_OUTPUT_DIR=$TESTING_DIR"/full_test_output"
FULL_TEST_OUTPUT_DIR_WIN=$TESTING_DIR_WIN"\\full_test_output"

ITERATIONS_COUNT=100
SHORT_OUTPUT=true

TEST_COUNT_TOTAL=0
TEST_COUNT_SUCCESSFUL=0

# $1 ... algorithm
# $2 ... scene idx
run_single_render () {
    if [ "$SHORT_OUTPUT" = "true" ]; then
        echo "Rendering: scene $2, $1, $ITERATIONS_COUNT iteration(s)"
        QUIET_SWITCH="-q"
    else
        echo
        QUIET_SWITCH=""
    fi

    "$PG3RENDER" -a $1 -s $2 -i $ITERATIONS_COUNT -e hdr -od "$FULL_TEST_OUTPUT_DIR_WIN" $QUIET_SWITCH

    [ "$?" = "0" ] && ((TEST_COUNT_SUCCESSFUL+=1))
    ((TEST_COUNT_TOTAL+=1))
}

# dtto
# $3 ... environment map
run_single_render_em () {
    if [ "$SHORT_OUTPUT" = "true" ]; then
        echo "Rendering: scene $2, em $3, $1, $ITERATIONS_COUNT iteration(s)"
        QUIET_SWITCH="-q"
    else
        echo
        QUIET_SWITCH=""
    fi

    "$PG3RENDER" -a $1 -s $2 -i $ITERATIONS_COUNT -e hdr -em $3 -od "$FULL_TEST_OUTPUT_DIR_WIN" $QUIET_SWITCH

    [ "$?" = "0" ] && ((TEST_COUNT_SUCCESSFUL+=1))
    ((TEST_COUNT_TOTAL+=1))
}

# $1 ... scene idx
# $2 ... environment map (optional)
run_rendering_set () {
    echo
    if [ "$SHORT_OUTPUT" != "true" ]; then
        echo "===================================================================="
    fi

    if [ "$2" == "" ]; then
        if [ "$SHORT_OUTPUT" != "true" ]; then
            echo "Rendering scene $1"
        fi

        run_single_render "dbs " $1
        run_single_render dlss $1
        run_single_render dlsa $1
        run_single_render dmis $1
    else
        if [ "$SHORT_OUTPUT" != "true" ]; then
            echo "Rendering scene $1, em $2"
        fi

        run_single_render_em "dbs " $1 $2
        run_single_render_em dlss $1 $2
        run_single_render_em dlsa $1 $2
        run_single_render_em dmis $1 $2
    fi
}

#echo
#pwd
cd "$PG3RENDER_BASE_DIR"
#pwd
#echo

rm -f "$FULL_TEST_OUTPUT_DIR"/*.hdr "$FULL_TEST_OUTPUT_DIR"/*.bmp
#read

scenes_with_em="6 7 8 9 10 11 12 13 15 16 17 18"
for scene in `seq 0 18`;
do
    if [[ $scenes_with_em =~ (^| )$scene($| ) ]]; then
        for em in `seq 0 11`;
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

read
