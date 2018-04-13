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

IMCONVERT=/usr/bin/convert

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

stitch_images ()
{
    # array by name
    local INPUT_LIST_NAME=$1[@]
    local INPUT_LIST=("${!INPUT_LIST_NAME}")
    local OUTPUT=$2

    echo "Stitching ${OUTPUT}..."

    local INPUT_PATH_LIST=()
    for INPUT in "${INPUT_LIST[@]}" ; do
        INPUT_PATH_LIST+=("${IMAGES_BASE_DIR}/${INPUT}")
    done

    OUTPUT_PATH="${IMAGES_BASE_DIR}/${OUTPUT}"

    "$IMCONVERT" +append "${INPUT_PATH_LIST[@]}" -resize 50% "${OUTPUT_PATH}"

    echo
}

###################################################################################################

START_TIME=`date +%s`

###################################################################################################


# Blog images

ITERS=512
OT_BASE=Blog
SHOW_BCKG=true
SHOW_BCKG_OT="_Bg"

OUTER_ROUGH=0.00
INNER_ROUGH=1.00
INNER_LAMB_ORANGE=0
INNER_LAMB_ORANGE_OT=""     #"Orange"
THICK=0

# Inner layer - Naive refraction
# White Lambert layer without any modifications
IMG_NAME=${OT_BASE}_InnerOnly_NoModif
INNER_ONLY=true
INNER_REFRACT=false
INNER_REFRACT_OT=""
INNER_FRESNEL=false
INNER_FRESNEL_OT=""
INNER_SOLANGCOMPR=false
INNER_SOLANGCOMPR_OT=""
FILENAME_LIST=()
for EM in 1 7 10; do
              render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_ORANGE} -auxb1 $SHOW_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $INNER_REFRACT -auxb4 $INNER_FRESNEL -auxb5 $INNER_SOLANGCOMPR \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_ORANGE_OT}${INNER_REFRACT_OT}${INNER_FRESNEL_OT}${INNER_SOLANGCOMPR_OT}${SHOW_BCKG_OT}
    FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_ORANGE} -auxb1 $SHOW_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $INNER_REFRACT -auxb4 $INNER_FRESNEL -auxb5 $INNER_SOLANGCOMPR \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_ORANGE_OT}${INNER_REFRACT_OT}${INNER_FRESNEL_OT}${INNER_SOLANGCOMPR_OT}${SHOW_BCKG_OT} \
                     -opof`
    FILENAME_LIST+=("$FILENAME")
done
stitch_images FILENAME_LIST "${IMG_NAME}_${ITERS}s.jpg"

# Inner layer - Naive refraction
# White Lambert layer with refracted directions
IMG_NAME=${OT_BASE}_InnerOnly_NaiveRefr
INNER_REFRACT=true 
INNER_REFRACT_OT="_Refr"
FILENAME_LIST=()
for EM in 1 7 10; do
              render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_ORANGE} -auxb1 $SHOW_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $INNER_REFRACT -auxb4 $INNER_FRESNEL -auxb5 $INNER_SOLANGCOMPR \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_ORANGE_OT}${INNER_REFRACT_OT}${INNER_FRESNEL_OT}${INNER_SOLANGCOMPR_OT}${SHOW_BCKG_OT}
    FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_ORANGE} -auxb1 $SHOW_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $INNER_REFRACT -auxb4 $INNER_FRESNEL -auxb5 $INNER_SOLANGCOMPR \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_ORANGE_OT}${INNER_REFRACT_OT}${INNER_FRESNEL_OT}${INNER_SOLANGCOMPR_OT}${SHOW_BCKG_OT} \
                     -opof`
    FILENAME_LIST+=("$FILENAME")
done
stitch_images FILENAME_LIST "${IMG_NAME}_${ITERS}s.jpg"

