#! /bin/sh

QT4IMAGE="/cygdrive/c/Program Files/Cornell PCG/HDRITools/bin/qt4Image.exe"
PG3_TRAINING_DIR="/cygdrive/c/Users/Ivo/Creativity/Programming/05 PG3 Training"
PG3_TRAINING_DIR_WIN="C:\\Users\\Ivo\\Creativity\\Programming\\05 PG3 Training"
PG3RENDER_BASE_DIR="$PG3_TRAINING_DIR/PG3 Training/PG3Render"
PG3RENDER32="$PG3RENDER_BASE_DIR/Win32/Release/PG3Render.exe"
PG3RENDER64="$PG3RENDER_BASE_DIR/x64/Release/PG3Render.exe"
PG3RENDER=$PG3RENDER64

DIFF_TOOL_BASE_DIR="$PG3_TRAINING_DIR/perceptual-diff/Bin/Win32"
PATH="$DIFF_TOOL_BASE_DIR:$PATH"
DIFF_TOOL=PerceptualDiff.exe

IMAGES_BASE_DIR_WIN="$PG3_TRAINING_DIR_WIN\\PG3 Training\\PG3Render\\output images"

#echo
#pwd
cd "$PG3RENDER_BASE_DIR"
#pwd
#echo

###################################################################################################

# Dielectric GGX microfacet testing
for ITERS in `echo 4`; do
    for ALG in `echo pt`; do      #dbs dlsa dmis ptn pt`; do
        for EM in `echo 1`; do        #1 10 12 11 7`; do
            # Glass sphere in air
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 24 -a $ALG -sb 1         -em $EM  -i $ITERS -ot GlassInAir; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 24 -a $ALG -sb 1 -iic 18 -em $EM  -i $ITERS -ot GlassInAir; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4         -em $EM  -i $ITERS -ot GlassInAir_0001 -auxf2 0.001; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4 -iic 18 -em $EM  -i $ITERS -ot GlassInAir_0001 -auxf2 0.001; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4         -em $EM  -i $ITERS -ot GlassInAir_0003 -auxf2 0.003; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4 -iic 18 -em $EM  -i $ITERS -ot GlassInAir_0003 -auxf2 0.003; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4         -em $EM  -i $ITERS -ot GlassInAir_0010 -auxf2 0.010; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4 -iic 18 -em $EM  -i $ITERS -ot GlassInAir_0010 -auxf2 0.010; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4         -em $EM  -i $ITERS -ot GlassInAir_0030 -auxf2 0.030; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4 -iic 18 -em $EM  -i $ITERS -ot GlassInAir_0030 -auxf2 0.030; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4         -em $EM  -i $ITERS -ot GlassInAir_0100 -auxf2 0.100; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4 -iic 18 -em $EM  -i $ITERS -ot GlassInAir_0100 -auxf2 0.100; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4         -em $EM  -i $ITERS -ot GlassInAir_0200 -auxf2 0.200; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4 -iic 18 -em $EM  -i $ITERS -ot GlassInAir_0200 -auxf2 0.200; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4         -em $EM  -i $ITERS -ot GlassInAir_0300 -auxf2 0.300; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4 -iic 18 -em $EM  -i $ITERS -ot GlassInAir_0300 -auxf2 0.300; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4         -em $EM  -i $ITERS -ot GlassInAir_0400 -auxf2 0.400; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4 -iic 18 -em $EM  -i $ITERS -ot GlassInAir_0400 -auxf2 0.400; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4         -em $EM  -i $ITERS -ot GlassInAir_0500 -auxf2 0.500; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4 -iic 18 -em $EM  -i $ITERS -ot GlassInAir_0500 -auxf2 0.500; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4         -em $EM  -i $ITERS -ot GlassInAir_1000 -auxf2 1.000; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 4 -iic 18 -em $EM  -i $ITERS -ot GlassInAir_1000 -auxf2 1.000; echo

            # Air sphere in glass
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 24 -a $ALG -sb 1         -em $EM  -i $ITERS -ot AirInGlass -auxf1 1; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 24 -a $ALG -sb 1 -iic 18 -em $EM  -i $ITERS -ot AirInGlass -auxf1 1; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 1         -em $EM  -i $ITERS -ot AirInGlass_0030 -auxf2 0.030 -auxf1 1; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 26 -a $ALG -sb 1 -iic 18 -em $EM  -i $ITERS -ot AirInGlass_0030 -auxf2 0.030 -auxf1 1; echo

            # Rectangles
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 27 -a $ALG -sb 1         -em $EM  -i $ITERS -ot AdjacentNormal;      echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 28 -a $ALG -sb 1         -em $EM  -i $ITERS -ot AdjacentNormal_0100; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 28 -a $ALG -sb 1 -iic 18 -em $EM  -i $ITERS -ot AdjacentNormal_0100; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 27 -a $ALG -sb 1         -em $EM  -i $ITERS -ot ReverseNormal      -auxf1 1; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 28 -a $ALG -sb 1         -em $EM  -i $ITERS -ot ReverseNormal_0100 -auxf1 1; echo
            "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 28 -a $ALG -sb 1 -iic 18 -em $EM  -i $ITERS -ot ReverseNormal_0100 -auxf1 1; echo
        done

        # Glass sphere in Cornell Box
        "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 30 -a $ALG -sb 4         -i $ITERS -ot GlassInAir; echo
        "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 30 -a $ALG -sb 4 -iic 18 -i $ITERS -ot GlassInAir; echo
        "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 32 -a $ALG -sb 4         -i $ITERS -ot GlassInAir_0001 -auxf2 0.001; echo
        "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 32 -a $ALG -sb 4 -iic 18 -i $ITERS -ot GlassInAir_0001 -auxf2 0.001; echo
        "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 32 -a $ALG -sb 4         -i $ITERS -ot GlassInAir_0010 -auxf2 0.010; echo
        "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 32 -a $ALG -sb 4 -iic 18 -i $ITERS -ot GlassInAir_0010 -auxf2 0.010; echo
        "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 32 -a $ALG -sb 4         -i $ITERS -ot GlassInAir_0100 -auxf2 0.100; echo
        "$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 32 -a $ALG -sb 4 -iic 18 -i $ITERS -ot GlassInAir_0100 -auxf2 0.100; echo
    done
