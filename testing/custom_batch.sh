#! /bin/sh

QT4IMAGE="/cygdrive/c/Program Files/Cornell PCG/HDRITools/bin/qt4Image.exe"
PG3_TRAINING_DIR="/cygdrive/c/Users/Ivo/Creativity/Programming/05 PG3 Training"
PG3_TRAINING_DIR_WIN="C:\\Users\\Ivo\\Creativity\\Programming\\05 PG3 Training"
PG3RENDER_BASE_DIR="$PG3_TRAINING_DIR/PG3 Training/PG3Render"
PG3RENDER32="$PG3RENDER_BASE_DIR/Win32/Release/PG3Render.exe"
PG3RENDER64="$PG3RENDER_BASE_DIR/x64/Release/PG3Render.exe"
PG3RENDER=$PG3RENDER64 #$PG3RENDER32 #

DIFF_TOOL_BASE_DIR="$PG3_TRAINING_DIR/perceptual-diff/Bin/Win32"
PATH="$DIFF_TOOL_BASE_DIR:$PATH"
DIFF_TOOL=PerceptualDiff.exe

IMAGES_BASE_DIR="$PG3_TRAINING_DIR/PG3 Training/PG3Render/output images"
IMAGES_BASE_DIR_WIN="$PG3_TRAINING_DIR_WIN\\PG3 Training\\PG3Render\\output images"

#echo
#pwd
cd "$PG3RENDER_BASE_DIR"
#pwd
#echo

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

render () {
    "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr "$@"; echo
}

###################################################################################################

START_TIME=`date +%s`

###################################################################################################

ITERS="1 4 32 128"
SCENES="20"   #"20 28"
EMS="3 4 10"
ALGORITHMS="dlsa"   #"dlsa dlss dbs dmis ptn pt"
OT="EmssSll5Slu7"

for SCENE in $SCENES; do
    for EM in $EMS; do
        for ALG in $ALGORITHMS; do
            for ITER in $ITERS; do
                render -s $SCENE -em $EM -a $ALG -sb 1 -i $ITER -ot $OT
            done
        done
    done
done

#for PL in `echo 1 2 3 4 5 6`; do
#    render -s 28 -em 3 -a ptn       -i 2 -minpl $PL -maxpl $PL -ot $OT
#    render -s 28 -em 3 -a pt -sb 1  -i 1 -minpl $PL -maxpl $PL -ot $OT
#done


#render -s 20 -a el -i 8 -auxf1 12 -auxf2 1000.00 -auxf3 4 -auxf5 0.35 -ot SubDiv_MaxError1000,00_Rot35
#render -s 20 -a el -i 8 -auxf1 12 -auxf2  100.00 -auxf3 4 -auxf5 0.35 -ot SubDiv_MaxError0100,00_Rot35
#render -s 20 -a el -i 8 -auxf1 12 -auxf2   20.00 -auxf3 4 -auxf5 0.35 -ot SubDiv_MaxError0020,00_Rot35
#render -s 20 -a el -i 8 -auxf1 12 -auxf2    5.00 -auxf3 4 -auxf5 0.35 -ot SubDiv_MaxError0005,00_Rot35
#render -s 20 -a el -i 8 -auxf1 12 -auxf2    2.00 -auxf3 4 -auxf5 0.35 -ot SubDiv_MaxError0002,00_Rot35
#render -s 20 -a el -i 8 -auxf1 12 -auxf2    0.90 -auxf3 4 -auxf5 0.35 -ot SubDiv_MaxError0000,90_Rot35
#render -s 20 -a el -i 8 -auxf1 12 -auxf2    0.60 -auxf3 4 -auxf5 0.35 -ot SubDiv_MaxError0000,60_Rot35
#render -s 20 -a el -i 8 -auxf1 12 -auxf2    0.30 -auxf3 4 -auxf5 0.35 -ot SubDiv_MaxError0000,30_Rot35
#render -s 20 -a el -i 8 -auxf1 12 -auxf2    0.10 -auxf3 4 -auxf5 0.35 -ot SubDiv_MaxError0000,10_Rot35
#render -s 20 -a el -i 8 -auxf1 12 -auxf2    0.05 -auxf3 4 -auxf5 0.35 -ot SubDiv_MaxError0000,05_Rot35


ENVIRONMENT_MAPS="1 10 12"
BASE_ITERS=600   #900  #107=1hr
IIC=18

#render -s 24 -a pt -sb 4 -i `expr $BASE_ITERS \* 1`  -iic $IIC -em 10
#render -s 26 -a pt -sb 4 -i `expr $BASE_ITERS \* 2`  -iic $IIC -em 10 -auxf2 0.010 -ot 0010
#render -s 26 -a pt -sb 4 -i `expr $BASE_ITERS \* 8`  -iic $IIC -em 10 -auxf2 0.050 -ot 0050
#render -s 26 -a pt -sb 4 -i `expr $BASE_ITERS \* 10` -iic $IIC -em 10 -auxf2 0.100 -ot 0100
#render -s 26 -a pt -sb 4 -i `expr $BASE_ITERS \* 10` -iic $IIC -em 10 -auxf2 0.150 -ot 0150
#render -s 26 -a pt -sb 4 -i `expr $BASE_ITERS \* 5`  -iic $IIC -em 10 -auxf2 0.400 -ot 0400

#render -s 26 -a pt -sb 4 -i `expr $BASE_ITERS \* 1`  -iic $IIC -em 1  -auxf2 0.040 -ot 0040
#render -s 26 -a pt -sb 4 -i `expr $BASE_ITERS \* 8`  -iic $IIC -em 10 -auxf2 0.040 -ot 0040
#render -s 26 -a pt -sb 4 -i `expr $BASE_ITERS \* 23` -iic $IIC -em 12 -auxf2 0.040 -ot 0040

