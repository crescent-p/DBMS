make
./nitcbase << EOF
SELECT * FROM RELATIONCAT INTO null WHERE Name = Students;
exit
EOF