# Inner layer - Naive refraction
# White Lambert layer with Fresnel attenuation
IMG_NAME=${OT_BASE}_InnerOnly_NaiveRefr_Fresnel
INNER_FRESNEL=true
INNER_FRESNEL_OT="_Fresnel"
FILENAME_LIST=()
for EM in 1 7 10; do
              render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_ORANGE} -auxb1 $SHOW_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $INNER_REFRACT -auxb4 $INNER_FRESNEL -auxb5 $INNER_SOLANGCOMPR \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_ORANGE_OT}${INNER_REFRACT_OT}${INNER_FRESNEL_OT}${INNER_SOLANGCOMPR_OT}${SHOW_BCKG_OT}
    FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_ORANGE} -auxb1 $SHOW_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $INNER_REFRACT -auxb4 $INNER_FRESNEL -auxb5 $INNER_SOLANGCOMPR \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_ORANGE_OT}${INNER_REFRACT_OT}${INNER_FRESNEL_OT}${INNER_SOLANGCOMPR_OT}${SHOW_BCKG_OT} \
                     -opof`
    FILENAME_LIST+=("$FILENAME")
done
stitch_images FILENAME_LIST "${IMG_NAME}_${ITERS}s.jpg"

# Inner layer - Medium attenuation
# Ideally white (albedo 100%) Lambert layer under brown medium.  Without outer layer.
# (thicknesses: large to none)
IMG_NAME=${OT_BASE}_MediumAttenuation
OUTER_ROUGH=0.00
INNER_ROUGH=1.00
INNER_LAMB_ORANGE=0
INNER_LAMB_ORANGE_OT=""     #"Orange"
MEDIUM_BLUE=0.0
MEDIUM_OT="Brown"
INNER_ONLY=true
INNER_REFRACT=true 
INNER_REFRACT_OT="_Refr"
INNER_FRESNEL=true
INNER_FRESNEL_OT="_Fresnel"
INNER_SOLANGCOMPR=false
INNER_SOLANGCOMPR_OT=""
for EM in 1 7 10; do
    FILENAME_LIST=()
    for THICK in 20.00 5.00 1.00 0.20 0.00; do
                 render -s 27 -em $EM -a dmis -i $ITERS \
                        -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_ORANGE} -auxf5 ${MEDIUM_BLUE} -auxb1 $SHOW_BCKG -auxb2 $INNER_ONLY \
                        -auxb3 $INNER_REFRACT -auxb4 $INNER_FRESNEL -auxb5 $INNER_SOLANGCOMPR \
                        -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_ORANGE_OT}${INNER_REFRACT_OT}${INNER_FRESNEL_OT}${INNER_SOLANGCOMPR_OT}_Thick${THICK}${MEDIUM_OT}${SHOW_BCKG_OT}
        FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                         -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_ORANGE} -auxf5 ${MEDIUM_BLUE} -auxb1 $SHOW_BCKG -auxb2 $INNER_ONLY \
                         -auxb3 $INNER_REFRACT -auxb4 $INNER_FRESNEL -auxb5 $INNER_SOLANGCOMPR \
                         -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_ORANGE_OT}${INNER_REFRACT_OT}${INNER_FRESNEL_OT}${INNER_SOLANGCOMPR_OT}_Thick${THICK}${MEDIUM_OT}${SHOW_BCKG_OT} \
                         -opof`
        FILENAME_LIST+=("$FILENAME")
    done
    stitch_images FILENAME_LIST "${IMG_NAME}_EM${EM}_${ITERS}s.jpg"
done

