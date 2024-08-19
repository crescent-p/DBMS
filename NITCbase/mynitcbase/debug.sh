make mode=debug
gdb nitcbase-debug << EOF
break Algebra::select
break linearSearch
break getAttrCatEntry 
run
SELECT * FROM Students INTO null WHERE Batch=J;
EOF