#render -s 26 -a dmis -i `expr $BASE_ITERS \* 8`  -em 1  -auxf2 0.040 -ot 0040
#render -s 26 -a dmis -i `expr $BASE_ITERS \* 8`  -em 10 -auxf2 0.040 -ot 0040
#render -s 26 -a dmis -i `expr $BASE_ITERS \* 23` -em 12 -auxf2 0.040 -ot 0040

#cd "$IMAGES_BASE_DIR/ggx conductor tiff"
#CONVERT="/bin/convert"
#$CONVERT 1s_e1_Smgd_pt_rr_splt4,1.0,1.0_iic18.0_900s_0040.tiff 1s_e12_Smgd_pt_rr_splt4,1.0,1.0_iic18.0_20700s_0040.tiff 1s_e10_Smgd_pt_rr_splt4,1.0,1.0_iic18.0_7200s_0040.tiff +append rough_dielectric.tiff
#$CONVERT 1s_e1_Smgd_dmis_4800s_0040.tiff 1s_e12_Smgd_dmis_13800s_0040.tiff 1s_e10_Smgd_dmis_4800s_0040.tiff +append rough_dielectric_reflections.tiff




#REFERENCE_IMG="$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_rr_splt4,1.0,1.0_82800sec_RR99.7.hdr"
#echo `"$DIFF_TOOL" -mode rmse -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_pl1-100_splt1,1.0,1.0_180sec.hdr"   "$REFERENCE_IMG"`
#echo `"$DIFF_TOOL" -mode rmse -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_rr_splt1,1.0,1.0_180sec_RR99.7.hdr" "$REFERENCE_IMG"`
#echo `"$DIFF_TOOL" -mode rmse -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_pl1-100_splt4,1.0,1.0_180sec.hdr"   "$REFERENCE_IMG"`
#echo `"$DIFF_TOOL" -mode rmse -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_rr_splt4,1.0,1.0_180sec_RR99.7.hdr" "$REFERENCE_IMG"`
#echo `"$DIFF_TOOL" -mode rmse -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_pl1-100_splt8,1.0,1.0_180sec.hdr"   "$REFERENCE_IMG"`
#echo `"$DIFF_TOOL" -mode rmse -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_rr_splt8,1.0,1.0_180sec_RR99.7.hdr" "$REFERENCE_IMG"`

#for ITERS in `echo 1 5 10 50 100 500`; do
#    "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 24 -a pt -em 10 -i $ITERS -sb 1
#done

#SCENES="0 1 2 3 4 5 6 7"
#for SCENE in $SCENES; do
#    case $SCENE in
#        0 )
#            REFERENCE_IMG="$IMAGES_BASE_DIR_WIN\\w2s_p_sdwd_pt_rr_70000s_WholeBrdfSampling.hdr"
#            ;;
#        1 )
#            REFERENCE_IMG="$IMAGES_BASE_DIR_WIN\\w2s_p_sdsgwdwg_pt_rr_150000s_WholeBrdfSampling.hdr"
#            ;;
#        2 )
#            REFERENCE_IMG="$IMAGES_BASE_DIR_WIN\\w2s_c_sdwd_pt_rr_80000s_WholeBrdfSampling.hdr"
#            ;;
#        3 )
#            REFERENCE_IMG="$IMAGES_BASE_DIR_WIN\\w2s_c_sdsgwdwg_pt_rr_150000s_WholeBrdfSampling.hdr"
#            ;;
#        4 )
#            REFERENCE_IMG="$IMAGES_BASE_DIR_WIN\\w2s_b_sdwd_pt_rr_80000s_WholeBrdfSampling.hdr"
#            ;;
#        5 )
#            REFERENCE_IMG="$IMAGES_BASE_DIR_WIN\\w2s_b_sdsgwdwg_pt_rr_150000s_WholeBrdfSampling.hdr"
#            ;;
#        6 )
#            REFERENCE_IMG="$IMAGES_BASE_DIR_WIN\\w2s_e0_sdwd_pt_rr_20000s_WholeBrdfSampling.hdr"
#            ;;
#        7 )
#            REFERENCE_IMG="$IMAGES_BASE_DIR_WIN\\w2s_e0_sdsgwdwg_pt_rr_50000s_WholeBrdfSampling.hdr"
#            ;;
#        * )
#            exit
#            ;;
#    esac
#
#    #"$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -a pt -s $SCENE -i 40 -ot PowerHeuristics
#
#    PATH1=`"$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -a pt -s $SCENE -i 40 -ot BalanceHeuristics   -opop`
#    PATH2=`"$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -a pt -s $SCENE -i 40 -ot OldPowerHeuristics  -opop`
#    PATH3=`"$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -a pt -s $SCENE -i 40 -ot PowerHeuristics     -opop`
#
#    echo $PATH1
#    echo $PATH2
#    echo $PATH3
#    echo `"$DIFF_TOOL" -mode rmse -gamma 1.0 "$PATH1" "$REFERENCE_IMG"`
#    echo `"$DIFF_TOOL" -mode rmse -gamma 1.0 "$PATH2" "$REFERENCE_IMG"`
#    echo `"$DIFF_TOOL" -mode rmse -gamma 1.0 "$PATH3" "$REFERENCE_IMG"`
#
#    echo
#done

###################################################################################################

echo
echo "The script has finished."
END_TIME=`date +%s`
TOTAL_TIME=`expr $END_TIME - $START_TIME`
echo "Total execution time:" `display_time $TOTAL_TIME`.
read
