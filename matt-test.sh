CC=$HOME/opencilk/build/bin/clang
CPP=$HOME/opencilk/build/bin/clang++
SENT=$HOME/opencilk/opencilk-project/cheetah/include/cilk/sentinel.h
PERF=perf.csv
OPT=-O3

# Default: 5
REPS=5
# Default: find /usr -name "libprofiler.*"
LIBPROF=/usr/lib/x86_64-linux-gnu/libprofiler.so.0
# Default: 1
PROFILE=0

# Default: *100
INTSUM=$((25000000*1))
# Default: +3
FIB=$((40-1))
# Default: 256, *2/5
HIST=(256 $(($INTSUM*2/5)))
# Default: *100
INTLIST=$((2000000*1))
# Default: -f vanHeukelum-cage15.bin -a p -c
PBFS=(-f pbfs/vanHeukelum-cage15.bin -a p -c)
# Default: -5
CSCALE_FIB=$(($FIB-5))
# Default: *10
CSCALE_INTSUM=$(($INTSUM*10))
# Default: -n 20000000
CSCALE_FFT=(-n 20000000)
# Default: 1
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

# Formatting
HEADSEP=';'
SEP=';'
ENDROW=''
SECTION='####'
COMMENT=' * '
COMMENT2=' ** '

run_exe()
{
    # echo $@
    # echo CILK_NWORKERS=$NWORKERS ./$1_$CONFSUF ${@:2}
    CILK_NWORKERS=$NWORKERS ./$1_$CONFSUF ${@:2} 2> /dev/null
}

profile_exe()
{
    # echo $@
    LD_PRELOAD=$LIBPROF CPUPROFILE=$1_$CONFSUF.prof CILK_NWORKERS=$NWORKERS ./$1_$CONFSUF ${@:2} 2> /dev/null
    google-pprof --text ./$1_$CONFSUF ./$1_$CONFSUF.prof > logs/$1_${CONFSUF}_profile.txt
    rm -f $1_$CONFSUF.prof
}

default_parse()
{
    RES="$(run_exe $@)"
    CODE=$?
    printf "$RES"
    return $CODE
}

cilkscale_parse()
{
    RES="$(run_exe $@)"
    CODE=$?
    ELTS=$(echo $RES | cut -d',' -f1)
    printf "$ELTS"
    return $CODE
}

run_test()
{
    # echo $@
    if [ ! -e $3_$CONFSUF ]
    then
        printf "$1 compilation failed!\n"
        return 1
    else
        printf "${COMMENT}Running test $1 with inputs '%s'... " "${*:4}"
    fi
    ERR=0
    printf "$1$HEADSEP$CONF$SEP" >> $PERF
    PARSED=$($2 $3 ${@:4})
    ERR=$(($ERR+$?))
    printf "$PARSED$SEP$ERR" >> $PERF
    printf "runtime: $PARSED; return code: $ERR\n"
    printf "%s\n" "$ENDROW" >> $PERF
}

profile_test()
{
    # echo $@
    if [ ! -e $2_$CONFSUF ]
    then
        printf "$1 compilation failed!\n"
        return 1
    else
        printf "${COMMENT}Profiling test $1 with inputs '%s'...\n" "${*:4}"
    fi
    profile_exe $2 ${@:3}
}

config()
{
    echo "$SECTION Runing test | SPA: $1; Inline: $2; Peer: $3 $SECTION"
    CONF=$1"$SEP"$2"$SEP"$3
    CONFSUF=$1$2$3
    make_sentinel $((1-$1)) $3 0 1 1 $2 $2 1
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
    printf "#endif\n" >> ${SENT}
}

build()
{
    LOC="$(pwd)"
    echo "Building cheetah"
    echo "${COMMENT}cmake output in $LOC/logs/build_${CONFSUF}_log.txt"
    cd ..
    rm ./build/lib/clang/10.0.1/lib/x86_64-unknown-linux-gnu/libopencilk-abi.bc
    sh opencilk.sh build &> $LOC/logs/build_${CONFSUF}_log.txt
    cd stress_test
}

# $1 = file ; $2 = f flags ; $3 = .o files ; $4 = linked libraries
compile_test_internal()
{
    rm -rf $1_$CONFSUF
    $CC $OPT -g -c -DTIMING_COUNT=$REPS $2 -o $1.o $1.c
    $CC $OPT -S -DTIMING_COUNT=$REPS $2 -o $1.s $1.c
    $CC $OPT $2 $3 $1.o $4 -o $1_$CONFSUF
}

compile_test()
{
    echo "${COMMENT}Compiling $1"
    compile_test_internal $1 "-fopencilk" "ktiming.o"
}

