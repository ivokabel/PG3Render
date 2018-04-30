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


# Blog images - Gallery

ITERS=32
OT_BASE=BlogGallery

# Inner Lambert - Outer Smooth

IMG_NAME=${OT_BASE}_LambertOrangeBoostedSmooth
INNER_ROUGH=1.00
OUTER_ROUGH=0.01
INNER_LAMB_COLOUR=3
INNER_LAMB_COLOUR_OT="OrangeBoosted"
THICK=0.0
MEDIUM_BLUE=0.0
MEDIUM_OT=""
#FILENAME_LIST=()
#for EM in 1 7 10; do
#              render -s 27 -em $EM -a dmis -i $ITERS \
#                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} \
#                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH}
#    FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
#                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} \
#                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH} \
#                     -opof`
#    FILENAME_LIST+=("$FILENAME")
#done
#stitch_images FILENAME_LIST "${IMG_NAME}_${ITERS}s.jpg"

# Inner Lambert - Outer Glossy

IMG_NAME=${OT_BASE}_LambertOrangeBoostedGlossy
INNER_ROUGH=1.00
OUTER_ROUGH=0.12
INNER_LAMB_COLOUR=3
INNER_LAMB_COLOUR_OT="OrangeBoosted"
THICK=0.0
MEDIUM_BLUE=0.0
MEDIUM_OT=""
#FILENAME_LIST=()
#for EM in 1 7 10; do
#              render -s 27 -em $EM -a dmis -i $ITERS \
#                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} \
#                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH}
#    FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
#                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} \
#                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH} \
#                     -opof`
#    FILENAME_LIST+=("$FILENAME")
#done
#stitch_images FILENAME_LIST "${IMG_NAME}_${ITERS}s.jpg"

# Inner Smooth - Medium - Outer Smooth

IMG_NAME=${OT_BASE}_SmoothMediumSmooth
INNER_ROUGH=0.01
OUTER_ROUGH=0.01
INNER_LAMB_COLOUR=0
INNER_LAMB_COLOUR_OT=""
THICK=1.0
MEDIUM_BLUE=0.0
MEDIUM_OT="Orange"
#FILENAME_LIST=()
#for EM in 1 7 10; do
#              render -s 27 -em $EM -a dmis -i $ITERS \
#                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} \
#                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH}
#    FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
#                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} \
#                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH} \
#                     -opof`
#    FILENAME_LIST+=("$FILENAME")
#done
#stitch_images FILENAME_LIST "${IMG_NAME}_${ITERS}s.jpg"

# Inner Glossy - Medium - Outer Smooth

IMG_NAME=${OT_BASE}_GlossyMediumSmooth
INNER_ROUGH=0.25
OUTER_ROUGH=0.01
INNER_LAMB_COLOUR=0
INNER_LAMB_COLOUR_OT=""
THICK=1.0
MEDIUM_BLUE=0.0
MEDIUM_OT="Orange"
#FILENAME_LIST=()
#for EM in 1 7 10; do
#              render -s 27 -em $EM -a dmis -i $ITERS \
#                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} \
#                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH}
#    FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
#                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} \
#                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH} \
#                     -opof`
#    FILENAME_LIST+=("$FILENAME")
#done
#stitch_images FILENAME_LIST "${IMG_NAME}_${ITERS}s.jpg"

# Inner Lambert - Medium - Outer Smooth

IMG_NAME=${OT_BASE}_LambertWhiteBoostedMediumSmooth
INNER_ROUGH=1.00
OUTER_ROUGH=0.01
INNER_LAMB_COLOUR=1
INNER_LAMB_COLOUR_OT="WhiteBoosted"
THICK=0.8
MEDIUM_BLUE=0.0
MEDIUM_OT="Orange"
#FILENAME_LIST=()
#for EM in 1 7 10; do
#              render -s 27 -em $EM -a dmis -i $ITERS \
#                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} \
#                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH}
#    FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
#                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} \
#                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH} \
#                     -opof`
#    FILENAME_LIST+=("$FILENAME")
#done
#stitch_images FILENAME_LIST "${IMG_NAME}_${ITERS}s.jpg"

