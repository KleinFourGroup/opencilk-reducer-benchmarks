#!/bin/bash

#### Fixed variables

CC=$HOME/opencilk/build/bin/clang
CXX=$HOME/opencilk/build/bin/clang++
SENT=$HOME/opencilk/opencilk-project/cheetah/include/cilk/sentinel.h
# Default: find /usr -name "libprofiler.*"
LIBPROF=/usr/lib/x86_64-linux-gnu/libprofiler.so.0
if [ ! -e $LIBPROF ]
then
    CANPROFILE=0
else
    CANPROFILE=1
fi

# PERF=perf.csv

PERFOPT=perf_opt.csv
PERFCOMM=perf_comm.csv
PERFSCALE=perf_scale.csv

OPT=-O3

#### Test identifiers
ISOPTTEST=1
ISCOMMTEST=2
ISSCALETEST=3

#### Commuative reducer test codes
CT_S=0
CT_A=1
CT_C=2
COMMTEST=$CT_S

#### Formatting
SEP="\t"
ENDROW=''
SECTION="####"
COMMENT="    "
COMMENT2="        "

# Default: 1, but it's truly irrelevant
NWORKERS=1
# Default: 0
FULLTEST=0

# Overhead codes
SG=0
SR=1
CR=2
CG=3

CONF=""
CONFSUF=""

#### Parsing arguments

parse_testexp()
{
    case "$1" in
        full) set_testexp "111";;
        opt) set_testexp "100";;
        comm) set_testexp "010";;
        scale) set_testexp "001";;
        *) set_testexp "$1";;
    esac
}

parse_confexp()
{
    case "$1" in
        full) set_confexp "...";;
        direct) set_confexp "(000)|(111)";;
        *) set_confexp "$1";;
    esac
}

parse_range()
{
    case "$1" in
        full) set_range 1 8;;
        fast) set_range 1 3;;
        serial) set_range 1 1;;
        max) set_range 8 8;;
        *) echo "Unrecognized input setting; defaulting to 'fast'"; set_range 1 3;;
    esac
}

parse_inputs()
{
    case "$1" in
        full) set_inputs 100 3 20000000 dedup/data/simlarge;;
        mid) set_inputs 60 2 15000000 dedup/data/simmedium;;
        fast) set_inputs 1 -1 10000000 dedup/data/simsmall;;
        *) echo "Unrecognized input setting; defaulting to 'fast'";  set_inputs 1 -1 10000000 dedup/data/simsmall;;
    esac
}

set_spoof()
{
    SPOOF=$1
}

set_fast_hyperobjects()
{
    FASTHYPER=$1
}

set_bigspa()
{
    BIGSPA=$1
}

set_reps()
{
    REPS=$1
}

set_testexp()
{
    TESTEXP="$1"
}

set_confexp()
{
    CONFEXP="$1"
}

set_profiling()
{
    if [[ $1 -eq 0 ]]
    then
        PROFILE=0
        PROFILEPARALLEL=0
    elif [[ $1 -eq 1 ]]
    then
        PROFILE=1
        PROFILEPARALLEL=0
    elif [[ $1 -eq 2 ]]
    then
        PROFILE=1
        PROFILEPARALLEL=1
    else
        PROFILE=0
        PROFILEPARALLEL=0
    fi
}

set_range()
{
    STARTNWORKERS=$1
    STOPNWORKERS=$2
}

set_inputs()
{
    # Default: 100
    INTMULT=$1
    # Default: 3
    FIBOFFSET=$2
    # Default: 20000000
    FFTN=$3
    # Default: dedup/data/simlarge
    DEDUP_TEST=$4

    INTSUM=$((25000000*$INTMULT))
    FIB=$((40+$FIBOFFSET))
    HIST=(256 $(($INTSUM*2/5)))
    INTLIST=$((2000000*$INTMULT))
    PBFS=(-f pbfs/vanHeukelum-cage15.bin -a p -c)
    CSCALE_FIB=$(($FIB-5))
    CSCALE_INTSUM=$(($INTSUM*10))
    CSCALE_FFT=(-n $FFTN -c)
    DEDUP_REDC=(-c -i $DEDUP_TEST/media.dat -o reducer.dat.ddp)
    DEDUP_SERC=(-c -i $DEDUP_TEST/media.dat -o serial.dat.ddp)
    DEDUP_REDU=(-u -i reducer.dat.ddp -o uncompressed.dat)
    
    COMM_HIST=(256 $(($INTSUM*2/5)))
    COMM_VECADD=(256 $(($INTSUM/20)))
}



