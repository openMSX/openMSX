as80 -z -x3 cb_sub
as80 -z -x3 cb_sh
as80 -lcb.syn -z -x3 cb_main
copy /b cb_main.bin+cb_sh.bin cbios.rom
copy cb_sub.bin cbios_sub.rom