# Inner Glossy - Medium - Outer Glossy

IMG_NAME=${OT_BASE}_GlossyMediumGlossy
INNER_ROUGH=0.22
OUTER_ROUGH=0.12
INNER_LAMB_COLOUR=0
INNER_LAMB_COLOUR_OT=""
THICK=1.0
MEDIUM_BLUE=0.0
MEDIUM_OT="Orange"
FILENAME_LIST=()
for EM in 1 7 10; do
              render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH}
    FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH} \
                     -opof`
    FILENAME_LIST+=("$FILENAME")
done
stitch_images FILENAME_LIST "${IMG_NAME}_${ITERS}s.jpg"



echo
echo "The script has finished."
END_TIME=`date +%s`
TOTAL_TIME=`expr $END_TIME - $START_TIME`
echo "Total execution time:" `display_time $TOTAL_TIME`.
read
exit



# Blog images - The technical part

ITERS=512
OT_BASE=Blog
HIDE_BCKG=false
HIDE_BCKG_OT="_Bg"

OUTER_ROUGH=0.01
INNER_LAMB_COLOUR=0
INNER_LAMB_COLOUR_OT=""     #1="Orange"
MEDIUM_BLUE=0.0
MEDIUM_OT="Orange"
NO_INNER_REFRACT=true
NO_INNER_REFRACT_OT=""
NO_INNER_FRESNEL=true
NO_INNER_FRESNEL_OT=""
NO_INNER_SOLANGCOMPR=true
NO_INNER_SOLANGCOMPR_OT=""
NO_INNER_PDFREFRCOMP=false
NO_INNER_PDFREFRCOMP_OT=""

# Outer layer
IMG_NAME=${OT_BASE}_OuterOnly
INNER_ROUGH=1.00
INNER_ONLY=false
THICK=1000 # Hack: eliminates inner contribution
FILENAME_LIST=()
for EM in 1 7 10; do
              render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH}${HIDE_BCKG_OT}
    FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH}${HIDE_BCKG_OT} \
                     -opof`
    FILENAME_LIST+=("$FILENAME")
done
stitch_images FILENAME_LIST "${IMG_NAME}_${ITERS}s.jpg"


# Inner layer - Naive refraction
# White Lambert layer without any modifications
IMG_NAME=${OT_BASE}_InnerOnly_NoModif
INNER_ROUGH=1.00
THICK=0
INNER_ONLY=true
NO_INNER_REFRACT=true
NO_INNER_REFRACT_OT=""
NO_INNER_FRESNEL=true
NO_INNER_FRESNEL_OT=""
NO_INNER_SOLANGCOMPR=true
NO_INNER_SOLANGCOMPR_OT=""
FILENAME_LIST=()
for EM in 1 7 10; do
              render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}${HIDE_BCKG_OT}
    FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}${HIDE_BCKG_OT} \
                     -opof`
    FILENAME_LIST+=("$FILENAME")
done
stitch_images FILENAME_LIST "${IMG_NAME}_${ITERS}s.jpg"


# Inner layer - Naive refraction
# White Lambert layer with refracted directions
IMG_NAME=${OT_BASE}_InnerOnly_NaiveRefr
NO_INNER_REFRACT=false 
NO_INNER_REFRACT_OT="_Refr"
FILENAME_LIST=()
for EM in 1 7 10; do
              render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}${HIDE_BCKG_OT}
    FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}${HIDE_BCKG_OT} \
                     -opof`
    FILENAME_LIST+=("$FILENAME")
done
stitch_images FILENAME_LIST "${IMG_NAME}_${ITERS}s.jpg"