compile_cilkscale_test()
{
    echo "${COMMENT}Compiling $1 with cilkscale"
    compile_test_internal $1 "-fopencilk -fcilktool=cilkscale" "ktiming.o"
}

compile_cilkscale_test_fft()
{
    echo "${COMMENT}Compiling $1 with cilkscale"
    compile_test_internal $1 "-fopencilk -fcilktool=cilkscale" "ktiming.o getoptions.o" "-lm"
}

compile_with_make()
{
    echo "${COMMENT}Compiling $1 with $2/Makefile"
    rm -rf $1_$CONFSUF
    cd $2; make -s clean; make -s; cd ..
    cp $2/$1 $1_$CONFSUF
}

compile()
{
    echo "Compiling tests"

    $CC $OPT -g -c -DTIMING_COUNT=$1 -o ktiming.o ktiming.c
    $CC $OPT -g -c -o getoptions.o getoptions.c
    
    compile_test intsum_check
    compile_test fib
    compile_test histogram
    compile_test intlist
    
    compile_cilkscale_test cilkscale_fib
    compile_cilkscale_test cilkscale_intsum
    compile_cilkscale_test_fft fft
    
    compile_with_make bfs pbfs
    compile_with_make BlackScholes BlackScholes
    compile_with_make Mandelbrot Mandelbrot
    
    rm -rf peer_set_pure_test_$CONFSUF
    $CC $OPT -g -c -DTIMING_COUNT=$REPS -fopencilk -fno-vectorize -o peer_set_pure_test.o peer_set_pure_test.c
    $CC $OPT -S -emit-llvm -DTIMING_COUNT=$REPS -fopencilk -o peer_set_pure_test.ll peer_set_pure_test.c
    $CC $OPT -fopencilk ktiming.o peer_set_pure_test.o -o peer_set_pure_test_$CONFSUF
}

clean_all()
{
    rm -rf build_*.txt perf.csv
    rm -rf  *.bmp *.valsig *.prof logs/
    clean_exe
}

clean_exe()
{
    rm -rf intsum_check_*
    rm -rf fib_*
    rm -rf histogram_*
    rm -rf intlist_*

    rm -rf cilkscale_fib_*
    rm -rf cilkscale_intsum_*
    rm -rf fft_*
    
    rm -rf bfs_*
    rm -rf BlackScholes_*
    rm -rf Mandelbrot_*

    rm -rf peer_set_pure_test_*
    rm -rf *.o *.s *.ll
}

test_intsum()
{
    run_test "intsum" default_parse intsum_check $INTSUM $CR
    if [ ! $PROFILE -eq 0 ]
    then
        profile_test "intsum" intsum_check $INTSUM $CR
    fi
}

test_fib()
{
    run_test "fib" default_parse fib $FIB $CR
    if [ ! $PROFILE -eq 0 ]
    then
        profile_test "fib" fib $FIB $CR
        fi
}

test_histogram()
{
    run_test "histogram" default_parse histogram ${HIST[@]} $CR
    if [ ! $PROFILE -eq 0 ]
    then
        profile_test "histogram" histogram ${HIST[@]} $CR
    fi
}

test_intlist()
{
    run_test "intlist" default_parse intlist $INTLIST $CR
    if [ ! $PROFILE -eq 0 ]
    then
        profile_test "intlist" intlist $INTLIST $CR
    fi
}

test_bfs()
{
    run_test "PBFS" default_parse bfs ${PBFS[@]}
    if [ ! $PROFILE -eq 0 ]
    then
        profile_test "PBFS" bfs ${PBFS[@]}
    fi
}

test_cilkscale_fib()
{
    run_test "cilkscale (fib)" cilkscale_parse cilkscale_fib $CSCALE_FIB
    if [ ! $PROFILE -eq 0 ]
    then
        profile_test "cilkscale (fib)" cilkscale_fib $CSCALE_FIB
    fi
}

test_cilkscale_intsum()
{
    run_test "cilkscale (intsum)" cilkscale_parse cilkscale_intsum $CSCALE_INTSUM
    if [ ! $PROFILE -eq 0 ]
    then
        profile_test "cilkscale (intsum)" cilkscale_intsum $CSCALE_INTSUM
    fi
}

test_fft()
{
    run_test "fft" cilkscale_parse fft ${CSCALE_FFT[@]}
    if [ ! $PROFILE -eq 0 ]
    then
        profile_test "fft" fft ${CSCALE_FFT[@]}
    fi
}

test_BlackScholes()
{
    run_test "BlackScholes" cilkscale_parse BlackScholes # No arguments
    if [ ! $PROFILE -eq 0 ]
    then
        profile_test "BlackScholes" BlackScholes # No arguments
    fi
}

