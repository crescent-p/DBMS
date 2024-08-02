#! /bin/bash

DIR="./NITCbase/"

if [ ! -d "$DIR" ]; then
	mkdir $DIR
fi

if [ -z "$(ls -A $DIR)" ]; then
	cd $DIR
	echo Downloading the NITCbase packages...
	wget https://github.com/Nitcbase/nitcbase/archive/master.tar.gz -O nitcbase.tar.gz
	wget https://github.com/Nitcbase/xfs-interface/archive/master.tar.gz -O xfs-interface.tar.gz
	echo Extracting the package...
	tar -xvzf nitcbase.tar.gz
	tar -xvzf xfs-interface.tar.gz
	rm nitcbase.tar.gz xfs-interface.tar.gz
	mv nitcbase-master mynitcbase
	mv xfs-interface-master XFS_Interface
	(cd XFS_Interface && make)
	mkdir -p {Disk,Files/Batch_Execution_Files,Files/Input_Files,Files/Output_Files}
	echo NITCbase package installed.

else
	# Take action if $DIR exists. #
	echo "ERROR: $DIR directory already exists. If you want to install a fresh copy, remove the existing directory and retry."
fi
