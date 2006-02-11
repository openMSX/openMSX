#
# This is an example of a UMR callback function. You can set the "umr_callback"
# setting in openMSX to "umrcallback" and this function will be called whenever
# openMSX detects a read from uninitialized memory. It might be a good idea to
# go into 'debug break' mode in this callback, so that you can easily examine
# what is going on which has caused the UMR.
# 
#  @param addr The absolute address in the RAM device.
#  @param name The name of the RAM device.
#
proc umrcallback { addr name } { 
	puts stderr [format "UMR detected in RAM device \"$name\", offset: 0x%04X, PC: 0x%04X" $addr [reg PC]]
}
