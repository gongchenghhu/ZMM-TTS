@echo off

echo Encoding Vector 5
g728 enc ..\testvector\IN5.BIN bits.out
echo Decoding with packet loss concealment
g728 -stats -plcsize 10 plc bits.out mask1 speech.out
