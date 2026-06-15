#!/bin/bash
echo "cleaning for report gen package.."
make clean
echo "start make source for codechecker..."
echo generating code checker report!
~/.local/bin/CodeChecker log --build "make util mtx" --output reports/compile_commands.json
~/.local/bin/CodeChecker analyze reports/compile_commands.json --enable sensitive --output reports/reports
~/.local/bin/CodeChecker parse reports/reports > codechecker.lst
rm -rf reports/*
set -e
echo "start checksec on binaries..."
./RunChecksec.sh > Checksecresult.txt
echo "genreating package.."
echo "scan using ClamAV before packing"
echo "--------------Start pack scan------------" > clamscanreport.txt
clamscan -r ../. >> clamscanreport.txt
echo "--------------Finished-------------------" >> clamscanreport.txt
make pack
echo "scan final output using ClamAV"
echo "--------------Final Binary Scan----------" >> clamscanreport.txt
clamscan *.bin >> clamscanreport.txt
echo "-----------------------------------------" >> clamscanreport.txt
clamscan *.hash >> clamscanreport.txt
echo "--------------Finished-------------------" >> clamscanreport.txt
echo "finished genreating package.."