#### Test inputs

#### Utility functions

over_worker_range()
{
    for ((NWORKERS=$STARTNWORKERS;NWORKERS<=$STOPNWORKERS;NWORKERS++))
    do
        $@
    done
    NWORKERS=$STARTNWORKERS
}

over_commutative()
{
    for ((COMMTEST=$CT_S;COMMTEST<=$CT_C;COMMTEST++))
    do
        $@
    done
    COMMTEST=$CT_S
}

over_bins()
{
    if [[ !($BIGSPA -eq 0) ]]
    then
        BINSTEP=128
        BINMAX=8192
    else
        BINSTEP=512
        BINMAX=4096
        if [[ $ISSPA -eq 1 ]]
        then
            BINSTEP=128
            BINMAX=1024
        fi
    fi


    BINS=$BINSTEP
    for ((BINS=$BINSTEP;BINS<$BINMAX;BINS+=$BINSTEP))
    do
        $@
    done
    BINS=$BINSTEP
}

print_to_perf()
{
    OFILE=$PERF

    printf "$1$SEP" >> $OFILE
    printf "$NWORKERS$SEP" >> $OFILE
    if [[ $WHICHTEST -eq $ISCOMMTEST ]]
    then
        printf "$COMMTEST$SEP" >> $OFILE
    fi
    if [[ $WHICHTEST -eq $ISSCALETEST ]]
    then
        printf "$BINS$SEP" >> $OFILE
        printf "$MERGES$SEP" >> $OFILE
        printf "$STEALS$SEP" >> $OFILE
    fi
    printf "$CONF$SEP" >> $OFILE
    printf "$RUNTIME$SEP" >> $OFILE
    printf "$AUXDATA$SEP" >> $OFILE
    printf "$RETCODE" >> $OFILE
    printf "%s\n" "$ENDROW" >> $OFILE
}

#### Running and profiling primitives

run_exe()
{
    # echo $@
    # echo CILK_NWORKERS=$NWORKERS ./$1_$CONFSUF ${@:2}
    if [[ $SPOOF -eq 0 ]]
    then
        CILK_NWORKERS=$NWORKERS ./$1_$CONFSUF ${@:2} 2> /dev/null
    else
        echo "# CILK_NWORKERS=$NWORKERS ./$1_$CONFSUF ${@:2} 2> /dev/null"
    fi
}

profile_exe()
{
    # echo $@
    if [ ! $PROFILEPARALLEL -eq 0 ]
    then
        PFILE=$1_$NWORKERS$CONFSUF.prof
        PLOG=$1_${NWORKERS}${CONFSUF}_profile.txt
    else
        PFILE=$1_$CONFSUF.prof
        PLOG=$1_${CONFSUF}_profile.txt
    fi
    if [[ ($SPOOF -eq 0) && (! $CANPROFILE -eq 0) ]]
    then
        LD_PRELOAD=$LIBPROF CPUPROFILE=$PFILE CILK_NWORKERS=$NWORKERS ./$1_$CONFSUF ${@:2} &> /dev/null
        google-pprof --text ./$1_$CONFSUF ./$PFILE &> logs/$PLOG
        rm -f $PFILE
    else
        echo "# LD_PRELOAD=$LIBPROF CPUPROFILE=$PFILE CILK_NWORKERS=$NWORKERS ./$1_$CONFSUF ${@:2} &> /dev/null"
        echo "# google-pprof --text ./$1_$CONFSUF ./$PFILE &> logs/$PLOG"
        echo "# rm -f $PFILE"
    fi
}

#### Parsing and testing output

default_parse()
{
    RAWOUTPUT="$(run_exe $@)"
    RETCODE=$?
    RUNTIME=$(echo "$RAWOUTPUT" | tail -1)
    AUXDATA=""
}

