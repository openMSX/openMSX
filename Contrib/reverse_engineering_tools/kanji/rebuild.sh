#!/bin/bash --
ls -d ktst30.asc ktst30a.asc ktst30b.asc ktst30c.asc &&
{
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
    LC_ALL=C printf '%s%s%-6s\xfe\0\0\0\0\0\0\0%s' "$hdr" "$e" "KTST30" "$hdr"
    _cat ktst30.asc
    LC_ALL=C printf '%s%s%-6s\xfe\0\0\0\0\0\0\0%s' "$hdr" "$e" "KT30A" "$hdr"
    _cat ktst30a.asc
    LC_ALL=C printf '%s%s%-6s\xfe\0\0\0\0\0\0\0%s' "$hdr" "$e" "KT30B" "$hdr"
    _cat ktst30b.asc
    LC_ALL=C printf '%s%s%-6s\xfe\0\0\0\0\0\0\0%s' "$hdr" "$e" "KT30C" "$hdr"
    _cat ktst30c.asc
) | LC_ALL=C sed 's,KTST30[.]ASC,CAS:KTST30,g;s,\(KT\)ST\(30[A-Z]\)[.]ASC,CAS:\1\2  ,g' > ktst30.cas
}
