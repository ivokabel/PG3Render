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

IMAGES_BASE_DIR_WIN="$PG3_TRAINING_DIR_WIN\\PG3 Training\\PG3Render\\output images\\steer_sampl_tuning"

###################################################################################################

# Debug, normally set by the parent script with optional redirection to a file

#OPERATION_MODE="compare"        #"render compare plot"
#CVS_OUTPUT=true                 # false for debugging; ignored if rendering
#CVS_SEPAR=" "
#CVS_DATASETS_IN_COLUMNS=true    # Transpose for Gnuplot; ignored if rendering

#RENDERING_TIME=15
#SCENES=20
#EMS="12 10"                         #"12 10 11"
#MAX_SUBDIV_LEVEL="7 8 9 10"
#MAX_ERRORS="0.10 0.20 0.30 0.40 0.50"  # The only parameter which changes in a graph (1D slice)

if [[ "$OPERATION_MODE" =~ (^| )"render"($| ) ]]; then
    # Rendering...
    CVS_OUTPUT=false
    CVS_DATASETS_IN_COLUMNS=false
fi

###################################################################################################

#PATH="$DIFF_TOOL_BASE_DIR:$PATH"
#echo $PATH
#echo

#echo
#pwd
cd "$PG3RENDER_BASE_DIR"
#pwd
#echo

###################################################################################################

# $1 ... Reference image
# $2 ... Compared image
# $3 ... Difference image
compare_images () {
    if [ "$CVS_OUTPUT" != "true" ]; then
        echo "Comparing:"
        echo "   $1"
        echo "   $2"
    fi

    "$DIFF_TOOL" -mode rmse -gamma 1.0 -verbosity onlydiff -output "$3" "$1" "$2"

    if [ "$CVS_OUTPUT" != "true" ]; then
        echo
    fi
}

# $1 ... scene id
# $2 ... EM
# $3 ... max subdiv level
# $4 ... max error
# $5 ... rendering time
# $6 ... rendering output directory
# $7 ... reference image path
render_or_compare () {
    if [[ "$OPERATION_MODE" =~ (^| )"render"($| ) ]]; then
        # Rendering...
        mkdir -p "$6"
        "$PG3RENDER" -od "$6" -e hdr -a dlsa -s $1 -em $2 -t $5 -auxf3 $3 -auxf1 $4 -ot "EmssE${4}Sll5Slu${3}Tms40"
        echo
    else
        # Comparing...
        RENDERED_IMG=`"$PG3RENDER" -opop -od "$6" -e hdr -a dlsa -s $1 -em $2 -t $5 -auxf3 $3 -auxf1 $4 -ot "EmssE${4}Sll5Slu${3}Tms40"`
        DIFF_IMG=`    "$PG3RENDER" -opop -od "$6" -e hdr -a dlsa -s $1 -em $2 -t $5 -auxf3 $3 -auxf1 $4 -ot "EmssE${4}Sll5Slu${3}Tms40_Diff"`
        compare_images "$7" "$RENDERED_IMG" "$DIFF_IMG"
    fi
}

# $1 ... scene ID
# $2 ... EM
setup_out_dir_and_img () {
    case $1 in
        20 )
            case $2 in
                10 )
                    OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\1s_e10_Spd_dlsa"
                    REFERENCE_IMG="$OUT_IMG_DIR_WIN\\1s_e10_Spd_dlsa_60000s_Reference.hdr"
                    ;;
                11 )
                    OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\1s_e11_Spd_dlsa"
                    REFERENCE_IMG="$OUT_IMG_DIR_WIN\\1s_e11_Spd_dlsa_24000s_Reference.hdr"
                    ;;
                12 )
                    OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\1s_e12_Spd_dlsa"
                    REFERENCE_IMG="$OUT_IMG_DIR_WIN\\1s_e12_Spd_dlsa_84000s_Reference.hdr"
                    ;;
                * )
                    echo "Unsupported EM!"
                    exit 2
                    ;;
            esac
            ;;
        22 )
            case $2 in
                10 )
                    OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\1s_e10_Spdg_dmis"
                    REFERENCE_IMG="$OUT_IMG_DIR_WIN\\1s_e10_Spdg_dmis_24000s_Reference.hdr"
                    ;;
                11 )
                    OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\1s_e11_Spdg_dmis"
                    REFERENCE_IMG="$OUT_IMG_DIR_WIN\\1s_e11_Spdg_dmis_156000s_Reference.hdr"
                    ;;
                12 )
                    OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\1s_e12_Spdg_dmis"
                    REFERENCE_IMG="$OUT_IMG_DIR_WIN\\1s_e12_Spdg_dmis_84000s_Reference.hdr"
                    ;;
                * )
                    echo "Unsupported EM!"
                    exit 2
                    ;;
            esac
            ;;
        * )
            echo "Unsupported scene!"
            exit 2
            ;;
    esac
}