# Inner layer - Naive refraction
# White Lambert layer with Fresnel attenuation
IMG_NAME=${OT_BASE}_InnerOnly_NaiveRefr_Fresnel
NO_INNER_FRESNEL=false
NO_INNER_FRESNEL_OT="_Fresnel"
FILENAME_LIST=()
for EM in 1 7 10; do
              render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}${HIDE_BCKG_OT}
    FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}${HIDE_BCKG_OT} \
                     -opof`
    FILENAME_LIST+=("$FILENAME")
done
stitch_images FILENAME_LIST "${IMG_NAME}_${ITERS}s.jpg"


# Inner layer - Medium attenuation
# Ideally white (albedo 100%) Lambert layer under brown medium.  Without outer layer.
IMG_NAME=${OT_BASE}_MediumAttenuation
INNER_ROUGH=1.00
INNER_LAMB_COLOUR=0
INNER_LAMB_COLOUR_OT=""     #"Orange"
INNER_ONLY=true
NO_INNER_REFRACT=false 
NO_INNER_REFRACT_OT="_Refr"
NO_INNER_FRESNEL=false
NO_INNER_FRESNEL_OT="_Fresnel"
NO_INNER_SOLANGCOMPR=true
NO_INNER_SOLANGCOMPR_OT=""
for EM in 1 7 10; do
    FILENAME_LIST=()
    for THICK in 20.00 5.00 1.00 0.20 0.00; do
                 render -s 27 -em $EM -a dmis -i $ITERS \
                        -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                        -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                        -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}${HIDE_BCKG_OT}
        FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                         -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                         -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                         -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}${HIDE_BCKG_OT} \
                         -opof`
        FILENAME_LIST+=("$FILENAME")
    done
    stitch_images FILENAME_LIST "${IMG_NAME}_EM${EM}_${ITERS}s.jpg"
done


# Inner layer - Missing solid angle problem.
# Glossy layer under brown medium. (thicknesses: large to none; 3 lights)
IMG_NAME=${OT_BASE}_InnerMedium_SolAngProblem
INNER_ROUGH=0.10
for EM in 1; do
    FILENAME_LIST=()
    for THICK in 20.00 5.00 1.00 0.20 0.00; do
                  render -s 27 -em $EM -a dmis -i $ITERS \
                         -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                         -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                         -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}${HIDE_BCKG_OT}
        FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                         -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                         -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                         -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}${HIDE_BCKG_OT} \
                         -opof`
        FILENAME_LIST+=("$FILENAME")
    done
    stitch_images FILENAME_LIST "${IMG_NAME}_EM${EM}_${ITERS}s.jpg"
done


# Inner layer - Solid angle compression applied.
# Lambert layer
IMG_NAME=${OT_BASE}_InnerLambert_SolAngComp
INNER_ROUGH=1.00
NO_INNER_SOLANGCOMPR=false
NO_INNER_SOLANGCOMPR_OT="_SolAngCompr"
THICK=0.0
FILENAME_LIST=()
for EM in 1 7 10; do
              render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}${HIDE_BCKG_OT}
    FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}${HIDE_BCKG_OT} \
                     -opof`
    FILENAME_LIST+=("$FILENAME")
done
stitch_images FILENAME_LIST "${IMG_NAME}_${ITERS}s.jpg"


# Sampling Inner layer - Darkening problem
IMG_NAME=${OT_BASE}_InnerGlossy_PdfDarkening
INNER_ROUGH=0.04
NO_INNER_PDFREFRCOMP=true
NO_INNER_PDFREFRCOMP_OT="_NoPdfComp"
FILENAME_LIST=()
for EM in 1 7 10; do
              render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}${HIDE_BCKG_OT}
    FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}${HIDE_BCKG_OT} \
                     -opof`
    FILENAME_LIST+=("$FILENAME")
done
stitch_images FILENAME_LIST "${IMG_NAME}_${ITERS}s.jpg"


# Sampling Inner layer - Darkening problem fixed
IMG_NAME=${OT_BASE}_InnerGlossy_PdfDarkeningFixed
NO_INNER_PDFREFRCOMP=false
NO_INNER_PDFREFRCOMP_OT=""
FILENAME_LIST=()
for EM in 1 7 10; do
              render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}${HIDE_BCKG_OT}
    FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                     -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                     -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                     -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}${HIDE_BCKG_OT} \
                     -opof`
    FILENAME_LIST+=("$FILENAME")