test_Mandelbrot()
{
    run_test "Mandelbrot" cilkscale_parse Mandelbrot # No arguments
    if [ ! $PROFILE -eq 0 ]
    then
        profile_test "Mandelbrot" Mandelbrot # No arguments
    fi
}

make_runtime()
{
    build # &> /dev/null
    compile
}

run_tests()
{
    echo "Running tests"
    touch $PERF
    echo "${COMMENT}Performance data in $PERF"

    test_intsum
    test_fib
    test_histogram
    test_intlist

    test_cilkscale_fib
    test_cilkscale_intsum
    test_fft
    
    test_bfs
    test_BlackScholes
    test_Mandelbrot
}

build_and_test()
{
    config $1 $2 $3
    make_runtime
    run_tests
}

clean_all
mkdir -p logs
build_and_test 0 0 0
#build_and_test 0 1 0
#build_and_test 0 0 1
#build_and_test 0 1 1
#build_and_test 1 0 0
#build_and_test 1 1 0
#build_and_test 1 0 1
#build_and_test 1 1 1
clean_exe

#Old funcs

test_header()
{
    printf "TEST$HEADSEP"
    if [ $1 -eq 1 ]
    then
        
        printf "SG$SEP"
        printf "SR$SEP"
        printf "CG$SEP"
    fi
    printf "CR%s\n" "$ENDROW"
}

# $1 = test name ; $2 = exe name ; $3 = input
# $1 = test name ; $2 = parser ; $3 = exe name ; $4 = input
run_overhead_test()
{
    # echo $@
    if [ ! -e $3_$CONFSUF ]
    then
        printf "$1 compilation failed!\n"
        return 1
    fi
    ERR=0
    printf "$1$HEADSEP$CONF$SEP"
    $2 $3 ${@:4} 0
    ERR=$(($ERR+$?))
    printf "$SEP"
    $2 $3 ${@:4} 1
    ERR=$(($ERR+$?))
    printf "$SEP"
    $2 $3 ${@:4} 3
    ERR=$(($ERR+$?))
    printf "$SEP"
    $2 $3 ${@:4} 2
    ERR=$(($ERR+$?))
    printf "$SEP$ERR"
    printf "%s\n" "$ENDROW"
}

test_intsum_old()
{
    if [ ! -e intsum_check_$CONFSUF ]
    then
        printf "intsum compilation failed!\n"
        return 1
    fi
    ERR=0
    printf "intsum$HEADSEP"
    if [ $1 -eq 1 ]
    then
        run_exe intsum_check $INTSUM 0
        ERR=$(($ERR+$?))
        printf "$SEP"
        run_exe intsum_check $INTSUM 1
        ERR=$(($ERR+$?))
        printf "$SEP"
        run_exe intsum_check $INTSUM 3
        ERR=$(($ERR+$?))
        printf "$SEP"
    fi
    run_exe intsum_check $INTSUM 2
    ERR=$(($ERR+$?))
    printf "$SEP"
    if [ $ERR -eq 0 ]
    then
        printf 'No errors!'
    else
        printf '$ERR errors!'
    fi
    printf "%s\n" "$ENDROW"
}

test_fib_old()
{
    if [ ! -e fib_$CONFSUF ]
    then
        printf "fib compilation failed!\n"
        return 1
    fi
    ERR=0
    printf "fib$HEADSEP"
    if [ $1 -eq 1 ]
    then
        run_exe fib $FIB 0
        ERR=$(($ERR+$?))
        printf "$SEP"
        run_exe fib $FIB 1
        ERR=$(($ERR+$?))
        printf "$SEP"
        run_exe fib $FIB 3
        ERR=$(($ERR+$?))
        printf "$SEP"
    fi
    run_exe fib $FIB 2
    ERR=$(($ERR+$?))
    printf "$SEP"
    if [ $ERR -eq 0 ]
    then
        printf 'No errors!'
    else
        printf '$ERR errors!'
    fi
    printf "%s\n" "$ENDROW"
}

test_peer()
{
    if [ ! -e peer_set_pure_test_$CONFSUF ]
    then
        printf "vecsum compilation failed!\n"
        return 1
    fi
    printf "vecsum$HEADSEP"
    CILK_NWORKERS=1 ./peer_set_pure_test_$CONFSUF $INTSUM 0
    printf "$SEP"
    CILK_NWORKERS=1 ./peer_set_pure_test_$CONFSUF $INTSUM 1
    printf "$SEP"
    CILK_NWORKERS=1 ./peer_set_pure_test_$CONFSUF $INTSUM 2
    printf "$SEP"
    CILK_NWORKERS=1 ./peer_set_pure_test_$CONFSUF $INTSUM 3
    printf "%s\n" "$ENDROW"
}