### Main ##########################################################################################

if [ "$CVS_DATASETS_IN_COLUMNS" != "true" ]; then

    ################################################
    # Datasets are organized in rows
    ################################################

    # CSV header
    if [ "$CVS_OUTPUT" == "true" ]; then
        OUT_STR="\"Scene ID\"${CVS_SEPAR}\"Rendering Time\"${CVS_SEPAR}\"Maximal Triangle Sub-division Level\""
        for ME in $MAX_ERRORS; do
            OUT_STR="$OUT_STR${CVS_SEPAR}$ME"
        done
        echo "$OUT_STR"
    fi

    # Rendering, evaluation
    for SCENE in $SCENES; do
        for EM in $EMS; do
            setup_out_dir_and_img $SCENE $EM
            for MSL in $MAX_SUBDIV_LEVEL; do
                if [ "$CVS_OUTPUT" != "true" ]; then
                    for ME in $MAX_ERRORS; do
                        render_or_compare $SCENE $EM $MSL $ME $RENDERING_TIME "$OUT_IMG_DIR_WIN" "$REFERENCE_IMG"
                    done
                else
                    OUT_STR="$SCENE${CVS_SEPAR}$RENDERING_TIME${CVS_SEPAR}$MSL${CVS_SEPAR}"
                    for ME in $MAX_ERRORS; do
                        COMPARE_STR=`render_or_compare $SCENE $EM $MSL $ME $RENDERING_TIME "$OUT_IMG_DIR_WIN" "$REFERENCE_IMG"`
                        OUT_STR="$OUT_STR${CVS_SEPAR}$COMPARE_STR"
                    done
                    echo "$OUT_STR"
                fi
            done
        done
    done

else

    ################################################
    # Datasets are organized in columns
    ################################################

    # CSV header
    if [ "$CVS_OUTPUT" == "true" ]; then
        OUT_STR="\"Configuration\""
        for SCENE in $SCENES; do
            for EM in $EMS; do
                setup_out_dir_and_img $SCENE $EM
                for MSL in $MAX_SUBDIV_LEVEL; do
                    CONFIG_NAME="\"Scene $SCENE, EM $EM: $MSL at ${RENDERING_TIME}sec\""
                    OUT_STR="$OUT_STR${CVS_SEPAR}$CONFIG_NAME"
                done
            done
        done
        echo "$OUT_STR"
    fi

    # Rendering, evaluation
    for ME in $MAX_ERRORS; do
        OUT_STR="$ME"
        for SCENE in $SCENES; do
            for EM in $EMS; do
                setup_out_dir_and_img $SCENE $EM
                for MSL in $MAX_SUBDIV_LEVEL; do
                    #OUT_STR="$SCENE${CVS_SEPAR}$RENDERING_TIME${CVS_SEPAR}${CVS_SEPAR}$MSL"

                    COMPARE_STR=`render_or_compare $SCENE $EM $MSL $ME $RENDERING_TIME "$OUT_IMG_DIR_WIN" "$REFERENCE_IMG"`
                    OUT_STR="$OUT_STR${CVS_SEPAR}$COMPARE_STR"
                done
            done
        done
        echo "$OUT_STR"
    done

fi


###################################################################################################

if [[ "$OPERATION_MODE" =~ (^| )"render"($| ) ]]; then
    # Rendering...
    echo
    echo "The script has finished."
    read
fi
