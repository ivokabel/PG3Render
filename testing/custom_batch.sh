#! /bin/sh

QT4IMAGE="/cygdrive/c/Program Files/Cornell PCG/HDRITools/bin/qt4Image.exe"
PG3RENDER_BASE_DIR="/cygdrive/c/Users/Ivo/Creativity/Programming/05 PG3 Training/PG3 Training/PG3Render/"
PG3RENDER32=$PG3RENDER_BASE_DIR"Win32/Release/PG3Render.exe"
PG3RENDER64=$PG3RENDER_BASE_DIR"x64/Release/PG3Render.exe"
PG3RENDER=$PG3RENDER64

#echo
#pwd
cd "$PG3RENDER_BASE_DIR"
#pwd
#echo

###################################################################################################

for SCENE in `seq 2 7`;  # 6x
do
    for SM in `echo 1 4 8 16`;  # 4x
    do
        for SLB in `echo 1 2`;  # 2x
        do
            for SL in `echo 0.2 0.4 0.6 0.8 1.0`;  # 5x
            do
                #echo Scene: $SCENE sm: $SM slb: $SLB sl: $SL
                "$PG3RENDER" -od ".\output images" -e hdr -a pt -s $scene -t 400 -sm  1 -slb 1 -sl 1.0
                echo
            done
            #echo
        done
        #echo
    done
    #echo
done

###################################################################################################

echo
echo "The script has finished."
read