# Inner layer - Proper refraction
# Missing solid angle problem. Glossy layer under brown medium. (thicknesses: large to none; 3 lights)
# (thicknesses: large to none)
IMG_NAME=${OT_BASE}_InnerMedium_SolAngProblem
INNER_ROUGH=0.10
for EM in 1; do
    FILENAME_LIST=()
    for THICK in 20.00 5.00 1.00 0.20 0.00; do
                  render -s 27 -em $EM -a dmis -i $ITERS \
                         -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_ORANGE} -auxf5 ${MEDIUM_BLUE} -auxb1 $SHOW_BCKG -auxb2 $INNER_ONLY \
                         -auxb3 $INNER_REFRACT -auxb4 $INNER_FRESNEL -auxb5 $INNER_SOLANGCOMPR \
                         -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_ORANGE_OT}${INNER_REFRACT_OT}${INNER_FRESNEL_OT}${INNER_SOLANGCOMPR_OT}_Thick${THICK}${MEDIUM_OT}${SHOW_BCKG_OT}
        FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                         -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_ORANGE} -auxf5 ${MEDIUM_BLUE} -auxb1 $SHOW_BCKG -auxb2 $INNER_ONLY \
                         -auxb3 $INNER_REFRACT -auxb4 $INNER_FRESNEL -auxb5 $INNER_SOLANGCOMPR \
                         -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_ORANGE_OT}${INNER_REFRACT_OT}${INNER_FRESNEL_OT}${INNER_SOLANGCOMPR_OT}_Thick${THICK}${MEDIUM_OT}${SHOW_BCKG_OT} \
                         -opof`
        FILENAME_LIST+=("$FILENAME")
    done
    stitch_images FILENAME_LIST "${IMG_NAME}_EM${EM}_${ITERS}s.jpg"
done

# Inner layer - Proper refraction - BSDF under a smooth refractive interface
# Solid angle compression applied. Glossy layer under brown medium.
# (thicknesses: large to none)
IMG_NAME=${OT_BASE}_InnerMedium_SolAngComp
INNER_ROUGH=0.10
INNER_SOLANGCOMPR=true
INNER_SOLANGCOMPR_OT="_SolAngCompr"
for EM in 1; do
    FILENAME_LIST=()
    for THICK in 20.00 5.00 1.00 0.20 0.00; do
                  render -s 27 -em $EM -a dmis -i $ITERS \
                         -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_ORANGE} -auxf5 ${MEDIUM_BLUE} -auxb1 $SHOW_BCKG -auxb2 $INNER_ONLY \
                         -auxb3 $INNER_REFRACT -auxb4 $INNER_FRESNEL -auxb5 $INNER_SOLANGCOMPR \
                         -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_ORANGE_OT}${INNER_REFRACT_OT}${INNER_FRESNEL_OT}${INNER_SOLANGCOMPR_OT}_Thick${THICK}${MEDIUM_OT}${SHOW_BCKG_OT}
        FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                         -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_ORANGE} -auxf5 ${MEDIUM_BLUE} -auxb1 $SHOW_BCKG -auxb2 $INNER_ONLY \
                         -auxb3 $INNER_REFRACT -auxb4 $INNER_FRESNEL -auxb5 $INNER_SOLANGCOMPR \
                         -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_ORANGE_OT}${INNER_REFRACT_OT}${INNER_FRESNEL_OT}${INNER_SOLANGCOMPR_OT}_Thick${THICK}${MEDIUM_OT}${SHOW_BCKG_OT} \
                         -opof`
        FILENAME_LIST+=("$FILENAME")
    done
    stitch_images FILENAME_LIST "${IMG_NAME}_EM${EM}_${ITERS}s.jpg"
done

# Whole formula
# Highly glossy outer layer, ideally white (albedo 100%) Lambert inner layer and brown medium between them
# (thicknesses: large to none)
IMG_NAME=${OT_BASE}_WholeLambert
OUTER_ROUGH=0.02
INNER_ROUGH=1.00
INNER_ONLY=false
for EM in 1 7 10; do
    FILENAME_LIST=()
    for THICK in 20.00 5.00 1.00 0.20 0.00; do
                 render -s 27 -em $EM -a dmis -i $ITERS \
                        -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_ORANGE} -auxf5 ${MEDIUM_BLUE} -auxb1 $SHOW_BCKG -auxb2 $INNER_ONLY \
                        -auxb3 $INNER_REFRACT -auxb4 $INNER_FRESNEL -auxb5 $INNER_SOLANGCOMPR \
                        -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_ORANGE_OT}${INNER_REFRACT_OT}${INNER_FRESNEL_OT}${INNER_SOLANGCOMPR_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH}${SHOW_BCKG_OT}
        FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                         -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_ORANGE} -auxf5 ${MEDIUM_BLUE} -auxb1 $SHOW_BCKG -auxb2 $INNER_ONLY \
                         -auxb3 $INNER_REFRACT -auxb4 $INNER_FRESNEL -auxb5 $INNER_SOLANGCOMPR \
                         -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_ORANGE_OT}${INNER_REFRACT_OT}${INNER_FRESNEL_OT}${INNER_SOLANGCOMPR_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH}${SHOW_BCKG_OT} \
                         -opof`
        FILENAME_LIST+=("$FILENAME")
    done
    stitch_images FILENAME_LIST "${IMG_NAME}_EM${EM}_${ITERS}s.jpg"
done

# Whole formula
# Highly glossy outer layer, glossy inner layer and brown medium between them
# (thicknesses: large to none)
IMG_NAME=${OT_BASE}_WholeGlossy
INNER_ROUGH=0.20
for EM in 1 7 10; do
    FILENAME_LIST=()
    for THICK in 20.00 5.00 1.00 0.20 0.00; do
                  render -s 27 -em $EM -a dmis -i $ITERS \
                         -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_ORANGE} -auxf5 ${MEDIUM_BLUE} -auxb1 $SHOW_BCKG -auxb2 $INNER_ONLY \
                         -auxb3 $INNER_REFRACT -auxb4 $INNER_FRESNEL -auxb5 $INNER_SOLANGCOMPR \
                         -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_ORANGE_OT}${INNER_REFRACT_OT}${INNER_FRESNEL_OT}${INNER_SOLANGCOMPR_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH}${SHOW_BCKG_OT}
        FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                         -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_ORANGE} -auxf5 ${MEDIUM_BLUE} -auxb1 $SHOW_BCKG -auxb2 $INNER_ONLY \
                         -auxb3 $INNER_REFRACT -auxb4 $INNER_FRESNEL -auxb5 $INNER_SOLANGCOMPR \
                         -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_ORANGE_OT}${INNER_REFRACT_OT}${INNER_FRESNEL_OT}${INNER_SOLANGCOMPR_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH}${SHOW_BCKG_OT} \
                         -opof`
        FILENAME_LIST+=("$FILENAME")
    done
    stitch_images FILENAME_LIST "${IMG_NAME}_EM${EM}_${ITERS}s.jpg"
