#!/bin/bash --
cd "$( dirname -- "$0" || echo . )" &&
printf 'working in directory %q\n' "$( pwd )" &&
echo checking for source files &&
    ls -d BACONLDR.BIN autoexec.bas cktst31.bcl cktst31.asc ktst31{,a,b,c}.asc ktst31.md rebuild.sh cas2wav.py &&
    echo checking for needed tools &&
    ls -d "$(
        type -p msx_bacon ||
            echo ' ... no msx_bacon in $PATH '
    )" "$(
        type -p zma ||
            echo ' ... no zma in $PATH '
    )" "$(
        type -p openmsx ||
            echo ' ... no openmsx in $PATH '
    )" "$(
        type -p sox ||
            echo ' ... no sox in $PATH '
    )" "$(
        type -p zip ||
            echo ' ... no zip in $PATH '
    )" "$(
        type -p perl ||
            echo ' ... no perl in $PATH '
    )" "$(
        type -p python3 ||
            echo ' ... no python3 in $PATH '
    )" "$(
        type -p sed ||
            echo ' ... no sed in $PATH '
    )" "$(
        type -p dd ||
            echo ' ... no dd in $PATH '
    )" "$(
        type -p wc ||
            echo ' ... no wc in $PATH '
    )" &&
    rm -vf zma.log zma.sym cktst31.asm cktst31.bin "ktst31 [RUN'CAS-'].cas" "ktst31 [RUN'CAS-'].wav" "ktst31 [RUN'CAS-'] [2400bps].wav" ktst31.dsk ktst31.zip &&
    {
        openmsx --version &&
        msx_bacon -original -O3 cktst31.asc cktst31.asm &&
            zma cktst31.asm cktst31.bin &&
            rm -vf zma.log zma.sym cktst31.asm &&
            perl -pi -e 's/SZ=[0-9][0-9]*/SZ='$(( $( LC_ALL=C wc -c < cktst31.bin ) ))'/i' ktst31.asc &&
            mkdir -pv /tmp/ktst31.$$ &&
            cp -v BACONLDR.BIN autoexec.bas cktst31.bcl cktst31.asc cktst31.bin ktst31{,a,b,c}.asc /tmp/ktst31.$$ &&
            dd bs=1024 count=720 if=/dev/zero of=ktst31.dsk &&
            echo starting headless openmsx... &&
            openmsx -machine Sanyo_PHC-70FD2 -diska ktst31.dsk -diskb /tmp/ktst31.$$ \
                    -command 'set fastforward on' \
                    -command 'set renderer none' \
                    -command 'set old_sound_driver "$sound_driver"' \
                    -command 'set sound_driver null' \
                    -command 'after time 15 { type -release -freq 10 "_format:copy\"b:????????.???\"to\"a:\":def usr=&h65:?usr(0)\ra2 " }' \
                    -command 'after time 30 { set sound_driver "$old_sound_driver"; }' \
                    -command 'debug set_bp 0x65 "" { set sound_driver "$old_sound_driver"; exit; }' &&
            echo openmsx exited successfully &&
            python3 -c 'if True:
                import sys

                _, dsk = sys.argv
                buf = open(dsk, "rb").read()
                buf = bytes(
                    [
                            # zero out all timestamps in the first sector of the root directory
                            0 if i // 512 == 7 and i % 32 in range(13, 26) else buf[i]
                                    for i in range(len(buf))
                    ]
                )
                open(dsk, "wb").write(buf)
            ' ktst31.dsk &&
            rm -rvf /tmp/ktst31.$$ &&
            (
                hdr="$(LC_ALL=C printf '\x1f\xa6\xde\xba\xcc\x13\x7d\x74')"
                _cat() {
                    local f="$@"
                    local sz=$(( $( LC_ALL=C wc -c < "$f" | awk '{print $1}' ) ))
                    local i=0
                    while [[ $i -lt $sz ]]
                    do
                        echo "_cat $f $i/$sz" >&2
                        dd bs=1 skip=$i count=256 < <(
                            LC_ALL=C cat "$f"
                            dd bs=1 count=256 < /dev/zero 2>/dev/null |
                                LC_ALL=C tr '\0' $'\x1A'
                        ) 2>/dev/null
                        i=$((i+256))
                        if [[ $i -lt $sz ]]
                        then
                            LC_ALL=C printf %s "$hdr"
                        fi
                    done
                    LC_ALL=C printf '\xfe\0\0\0\0\0\0\0'
                }
                e="$(
                    dd bs=1 count=10 if=/dev/zero 2>/dev/null |
                        LC_ALL=C tr '\0' $'\xea'
                )"
                fmt='%s%s%-6s\xfe\0\0\0\0\0\0\0%s'
                LC_ALL=C printf "$fmt" "$hdr" "$e" "KTST31" "$hdr"
                _cat ktst31.asc
                LC_ALL=C printf "$fmt" "$hdr" "$e" "KT31A" "$hdr"
                _cat ktst31a.asc
                LC_ALL=C printf "$fmt" "$hdr" "$e" "KT31B" "$hdr"
                _cat ktst31b.asc
                LC_ALL=C printf "$fmt" "$hdr" "$e" "KT31C" "$hdr"
                _cat ktst31c.asc
            ) | LC_ALL=C sed 's,BLOAD,REM *,gi;s,KTST31[.]ASC,CAS:KTST31,gi;s,\(KT\)ST\(31[A-Z]\)[.]ASC",CAS:\1\2"  ,gi' > "ktst31 [RUN'CAS-'].cas" &&
            python3 cas2wav.py "ktst31 [RUN'CAS-'].cas" "ktst31 [RUN'CAS-'].wav" &&
            sox -r 44100 "ktst31 [RUN'CAS-'].wav" "ktst31 [RUN'CAS-'] [2400bps].wav" &&
            zip -9v ktst31.zip \
                BACONLDR.BIN \
                autoexec.bas \
                cas2wav.py \
                cktst31.asc \
                cktst31.bcl \
                cktst31.bin \
                "ktst31 [RUN'CAS-'] [2400bps].wav" \
                "ktst31 [RUN'CAS-'].cas" \
                "ktst31 [RUN'CAS-'].wav" \
                ktst31.asc \
                ktst31.dsk \
                ktst31.md \
                ktst31a.asc \
                ktst31b.asc \
                ktst31c.asc \
                rebuild.sh &&
            echo "Success, all rebuild steps completed"
    }