cilkscale_parse()
{
    RAWOUTPUT="$(run_exe $@)"
    RETCODE=$?
    RUNTIME=$(echo "$RAWOUTPUT" | tail -3 | head -1)
    WORK=$(echo "$RAWOUTPUT" | tail -1 | cut -d',' -f2)
    SPAN=$(echo "$RAWOUTPUT" | tail -1 | cut -d',' -f3)
    PARALLELISM=$(echo "$RAWOUTPUT" | tail -1 | cut -d',' -f4)
    AUXDATA="$WORK,$SPAN,$PARALLELISM"
}

scaling_parse()
{
    RAWOUTPUT="$(run_exe $@)"
    RETCODE=$?
    RUNTIME=$(echo "$RAWOUTPUT" | tail -3 | head -1)
    MERGES=$(echo "$RAWOUTPUT" | tail -2 | head -1)
    STEALS=$(echo "$RAWOUTPUT" | tail -1)
    if [[ !($STEALS -eq 0) ]]
    then
        AUXDATA="$(($MERGES/$STEALS))"
    else
        AUXDATA="0"
    fi
}

scaling_cilkscale_parse()
{
    RAWOUTPUT="$(run_exe $@)"
    RETCODE=$?
    RUNTIME=$(echo "$RAWOUTPUT" | tail -5 | head -1)
    MERGES=$(echo "$RAWOUTPUT" | tail -4 | head -1)
    STEALS=$(echo "$RAWOUTPUT" | tail -3 | head -1)
    PARALLELISM=$(echo "$RAWOUTPUT" | tail -1 | cut -d',' -f4)
    AUXDATA="$PARALLELISM"
}

default_check()
{
    RETCODE=$(($RETCODE))
}

dedup_check()
{
    # Prepare for spaghetti code
    ERR=0
    ./dedup-serial_$CONFSUF ${DEDUP_SERC[@]} &> /dev/null
    diff reducer.dat.ddp serial.dat.ddp &> /dev/null
    ERR=$(($ERR+$?))
    run_exe dedup-reducer ${DEDUP_REDU[@]} &> /dev/null
    diff uncompressed.dat $DEDUP_TEST/media.dat &> /dev/null
    ERR=$(($ERR+$?))
    if [[ ! $ERR -eq 0 ]]
    then
        ERR=1
    fi
    RETCODE=$(($RETCODE+$ERR))
}

# $1 = test name
# $2 = output parser
# $3 = output checker
# $4 = executable file
# $5... arguments
run_test()
{
    # echo $@
    DATE=$(date "+%T")
    if [ ! -e $4_$CONFSUF ]
    then
        echo "${COMMENT}(${DATE}) $1 compilation failed!"
        return 1
    else
        echo "${COMMENT}(${DATE}) Running test $1 on $NWORKERS worker(s)"
        echo "${COMMENT2}input: '${*:5}'"
    fi
    if [[ $SPOOF -eq 0 ]]
    then
        $2 $4 ${@:5}
        $3
    else
        run_exe $4 ${@:5}
        RETCODE=0
        RUNTIME=0.00
        AUXDATA=""
    fi
    echo "${COMMENT2}runtime: $RUNTIME"
    echo "${COMMENT2}return code: $RETCODE"
    print_to_perf "$1"
}

profile_test()
{
    # echo $@
    if [[ (! $PROFILE -eq 0) && ( (! $PROFILEPARALLEL -eq 0) || $NWORKERS -eq 1 ) ]]
    then
        DATE=$(date "+%T")
        if [ ! -e $2_$CONFSUF ]
        then
            echo "${COMMENT}(${DATE}) $1 compilation failed!"
            return 1
        else
            echo "${COMMENT}(${DATE}) Profiling test $1 on $NWORKERS worker(s)"
            echo "${COMMENT2}input: '${*:3}'"
        fi
        profile_exe $2 ${@:3}
    fi
}

#### Building the runtime

config()
{
    echo "$SECTION Running test | SPA: $1; Inline: $2; Peer: $3; Commutative: $4 $SECTION"

    # Setting hidden checks
    if [[ (! FASTHYPER -eq 0) && (! $3 -eq 0) ]]
    then
        HFLAG="-mllvm -fast-hyper-object"
    else
        HFLAG=""
    fi

    date
    CONF=$1"$SEP"$2"$SEP"$3
    CONFSUF=$1$2$3-$WHICHTEST
    make_sentinel $((1-$1)) $3 0 1 1 $2 $2 1 $4 $BIGSPA
}