done



# Layered references: components' contribution analysis

#ITERS=1    #10000
#OT=ArtifactsAnalysis    #DistortionAnalysis   #LayersAnalysis
#INNER_ROUGH=0.01    #1.00
#for EM in 1 7 10; do
#    for THICK in 0.005; do      #0.001 0.010 0.100 0.250 0.500; do
#        render -s 38 -em $EM -a pt -minpl 2 -maxpl 2    -i $ITERS -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -ot Inner${INNER_ROUGH}_Thick${THICK}_${OT}
#        render -s 38 -em $EM -a pt -minpl 3 -maxpl 3    -i $ITERS -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -ot Inner${INNER_ROUGH}_Thick${THICK}_${OT}
#        render -s 38 -em $EM -a pt -minpl 4 -maxpl 4    -i $ITERS -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -ot Inner${INNER_ROUGH}_Thick${THICK}_${OT}
#        render -s 38 -em $EM -a pt -minpl 5 -maxpl 1000 -i $ITERS -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -ot Inner${INNER_ROUGH}_Thick${THICK}_${OT}
#        render -s 38 -em $EM -a pt -minpl 2 -maxpl 4    -i $ITERS -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -ot Inner${INNER_ROUGH}_Thick${THICK}_${OT}
#        render -s 38 -em $EM -a pt -minpl 2 -maxpl 1000 -i $ITERS -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -ot Inner${INNER_ROUGH}_Thick${THICK}_${OT}
#    done
#done

# SRAL model analysis

#REF_ITERS=10000
#MODEL_ITERS=500
#
#REF_THICK=0.005
#MODEL_THICK=0.00
#
#for OUTER_ROUGH in 0.10 0.20 0.30; do  #0.01 0.05
#
#    # Reference: single-scattering, multi-scattering
#    OT=_Reference
#    for EM in 1 7 10; do
#        for INNER_ROUGH in 1.00 0.30 0.10 0.01; do
#             render -s 38 -em $EM -a pt -minpl 2 -maxpl 1000 -i $REF_ITERS -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $REF_THICK -ot Inner${INNER_ROUGH}_Outer${OUTER_ROUGH}_Thick${REF_THICK}${OT}
#        done
#    done
#    for EM in 1 7 10; do
#        for INNER_ROUGH in 1.00 0.30 0.10 0.01; do
#            render -s 38 -em $EM -a pt -minpl 2 -maxpl 4    -i $REF_ITERS -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $REF_THICK -ot Inner${INNER_ROUGH}_Outer${OUTER_ROUGH}_Thick${REF_THICK}${OT}
#        done
#    done
#
#    # Model
#    OT=_NoBg
#    for EM in 1 7 10; do
#        for INNER_ROUGH in 1.00 0.30 0.10 0.01; do
#            render -s 27 -em $EM -a dmis -i $MODEL_ITERS -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $MODEL_THICK -ot RefrGlob_Inner${INNER_ROUGH}_Outer${OUTER_ROUGH}${OT}
#        done
#    done
#
#    # Reference: missing energy
#    OT=_Reference
#    for EM in 1 7 10; do
#        for INNER_ROUGH in 1.00 0.30 0.10 0.01; do
#            render -s 38 -em $EM -a pt -minpl 5 -maxpl 1000 -i $REF_ITERS -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $REF_THICK -ot Inner${INNER_ROUGH}_Outer${OUTER_ROUGH}_Thick${REF_THICK}${OT}
#        done
#    done
#done

