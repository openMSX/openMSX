as80 -z -x3 msxb_sub
as80 -z -x3 msxb_sh
as80 -lmsx.syn -z -x3 msxb
copy /b msxb.bin+msxb_sh.bin msx.rom
copy msxb_sub.bin msxb_sub.rom