make_sentinel()
{
    echo "Making sentinel \"$@\""
    echo "${COMMENT}Sentinel header at $SENT"
    printf "#ifndef _SENTINEL_TESTS_H\n" > ${SENT}
    printf "#define _SENTINEL_TESTS_H\n" >> ${SENT}
    printf "#define HASH_REDUCER %s\n" "$1" >> ${SENT}
    printf "#define PEER_PURE %s\n" "$2" >> ${SENT}
    printf "#define PRUNE_BRANCHES %s\n" "$3" >> ${SENT}
    printf "#define INLINE_TLS %s\n" "$4" >> ${SENT}
    printf "#define INLINE_MAP_LOOKUP %s\n" "$5" >> ${SENT}
    printf "#define SLOWPATH_LOOKUP %s\n" "$6" >> ${SENT}
    printf "#define INLINE_FULL_LOOKUP %s\n" "$7" >> ${SENT}
    printf "#define INLINE_ALL_TLS %s\n" "$8" >> ${SENT}
    printf "#define COMM_REDUCER %s\n" "$9" >> ${SENT}
    printf "#define BIG_SPA %s\n" "${10}" >> ${SENT}
    printf "#endif\n" >> ${SENT}
}

build()
{
    LOC="$(pwd)"
    echo "Building cheetah"
    echo "${COMMENT}cmake output in $LOC/logs/build_${CONFSUF}_log.txt"
    if [[ !($SPOOF -eq 2) ]]
    then
        cd ..
        rm ./build/lib/clang/10.0.1/lib/x86_64-unknown-linux-gnu/libopencilk-abi.bc
        sh opencilk.sh build &> $LOC/logs/build_${CONFSUF}_log.txt
        cd stress_test
    fi
}

#### Compilation

# $1 = file ; $2 = f flags ; $3 = .o files ; $4 = linked libraries
compile_c_test_internal()
{
    rm -rf $1_$CONFSUF
    echo "${COMMENT2}$CC $OPT $HFLAG -g -c -DTIMING_COUNT=$REPS $2 -o $1.o $1.c"
    $CC $OPT $HFLAG -g -c -DTIMING_COUNT=$REPS $2 -o $1.o $1.c
    $CC $OPT $HFLAG -S -emit-llvm -DTIMING_COUNT=$REPS -ftapir=none -o asm/$1_$CONFSUF.ll $1.c
    $CC $OPT $HFLAG -S -DTIMING_COUNT=$REPS $2 -o asm/$1_$CONFSUF.s $1.c
    echo "${COMMENT2}$CC $OPT $HFLAG $2 $3 $1.o $4 -o $1_$CONFSUF"
    $CC $OPT $HFLAG $2 $3 $1.o $4 -o $1_$CONFSUF
}

# $1 = file ; $2 = f flags ; $3 = .o files ; $4 = linked libraries
compile_cxx_test_internal()
{
    rm -rf $1_$CONFSUF
    echo "${COMMENT2}$CXX $OPT $HFLAG -g -c -DTIMING_COUNT=$REPS $2 -o $1.o $1.cpp"
    $CXX $OPT $HFLAG -g -c -DTIMING_COUNT=$REPS $2 -o $1.o $1.cpp
    $CXX $OPT $HFLAG -S -emit-llvm -DTIMING_COUNT=$REPS $2 -o asm/$1_$CONFSUF.ll $1.cpp
    $CXX $OPT $HFLAG -S -DTIMING_COUNT=$REPS $2 -o asm/$1_$CONFSUF.s $1.cpp
    echo "${COMMENT2}$CXX $OPT $HFLAG $2 $3 $1.o $4 -o $1_$CONFSUF"
    $CXX $OPT $HFLAG $2 $3 $1.o $4 -o $1_$CONFSUF
}

compile_test()
{
    echo "${COMMENT}Compiling $1"
    compile_c_test_internal $1 "-fopencilk" "ktiming.o"
}

compile_cilkscale_test()
{
    echo "${COMMENT}Compiling $1 with cilkscale"
    compile_c_test_internal $1 "-fopencilk -fcilktool=cilkscale" "ktiming.o"
}

