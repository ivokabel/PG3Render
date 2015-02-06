#! /bin/sh

QT4IMAGE="/cygdrive/c/Program Files/Cornell PCG/HDRITools/bin/qt4Image.exe"
PG3RENDER_BASE_DIR="/cygdrive/c/Users/Ivo/Creativity/Programming/05 PG3 Training/PG3 Training/PG3Render/"
PG3RENDER64=${PG3RENDER_BASE_DIR}"x64/Release/PG3Render.exe"
PG3RENDER=$PG3RENDER64

TESTING_DIR="./testing"
TESTING_DIR_WIN=".\\testing"
FULL_TEST_OUTPUT_DIR=$TESTING_DIR"/full_test_output"
FULL_TEST_OUTPUT_DIR_WIN=$TESTING_DIR_WIN"\\full_test_output"

ITERATIONS_COUNT=4

# $1 ... scene idx
# $2 ... iterations
# $3 ... environment map (optional)
run_render () {
    echo
    echo "===================================================================="

    if [ "$3" == "" ]; then
        echo "Rendering scene $1, $2 iterations"

        # TODO: delete old output file(s) - we need the name
        #rm -f $1.art $1.artraw $1.tiff

        echo
        "$PG3RENDER" -a dbs  -s $1 -i $2 -e hdr -od "$FULL_TEST_OUTPUT_DIR_WIN"
        echo
        "$PG3RENDER" -a dlss -s $1 -i $2 -e hdr -od "$FULL_TEST_OUTPUT_DIR_WIN"
        echo
        "$PG3RENDER" -a dlsa -s $1 -i $2 -e hdr -od "$FULL_TEST_OUTPUT_DIR_WIN"
        echo
        "$PG3RENDER" -a dmis -s $1 -i $2 -e hdr -od "$FULL_TEST_OUTPUT_DIR_WIN"
    else
        echo "Rendering scene $1, $2 iterations, em $3"

        echo
        "$PG3RENDER" -a dbs  -s $1 -i $2 -e hdr -em $3 -od "$FULL_TEST_OUTPUT_DIR_WIN"
        echo
        "$PG3RENDER" -a dlss -s $1 -i $2 -e hdr -em $3 -od "$FULL_TEST_OUTPUT_DIR_WIN"
        echo
        "$PG3RENDER" -a dlsa -s $1 -i $2 -e hdr -em $3 -od "$FULL_TEST_OUTPUT_DIR_WIN"
        echo
        "$PG3RENDER" -a dmis -s $1 -i $2 -e hdr -em $3 -od "$FULL_TEST_OUTPUT_DIR_WIN"
    fi
}

#echo
#pwd
cd "$PG3RENDER_BASE_DIR"
#pwd
#echo

rm -f "$FULL_TEST_OUTPUT_DIR"/*.hdr "$FULL_TEST_OUTPUT_DIR"/*.bmp
#read

scenes_with_em="6 7 8 9 10 11 12 13 15 16 17"
for scene in `seq 0 17`;
do
    echo
    if [[ $scenes_with_em =~ (^| )$scene($| ) ]]; then
        for em in `seq 0 10`;
        do
            run_render $scene $ITERATIONS_COUNT $em
        done
    else
        run_render $scene $ITERATIONS_COUNT
    fi
done

echo
echo "The script has finished."
read