# Layered model: Second layer sampling/convergence test

#OT=_InnerOnly_NoBg_ConvTest_SolAngIrrConvPdf
#ITERS=500
#THICK=0.00
#for EM in 10; do     #1 7 10; do
#    for OUTER_ROUGH in 0.01; do
#        for INNER_ROUGH in 1.0 0.50 0.30 0.10; do
#             render -s 27 -em $EM -a dlsa -i $ITERS -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -ot RefrGlob_Inner${INNER_ROUGH}_Outer${OUTER_ROUGH}${OT}
#            #render -s 27 -em $EM -a dbs  -i $ITERS -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -ot RefrGlob_Inner${INNER_ROUGH}_Outer${OUTER_ROUGH}${OT}
#            #render -s 27 -em $EM -a dmis -i $ITERS -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -ot RefrGlob_Inner${INNER_ROUGH}_Outer${OUTER_ROUGH}${OT}
#        done
#    done
#done




# Asserts
#render -s 27 -em 1 -a dbs  -i 128 -auxf1 0.45 -auxf2 0.40 -auxf3 0.00 -ot RefrGlob_Inner0.40_Outer0.45_Thick0.00_IrrConv
#render -s 27 -em 1 -a dlsa -i 128 -auxf1 0.45 -auxf2 0.40 -auxf3 0.00 -ot RefrGlob_Inner0.40_Outer0.45_Thick0.00_IrrConv
#render -s 27 -em 1 -a dmis -i 128 -auxf1 0.45 -auxf2 0.40 -auxf3 0.00 -ot RefrGlob_Inner0.40_Outer0.45_Thick0.00_IrrConv

# Specular-Glossy: Furnace test
#OUTER_ROUGH=0.005
#THICK=0.00
#for EM in 1 10; do
#    for INNER_ROUGH in 0.01 0.05 0.10 0.15 0.20 0.25 0.30 0.35 0.40 0.45; do
#        render -s 27 -em $EM -a dmis -i 128 -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -ot RefrGlob_Inner${INNER_ROUGH}_Outer${OUTER_ROUGH}_Thick${THICK}_IrrConv
#    done
#done

# Specular-Lambert Furnace Test
#THICK=0.00
#for EM in 1 10; do
#    for OUTER_ROUGH in 0.01 0.05 0.10 0.15 0.20 0.25 0.30 0.35 0.40 0.45; do
#        render -s 27 -em $EM -a dmis -i 128 -auxf1 $OUTER_ROUGH -auxf3 $THICK -ot RefrGlob_InnerLambGrey1.0_Outer${OUTER_ROUGH}_MediumYellow_Thick${THICK}_IrrConv
#    done
#done

# Specular-Medium-Glossy: Thickness
#OUTER_ROUGH=0.005
#INNER_ROUGH=0.39
#for EM in 1 10; do
#    for THICK in 0.00 0.01 0.02 0.05 0.10 0.20 0.50 1.00 2.00 5.00; do
#        render -s 27 -em $EM -a dmis -i 128 -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -ot RefrGlob_Inner${INNER_ROUGH}_Outer${OUTER_ROUGH}_MediumYellow_Thick${THICK}_IrrConv
#    done
#done

# Specular-Medium-Glossy: Inner roughness
#OUTER_ROUGH=0.005
#THICK=0.30
#for EM in 10; do        # 1; do
#    for INNER_ROUGH in 0.05 0.10 0.15 0.20 0.25 0.30 0.35 0.40; do
#        render -s 27 -em $EM -a dmis -i 128 -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -ot RefrGlob_Inner${INNER_ROUGH}_Outer${OUTER_ROUGH}_MediumYellow_Thick${THICK}_IrrConv
#    done
#done

