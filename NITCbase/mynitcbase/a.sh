make
./nitcbase << EOF
SELECT * FROM RELATIONCAT INTO null WHERE #Records>10;
exit
EOF