done
stitch_images FILENAME_LIST "${IMG_NAME}_${ITERS}s.jpg"


# Inner layer - Solid angle compression applied.
# Glossy layer under brown medium.
IMG_NAME=${OT_BASE}_InnerGlossyMedium_SolAngComp
INNER_ROUGH=0.10
for EM in 1; do
    FILENAME_LIST=()
    for THICK in 20.00 5.00 1.00 0.20 0.00; do
                  render -s 27 -em $EM -a dmis -i $ITERS \
                         -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                         -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                         -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}${HIDE_BCKG_OT}
        FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                         -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                         -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                         -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}${HIDE_BCKG_OT} \
                         -opof`
        FILENAME_LIST+=("$FILENAME")
    done
    stitch_images FILENAME_LIST "${IMG_NAME}_EM${EM}_${ITERS}s.jpg"
done


# Whole formula
# Highly glossy outer layer, ideally white (albedo 100%) Lambert inner layer and brown medium between them
IMG_NAME=${OT_BASE}_WholeLambert
INNER_ROUGH=1.00
INNER_ONLY=false
for EM in 1 7 10; do
    FILENAME_LIST=()
    for THICK in 20.00 5.00 1.00 0.20 0.00; do
                 render -s 27 -em $EM -a dmis -i $ITERS \
                        -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                        -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                        -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH}${HIDE_BCKG_OT}
        FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                         -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                         -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                         -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH}${HIDE_BCKG_OT} \
                         -opof`
        FILENAME_LIST+=("$FILENAME")
    done
    stitch_images FILENAME_LIST "${IMG_NAME}_EM${EM}_${ITERS}s.jpg"
done


# Whole formula
# Highly glossy outer layer, glossy inner layer and brown medium between them
IMG_NAME=${OT_BASE}_WholeGlossy
INNER_ROUGH=0.20
for EM in 1 7 10; do
    FILENAME_LIST=()
    for THICK in 20.00 5.00 1.00 0.20 0.00; do
                  render -s 27 -em $EM -a dmis -i $ITERS \
                         -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                         -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                         -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH}${HIDE_BCKG_OT}
        FILENAME=`render -s 27 -em $EM -a dmis -i $ITERS \
                         -auxf1 $OUTER_ROUGH -auxf2 $INNER_ROUGH -auxf3 $THICK -auxf4 ${INNER_LAMB_COLOUR} -auxf5 ${MEDIUM_BLUE} -auxb1 $HIDE_BCKG -auxb2 $INNER_ONLY \
                         -auxb3 $NO_INNER_REFRACT -auxb4 $NO_INNER_FRESNEL -auxb5 $NO_INNER_SOLANGCOMPR -auxb6 $NO_INNER_PDFREFRCOMP \
                         -ot ${IMG_NAME}_Inner${INNER_ROUGH}${INNER_LAMB_COLOUR_OT}${NO_INNER_REFRACT_OT}${NO_INNER_FRESNEL_OT}${NO_INNER_SOLANGCOMPR_OT}${NO_INNER_PDFREFRCOMP_OT}_Thick${THICK}${MEDIUM_OT}_Outer${OUTER_ROUGH}${HIDE_BCKG_OT} \
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

###################################################################################################

echo
echo "The script has finished."
END_TIME=`date +%s`
TOTAL_TIME=`expr $END_TIME - $START_TIME`
echo "Total execution time:" `display_time $TOTAL_TIME`.
read
