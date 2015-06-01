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

IMAGES_BASE_DIR_WIN="$PG3_TRAINING_DIR_WIN\\PG3 Training\\PG3Render\\output images"

###################################################################################################

# Debug. Usually set by parent script with eventual redirection to a file
#CVS_OUTPUT=true
#CVS_SEPAR=" "
#CVS_DATASETS_IN_COLUMNS=true    # Transpose the dataset
#DO_COMPARE=true

RENDERING_TIME=500
SCENES="7" #"2 3 6 7"                    #`seq 0 7`
SPLIT_BUDGETS="1 4 8 16"
SLB_RATIOS="1"                  #"1 2"
SPLIT_LEVELS="0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0"

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

# $1 ... image 1
# $2 ... image 2
compare_images () {
    if [ "$CVS_OUTPUT" != "true" ]; then
        echo "Comparing:"
        echo "   $1"
        echo "   $2"
    fi

    "$DIFF_TOOL" -mode rmse -gamma 1.0 "$1" "$2"

    if [ "$CVS_OUTPUT" != "true" ]; then
        echo
    fi
}

# $1 ... scene id
# $2 ... splitting budget
# $3 ... light-to-brdf splitting ratio
# $4 ... splitting level
# $5 ... rendering time
# $6 ... rendering output directory
# $7 ... reference image path
render_and_compare () {
    if [ "$DO_COMPARE" != "true" ]; then
        mkdir -p "$6"
        "$PG3RENDER" -od "$6" -e hdr -a pt -s $1 -t $5 -sm $2 -slbr $3 -sl $4
    else
        RENDERED_IMG=`"$PG3RENDER" -opop -od "$6" -e hdr -a pt -s $1 -t $5 -sm $2 -slbr $3 -sl $4`
        compare_images "$7" "$RENDERED_IMG"
    fi
}

# $1 ... scene ID
setup_out_dir_and_img () {
    case $1 in
        0 )
            OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\splitting analysis\\w2s_p_sdwd_pt_rr_splt"
            REFERENCE_IMG="$OUT_IMG_DIR_WIN\\w2s_p_sdwd_pt_rr_70000s_WholeBrdfSampling.hdr"
            ;;
        1 )
            OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\splitting analysis\\w2s_p_sdsgwdwg_pt_rr_splt"
            REFERENCE_IMG="$OUT_IMG_DIR_WIN\\w2s_p_sdsgwdwg_pt_rr_150000s_WholeBrdfSampling.hdr"
            ;;
        2 )
            OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\splitting analysis\\w2s_c_sdwd_pt_rr_splt"
            REFERENCE_IMG="$OUT_IMG_DIR_WIN\\w2s_c_sdwd_pt_rr_80000s_WholeBrdfSampling.hdr"
            ;;
        3 )
            OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\splitting analysis\\w2s_c_sdsgwdwg_pt_rr_splt"
            REFERENCE_IMG="$OUT_IMG_DIR_WIN\\w2s_c_sdsgwdwg_pt_rr_150000s_WholeBrdfSampling.hdr"
            ;;
        4 )
            OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\splitting analysis\\w2s_b_sdwd_pt_rr_splt"
            REFERENCE_IMG="$OUT_IMG_DIR_WIN\\w2s_b_sdwd_pt_rr_80000s_WholeBrdfSampling.hdr"
            ;;
        5 )
            OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\splitting analysis\\w2s_b_sdsgwdwg_pt_rr_splt"
            REFERENCE_IMG="$OUT_IMG_DIR_WIN\\w2s_b_sdsgwdwg_pt_rr_150000s_WholeBrdfSampling.hdr"
            ;;
        6 )
            OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\splitting analysis\\w2s_e0_sdwd_pt_rr_splt"
            REFERENCE_IMG="$OUT_IMG_DIR_WIN\\w2s_e0_sdwd_pt_rr_20000s_WholeBrdfSampling.hdr"
            ;;
        7 )
            OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\splitting analysis\\w2s_e0_sdsgwdwg_pt_rr_splt"
            REFERENCE_IMG="$OUT_IMG_DIR_WIN\\w2s_e0_sdsgwdwg_pt_rr_50000s_WholeBrdfSampling.hdr"
            ;;
        #10 )
        #    OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\splitting analysis\\w2s_e0_sdwd_pt_rr_splt"
        #    REFERENCE_IMG="$OUT_IMG_DIR_WIN\\w2s_e0_sdwd_pt_rr_20000s_WholeBrdfSampling.hdr"
        #    ;;
        #11 )
        #    OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\splitting analysis\\w2s_e0_sdwd_pt_rr_splt"
        #    REFERENCE_IMG="$OUT_IMG_DIR_WIN\\w2s_e0_sdwd_pt_rr_20000s_WholeBrdfSampling.hdr"
        #    ;;
        #12 )
        #    OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\splitting analysis\\w2s_e0_sdwd_pt_rr_splt"
        #    REFERENCE_IMG="$OUT_IMG_DIR_WIN\\w2s_e0_sdwd_pt_rr_20000s_WholeBrdfSampling.hdr"
        #    ;;
        * )
            exit
            ;;
    esac
}