compile_cilkscale_test_cxx()
{
    echo "${COMMENT}Compiling $1 with cilkscale"
    compile_cxx_test_internal $1 "-fopencilk -fcilktool=cilkscale" "ktiming.o" # "-lm"
}

compile_cilkscale_test_fft()
{
    echo "${COMMENT}Compiling $1 with cilkscale"
    compile_c_test_internal $1 "-fopencilk -fcilktool=cilkscale" "ktiming.o getoptions.o" "-lm"
}

compile_with_make()
{
    echo "${COMMENT}Compiling $1 with $2/Makefile"
    rm -rf $1_$CONFSUF
    cd $2
    make -s clean
    make HFLAG="$HFLAG" | sed "s/^/${COMMENT2}/"
    cd ..
    cp $2/$1 $1_$CONFSUF
}

compile_dedup()
{
    echo "${COMMENT}Compiling dedup with dedup/src/Makefile"
    rm -rf dedup-reducer_$CONFSUF dedup-serial_$CONFSUF
    cd dedup/src
    make -s clean
    make HFLAG="$HFLAG" | sed "s/^/${COMMENT2}/"
    cd ../..
    cp dedup/src/dedup-reducer dedup-reducer_$CONFSUF
    cp dedup/src/dedup-serial dedup-serial_$CONFSUF
}

compile_opt_tests()
{
    if [[ !($SPOOF -eq 2) ]]
    then
        compile_test intsum_check
        compile_test fib
        compile_test histogram
        compile_test intlist

        compile_cilkscale_test cilkscale_fib
        compile_cilkscale_test cilkscale_intsum
        compile_cilkscale_test_fft fft
        compile_cilkscale_test_cxx apsp-matteo

        compile_with_make bfs pbfs
        compile_with_make BlackScholes BlackScholes
        compile_with_make Mandelbrot Mandelbrot
        compile_dedup

        rm -rf peer_set_pure_test_$CONFSUF
        $CC $OPT -g -c -DTIMING_COUNT=$REPS -fopencilk -fno-vectorize -o peer_set_pure_test.o peer_set_pure_test.c
        $CC $OPT -S -emit-llvm -DTIMING_COUNT=$REPS -fopencilk -o peer_set_pure_test.ll peer_set_pure_test.c
        $CC $OPT -fopencilk ktiming.o peer_set_pure_test.o -o peer_set_pure_test_$CONFSUF
    fi
}

compile_comm_tests()
{
    if [[ !($SPOOF -eq 2) ]]
    then
        compile_test comm_histogram
        compile_test comm_vecadd
    fi
}

compile_scale_tests()
{
    if [[ !($SPOOF -eq 2) ]]
    then
        compile_test scale_histogram_arr_red
        compile_test scale_histogram_red_arr
        compile_cilkscale_test scale_cilkscale_histogram_arr_red
    fi
}

compile()
{
    echo "Compiling tests"

    $CC $OPT -g -c -DTIMING_COUNT=$1 -o ktiming.o ktiming.c
    $CC $OPT -g -c -o getoptions.o getoptions.c
    
    
    if [[ $WHICHTEST -eq $ISOPTTEST ]]
    then
        compile_opt_tests
    fi

    if [[ $WHICHTEST -eq $ISCOMMTEST ]]
    then
        compile_comm_tests
    fi

    if [[ $WHICHTEST -eq $ISSCALETEST ]]
    then
        compile_scale_tests
    fi
}

#### Cleanup

clean_make()
{
    cd $1; make -s clean; cd ..
}

clean_exe()
{
    # nm ./fib_*
    rm -rf intsum_check_*
    rm -rf fib_*
    rm -rf histogram_*
    rm -rf intlist_*

    rm -rf cilkscale_fib_*
    rm -rf cilkscale_intsum_*
    rm -rf fft_*
    rm -rf apsp-matteo_*
    
    rm -rf bfs_*
    clean_make pbfs
    rm -rf BlackScholes_*
    clean_make BlackScholes
    rm -rf Mandelbrot_*
    clean_make Mandelbrot
    rm -rf dedup-reducer_*
    rm -rf dedup-serial_*
    cd dedup/src; make -s clean; cd ../..

    rm -rf comm_histogram_*
    rm -rf comm_vecadd_*

    rm -rf scale_histogram_arr_red_*
    rm -rf scale_cilkscale_histogram_arr_red_*
    rm -rf scale_histogram_red_arr_*

    rm -rf peer_set_pure_test_*
    rm -rf *.o *.s *.ll
}