# Specular-Medium-Lambert
#for EM in 10 1; do
#    for THICK in 0.00 0.01 0.02 0.05 0.10 0.20 0.50 1.00 2.00 5.00; do
#        render -s 27 -em $EM -a dmis -i 256 -auxf1 0.01 -auxf3 $THICK -ot InnerLambGrey1.0_Outer0.010_RefrGlob_MediumYellow_Thick${THICK}_IrrConv
#    done
#done

### Diff tool comparison modes

#IMAGES_BASE_DIR_WIN="$PG3_TRAINING_DIR_WIN\\PG3 Training\\PG3Render\\output images\\steer_sampl_tuning"
#
#OUT_IMG_DIR_WIN="$IMAGES_BASE_DIR_WIN\\1s_e4_Spd_dlsa"
#REFERENCE_IMG="$OUT_IMG_DIR_WIN\\1s_e4_Spd_dlsa_72000s_Reference.hdr"
#COMPARED_IMG="$OUT_IMG_DIR_WIN\\1s_e4_Spd_dlsa_emss_6s.hdr"
#
#"$DIFF_TOOL" -mode rmsde          -verbose -gamma 1.0 -output "$OUT_IMG_DIR_WIN\\1s_e4_Spd_dlsa_Rmsde.hdr"         "$REFERENCE_IMG" "$COMPARED_IMG"; echo
#"$DIFF_TOOL" -mode msde           -verbose -gamma 1.0 -output "$OUT_IMG_DIR_WIN\\1s_e4_Spd_dlsa_Msde.hdr"          "$REFERENCE_IMG" "$COMPARED_IMG"; echo
#"$DIFF_TOOL" -mode rmsde_relative -verbose -gamma 1.0 -output "$OUT_IMG_DIR_WIN\\1s_e4_Spd_dlsa_RmsdeRelative.hdr" "$REFERENCE_IMG" "$COMPARED_IMG"; echo
#"$DIFF_TOOL" -mode msde_relative  -verbose -gamma 1.0 -output "$OUT_IMG_DIR_WIN\\1s_e4_Spd_dlsa_MsdeRelative.hdr"  "$REFERENCE_IMG" "$COMPARED_IMG"; echo


### SS params tuning - reference images

#BASE_ITERS=20     #12000           #1200=1h   
#
#render -s 22 -a dmis       -i `expr $BASE_ITERS \* 7`  -em 12
#render -s 22 -a dmis       -i `expr $BASE_ITERS \* 2`  -em 10
#render -s 22 -a dmis       -i `expr $BASE_ITERS \* 13` -em 11
#render -s 22 -a dmis       -i `expr $BASE_ITERS \* 6`  -em 4
#
#render -s 20 -a dlsa       -i `expr $BASE_ITERS \* 7`  -em 12
#render -s 20 -a dlsa       -i `expr $BASE_ITERS \* 5`  -em 10
#render -s 20 -a dlsa       -i `expr $BASE_ITERS \* 2`  -em 11
#render -s 20 -a dlsa       -i `expr $BASE_ITERS \* 6`  -em 4

#BASE_ITERS=10     #800            #1=5m5s
#IIC=1 # reduce indirect noise as much as possible
#
#render -s 7  -a pt -iic $IIC -i `expr $BASE_ITERS \* 35` -em 10
#render -s 7  -a pt -iic $IIC -i `expr $BASE_ITERS \* 35` -em 12
#render -s 7  -a pt -iic $IIC -i `expr $BASE_ITERS \* 30` -em 11
#
#render -s 9  -a pt -iic $IIC -i `expr $BASE_ITERS \* 16` -em 10
#render -s 9  -a pt -iic $IIC -i `expr $BASE_ITERS \* 30` -em 12
#render -s 9  -a pt -iic $IIC -i `expr $BASE_ITERS \* 15` -em 11


### Steerable sampler tuning

