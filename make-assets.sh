#! /bin/bash

# Compiles (`make`s) all the source code in the $srcs array, renames them,
# and copies them to the `assets/` directory.

working_dir=${PWD}
# echo ${working_dir}

# Create srcs array
declare -a srcs=("pico/config0" 
                #  "pico2/configdoesnotexist" 
                 "pico2/config0" 
                 "pico2/config1" 
                 "pico2/config21"
                 "pico2/config53"
                 "pico2/config53_at_125mhz"
                 "pimoroni_pico_lipo2xl_w_rp2350/config2"
                 "solderparty_rp2350_stamp_xl/config8"
                )

# Create dests array from srcs array
# e.g. "pico/config0" -> "config0_on_pico"
declare -a dests=()

old_ifs=$IFS
IFS='/' 

for i in ${!srcs[@]}; do
    my_string=${srcs[$i]}
    read -ra my_array <<< $my_string
    dests+=(${my_array[1]}_on_${my_array[0]})
done

IFS=$old_ifs

# Print dests. 
# for i in ${!dests[@]}; do
#     echo ${dests[$i]}
# done

all_src_dirs_exist=1

for i in ${!srcs[@]}; do
    src_dir=build/${srcs[$i]}
    if [ ! -d $src_dir ]; then
        echo $src_dir does not exist.
        all_src_dirs_exist=0
        break
    fi
done

if [ $all_src_dirs_exist = 1 ]; then
    all_srcs_compile=1

    for i in ${!srcs[@]}; do
        echo
        src=${srcs[$i]}
        dest=${dests[$i]}

        # Enter the source directory.
        cd $working_dir/build/$src/
        pwd
        make
        if [ $? -ne 0 ]; then
            echo An error occured compiling the project.
            all_srcs_compile=0
            break
        fi
    done

    if [ $all_srcs_compile = 1 ]; then
        cd $working_dir
        for i in ${!srcs[@]}; do
            echo
            src=${srcs[$i]}
            dest=${dests[$i]}

            # Copy and rename palavo.uf2 to assets folder.
            echo cp build/$src/palavo.uf2 assets/palavo_$dest.uf2
            cp build/$src/palavo.uf2 assets/palavo_$dest.uf2

            # A short delay so that the file timestamps are different enough to
            # be sorted in order of writing when listing the `assets`` directory
            # with `ls assets/ -xt | tac`. A delay of.001 doesn't work, but .01 does.
            sleep .01
        done
    # else
    #     echo At least one of the projects does not compile.
    fi
# else
#     echo At least one of the source directories does not exist.
fi
