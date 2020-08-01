all: 
	@if [ "`uname -s`" = "Linux" ] ; then \
		make -f Makefile_linux ; \
	else \
		make -f Makefile_unix ; \
	fi

clean: 
	@if [ "`uname -s`" = "Linux" ] ; then \
		make clean -f Makefile_linux ; \
	else \
		make clean -f Makefile_unix ; \
	fi