###################################################################################################

if [ "$CVS_DATASETS_IN_COLUMNS" != "true" ]; then

    ################################################
    # Datasets are organized in rows
    ################################################

    # CSV header
    if [ "$CVS_OUTPUT" == "true" ]; then
        OUT_STR="\"Scene ID\"${CVS_SEPAR}\"Rendering Time\"${CVS_SEPAR}\"Light-to-BRDF Samples Ratio\"${CVS_SEPAR}\"Splitting Budget\""
        for SL in $SPLIT_LEVELS; do
            OUT_STR="$OUT_STR${CVS_SEPAR}$SL"
        done
        echo "$OUT_STR"
    fi

    # Rendering, evaluation
    for SCENE in $SCENES; do
        setup_out_dir_and_img $SCENE
        for SLBR in $SLB_RATIOS; do
            for SBDGT in $SPLIT_BUDGETS; do
                if [ "$CVS_OUTPUT" != "true" ]; then
                    for SL in $SPLIT_LEVELS; do
                        render_and_compare $SCENE $SBDGT $SLBR $SL $RENDERING_TIME "$OUT_IMG_DIR_WIN" "$REFERENCE_IMG"
                    done
                else
                    OUT_STR="$SCENE${CVS_SEPAR}$RENDERING_TIME${CVS_SEPAR}$SLBR${CVS_SEPAR}$SBDGT"
                    for SL in $SPLIT_LEVELS; do
                        COMPARE_STR=`render_and_compare $SCENE $SBDGT $SLBR $SL $RENDERING_TIME "$OUT_IMG_DIR_WIN" "$REFERENCE_IMG"`
                        OUT_STR="$OUT_STR${CVS_SEPAR}$COMPARE_STR"
                    done
                    echo "$OUT_STR"
                fi
            done
        done
    done

elif [ "$CVS_OUTPUT" == "true" ]; then

    ################################################
    # Datasets are organized in rows
    ################################################

    # CSV header
    OUT_STR="\"Configuration\""
    for SCENE in $SCENES; do
        setup_out_dir_and_img $SCENE
        for SLBR in $SLB_RATIOS; do
            for SBDGT in $SPLIT_BUDGETS; do
                CONFIG_NAME="\"Scene $SCENE: $SBDGT, $SLBR at ${RENDERING_TIME}sec\""
                OUT_STR="$OUT_STR${CVS_SEPAR}$CONFIG_NAME"
            done
        done
    done
    echo "$OUT_STR"

    # Rendering, evaluation
    for SL in $SPLIT_LEVELS; do
        OUT_STR="$SL"
        for SCENE in $SCENES; do
            setup_out_dir_and_img $SCENE
            for SLBR in $SLB_RATIOS; do
                for SBDGT in $SPLIT_BUDGETS; do

                    #OUT_STR="$SCENE${CVS_SEPAR}$RENDERING_TIME${CVS_SEPAR}$SLBR${CVS_SEPAR}$SBDGT"

                    COMPARE_STR=`render_and_compare $SCENE $SBDGT $SLBR $SL $RENDERING_TIME "$OUT_IMG_DIR_WIN" "$REFERENCE_IMG"`
                    OUT_STR="$OUT_STR${CVS_SEPAR}$COMPARE_STR"
                done
            done
        done
        echo "$OUT_STR"
    done
else
    echo "Wrong combination of script settings!"
    exit 1
fi


###################################################################################################

if [ "$DO_COMPARE" != "true" ]; then
    echo
    echo "The script has finished."
    read
fi