clean_all()
{
    rm -rf build_*.txt perf_*.csv
    rm -rf  *.bmp *.valsig *.prof *.dat *.ddp logs/ asm/
    clean_exe
}

#### Specific tests

test_intsum()
{
    run_test "intsum" default_parse default_check intsum_check $INTSUM $CR
    profile_test "intsum" intsum_check $INTSUM $CR
}

test_fib()
{
    run_test "fib" default_parse default_check fib $FIB $CR
    profile_test "fib" fib $FIB $CR
}

test_histogram()
{
    run_test "histogram" default_parse default_check histogram ${HIST[@]} $CR
    profile_test "histogram" histogram ${HIST[@]} $CR
}

test_intlist()
{
    run_test "intlist" default_parse default_check intlist $INTLIST $CR
    profile_test "intlist" intlist $INTLIST $CR
}

test_bfs()
{
    run_test "PBFS" default_parse default_check bfs ${PBFS[@]}
    profile_test "PBFS" bfs ${PBFS[@]}
}

test_cilkscale_fib()
{
    run_test "cilkscale (fib)" cilkscale_parse default_check cilkscale_fib $CSCALE_FIB
    profile_test "cilkscale (fib)" cilkscale_fib $CSCALE_FIB
}

test_cilkscale_intsum()
{
    run_test "cilkscale (intsum)" cilkscale_parse default_check cilkscale_intsum $CSCALE_INTSUM
    profile_test "cilkscale (intsum)" cilkscale_intsum $CSCALE_INTSUM
}

test_fft()
{
    run_test "fft" cilkscale_parse default_check fft ${CSCALE_FFT[@]}
    profile_test "fft" fft ${CSCALE_FFT[@]}
}

test_apsp()
{
    run_test "apsp" cilkscale_parse default_check apsp-matteo
    profile_test "apsp" apsp-matteo
}

test_BlackScholes()
{
    run_test "BlackScholes" cilkscale_parse default_check BlackScholes # No arguments
    profile_test "BlackScholes" BlackScholes # No arguments
}

test_Mandelbrot()
{
    run_test "Mandelbrot" cilkscale_parse default_check Mandelbrot # No arguments
    profile_test "Mandelbrot" Mandelbrot # No arguments
}

test_dedup()
{
    run_test "dedup" default_parse dedup_check dedup-reducer ${DEDUP_REDC[@]}
    profile_test "dedup" dedup-reducer ${DEDUP_REDC[@]}
}

test_commutative_histogram()
{
    run_test "commutative histogram" default_parse default_check comm_histogram ${COMM_HIST[@]} $COMMTEST
    profile_test "commutative histogram" comm_histogram ${COMM_HIST[@]} $COMMTEST
}

test_commutative_vector_add()
{
    run_test "commutative vector addition" default_parse default_check comm_vecadd ${COMM_VECADD[@]} $COMMTEST
    profile_test "commutative vector addition" comm_vecadd ${COMM_VECADD[@]} $COMMTEST
}

test_scale_histogram_arr_red()
{
    S_HIST_AR=($BINS $(($INTSUM/5)))
    run_test "scaling histogram (array of reducers)" scaling_parse default_check scale_histogram_arr_red ${S_HIST_AR[@]}
    profile_test "scaling histogram (array of reducers)" scale_histogram_arr_red ${S_HIST_AR[@]}
}

test_scale_histogram_red_arr()
{
    S_HIST_AR=($BINS $(($INTSUM/5)))
    run_test "scaling histogram (reducer over arrays)" scaling_parse default_check scale_histogram_red_arr ${S_HIST_AR[@]}
    profile_test "scaling histogram (reducer over arrays)" scale_histogram_red_arr ${S_HIST_AR[@]}
}

test_cilkscale_scale_histogram_arr_red()
{
    S_HIST_AR=($BINS $(($INTSUM/5)))
    run_test "cilkscale histogram (array of reducers)" scaling_cilkscale_parse default_check scale_cilkscale_histogram_arr_red ${S_HIST_AR[@]}
    profile_test "cilkscale histogram (array of reducers)" scale_cilkscale_histogram_arr_red ${S_HIST_AR[@]}
}