done


#REFERENCE_IMG="$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_rr_splt4,1.0,1.0_82800sec_RR99.7.hdr"
#echo `"$DIFF_TOOL" -mode rmse -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_pl1-100_splt1,1.0,1.0_180sec.hdr"   "$REFERENCE_IMG"`
#echo `"$DIFF_TOOL" -mode rmse -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_rr_splt1,1.0,1.0_180sec_RR99.7.hdr" "$REFERENCE_IMG"`
#echo `"$DIFF_TOOL" -mode rmse -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_pl1-100_splt4,1.0,1.0_180sec.hdr"   "$REFERENCE_IMG"`
#echo `"$DIFF_TOOL" -mode rmse -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_rr_splt4,1.0,1.0_180sec_RR99.7.hdr" "$REFERENCE_IMG"`
#echo `"$DIFF_TOOL" -mode rmse -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_pl1-100_splt8,1.0,1.0_180sec.hdr"   "$REFERENCE_IMG"`
#echo `"$DIFF_TOOL" -mode rmse -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_rr_splt8,1.0,1.0_180sec_RR99.7.hdr" "$REFERENCE_IMG"`

#for EM in `echo 1 12 10`; do
    #"$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 24old -i 10 -a dbs      -em $EM -ot VisNormSampling0010
    #"$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 24old -i 10 -a dlss     -em $EM -ot VisNormSampling0010
    #"$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 24old -i 10 -a dmis     -em $EM -ot VisNormSampling0010
    #"$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 24old -i 10 -a pt -sb 1 -em $EM -ot VisNormSampling0010
    #"$PG3RENDER" -od "$IMAGES_BASE_DIR_WIN" -e hdr -s 24old -i 10 -a pt       -em $EM -ot VisNormSampling0010
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
read