#SCENE=22
#EMS="12 10 11"
#ITERS=1024
#
#for EM in $EMS; do
#    render -s $SCENE -em $EM -a dmis -i $ITERS -auxf1 0.10 -auxf2 5 -auxf3 7  -auxf4 0   -ot "EmssE0.10Sll5Slu7Tms0"
#    render -s $SCENE -em $EM -a dmis -i $ITERS -auxf1 0.10 -auxf2 5 -auxf3 7  -auxf4 40  -ot "EmssE0.10Sll5Slu7Tms40"
#    render -s $SCENE -em $EM -a dmis -i $ITERS -auxf1 0.10 -auxf2 5 -auxf3 10 -auxf4 40  -ot "EmssE0.10Sll5Slu10Tms40"
#    render -s $SCENE -em $EM -a dmis -i $ITERS -auxf1 0.25 -auxf2 5 -auxf3 10 -auxf4 40  -ot "EmssE0.25Sll5Slu10Tms40"
#    render -s $SCENE -em $EM -a dmis -i $ITERS -auxf1 0.50 -auxf2 5 -auxf3 10 -auxf4 40  -ot "EmssE0.50Sll5Slu10Tms40"
#    render -s $SCENE -em $EM -a dmis -i $ITERS -auxf1 0.75 -auxf2 5 -auxf3 10 -auxf4 40  -ot "EmssE0.75Sll5Slu10Tms40"
#done


### Associative arrays test

#declare -A map=( ["key1"]="key1_val1 key1_val2" ["key2"]="key1_val1 key1_val2" )
#
#for key in "${!map[@]}"; do
#    echo "Key:    $key"
#    echo "Values: ${map[$key]}"
#    for value in ${map[$key]}; do
#       echo "Value:  $value"
#    done
#done


### EM Filtering

#SCENES="20 22 25"
#EMS=4               #"4 10"
#ALGORITHMS="dlsa dbs dmis"
#ITERS="256"
#OT="AvgLuminance"
#for SCENE in $SCENES; do
#    for EM in $EMS; do
#        for ALG in $ALGORITHMS; do
#            for ITER in $ITERS; do
#                render -s $SCENE -em $EM -a $ALG -i $ITER -ot $OT
#            done
#        done
#    done
#done


### EM Samplers

#SCENES="22"
#EMS="12"
#ALGORITHMS="dlsa dbs dmis"
#ITERS="1024"
#OT="SidednessAware"         #"EmssSll5Slu7"

#for SCENE in $SCENES; do
#    for EM in $EMS; do
#        for ALG in $ALGORITHMS; do
#            for ITER in $ITERS; do
#                render -s $SCENE -em $EM -a $ALG -sb 1 -i $ITER
#            done
#        done
#    done
#done

#for PL in `echo 1 2 3 4 5 6`; do
#    render -s 29 -em 3 -a ptn       -i 2 -minpl $PL -maxpl $PL -ot $OT
#    render -s 29 -em 3 -a pt -sb 1  -i 1 -minpl $PL -maxpl $PL -ot $OT
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
#echo `"$DIFF_TOOL" -mode rmsde -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_pl1-100_splt1,1.0,1.0_180sec.hdr"   "$REFERENCE_IMG"`
#echo `"$DIFF_TOOL" -mode rmsde -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_rr_splt1,1.0,1.0_180sec_RR99.7.hdr" "$REFERENCE_IMG"`
#echo `"$DIFF_TOOL" -mode rmsde -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_pl1-100_splt4,1.0,1.0_180sec.hdr"   "$REFERENCE_IMG"`
#echo `"$DIFF_TOOL" -mode rmsde -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_rr_splt4,1.0,1.0_180sec_RR99.7.hdr" "$REFERENCE_IMG"`
#echo `"$DIFF_TOOL" -mode rmsde -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_pl1-100_splt8,1.0,1.0_180sec.hdr"   "$REFERENCE_IMG"`
#echo `"$DIFF_TOOL" -mode rmsde -gamma 1.0 "$IMAGES_BASE_DIR_WIN\\1s_e5_SffdRpd_pt_rr_splt8,1.0,1.0_180sec_RR99.7.hdr" "$REFERENCE_IMG"`

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
#    echo `"$DIFF_TOOL" -mode rmsde -gamma 1.0 "$PATH1" "$REFERENCE_IMG"`
#    echo `"$DIFF_TOOL" -mode rmsde -gamma 1.0 "$PATH2" "$REFERENCE_IMG"`
#    echo `"$DIFF_TOOL" -mode rmsde -gamma 1.0 "$PATH3" "$REFERENCE_IMG"`
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