#### Running everything

make_runtime()
{
    build # &> /dev/null
    compile
}

run_opt_tests()
{
    echo "Running optimization tests"
    PERF=$PERFOPT
    touch $PERF
    echo "${COMMENT}Performance data in $PERF"

    if [[ !($SPOOF -eq 2) ]]
    then
        over_worker_range test_intsum
        over_worker_range test_fib
        over_worker_range test_histogram
        over_worker_range test_intlist

        over_worker_range test_cilkscale_fib
        # Uses an atomic
        # over_worker_range test_cilkscale_intsum
        over_worker_range test_fft
        over_worker_range test_apsp
        
        over_worker_range test_bfs
        over_worker_range test_BlackScholes
        over_worker_range test_Mandelbrot

        over_worker_range test_dedup
    fi

    echo "Finished optimization tests"
}

run_comm_tests()
{
    echo "Running commutative tests"
    PERF=$PERFCOMM
    touch $PERF
    echo "${COMMENT}Performance data in $PERF"
    if [[ !($SPOOF -eq 2) ]]
    then
        over_worker_range over_commutative test_commutative_histogram
        over_worker_range over_commutative test_commutative_vector_add
    fi

    echo "Finished commutative tests"
}

run_scale_tests()
{
    echo "Running scale tests"
    PERF=$PERFSCALE
    touch $PERF
    echo "${COMMENT}Performance data in $PERF"
    if [[ !($SPOOF -eq 2) ]]
    then
        over_worker_range over_bins test_scale_histogram_arr_red
        over_worker_range over_bins test_scale_histogram_red_arr
        over_worker_range over_bins test_cilkscale_scale_histogram_arr_red
    fi

    echo "Finished scale tests"
}

run_tests()
{
    if [[ $WHICHTEST -eq $ISOPTTEST ]]
    then
        run_opt_tests
    fi

    if [[ $WHICHTEST -eq $ISCOMMTEST ]]
    then
        run_comm_tests
    fi

    if [[ $WHICHTEST -eq $ISSCALETEST ]]
    then
        run_scale_tests
    fi
}

build_and_test()
{
    if [[ $1$2$3 =~ $CONFEXP ]]
    then
        # Opt tests
        if [[ $TESTEXP =~ 1.. ]]
        then
            WHICHTEST=$ISOPTTEST
            config $1 $2 $3 0
            make_runtime
            run_tests
        fi
        # Commutative tests
        if [[ $TESTEXP =~ .1. ]]
        then
            # Only support comutative + SPA
            if [[ $1 -eq 1 ]]
            then
                WHICHTEST=$ISCOMMTEST
                config $1 $2 $3 1
                make_runtime
                run_tests
            fi
        fi
        # Scale tests
        if [[ $TESTEXP =~ ..1 ]]
        then
            WHICHTEST=$ISSCALETEST
            ISSPA=$1
            config $1 $2 $3 0
            make_runtime
            run_tests
        fi
    fi
}

full_stress_test()
{
    clean_all
    mkdir -p logs
    mkdir -p asm
    build_and_test 0 0 0
    build_and_test 0 1 0
    build_and_test 0 0 1
    build_and_test 0 1 1
    build_and_test 1 0 0
    build_and_test 1 1 0
    build_and_test 1 0 1
    build_and_test 1 1 1
    clean_exe
    echo "$SECTION Testing complete! $SECTION"
    date
}

#### Actually run the tests

set_spoof 0
set_reps 10
set_testexp 111
set_confexp 110

set_fast_hyperobjects 0
set_bigspa 1

parse_range full
set_profiling 0

if [[ $# -eq 0 ]]
then
    parse_testexp full
    parse_confexp 110
    parse_inputs fast
elif [[ $# -eq 1 ]]
then
    parse_testexp "$1"
    parse_confexp 110
    parse_inputs fast
elif [[ $# -eq 2 ]]
then
    parse_testexp "$1"
    parse_confexp "$2"
    parse_inputs fast
elif [[ $# -eq 3 ]]
then
    parse_testexp "$1"
    parse_confexp "$2"
    parse_inputs "$3"
else
    echo "Too many arguments!"
    return 1
fi


full_stress_test
