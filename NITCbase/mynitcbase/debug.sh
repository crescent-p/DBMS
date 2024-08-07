make mode=debug
gdb nitcbase-debug << EOF
break $1
run
c
EOF
