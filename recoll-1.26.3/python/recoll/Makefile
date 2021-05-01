all:
	echo libdir: $(libdir)
	test '.' = '.' || cp -rp ./recoll .
	libdir=$(libdir) python setup.py build
install:
	sudo python setup.py install
clean:
	rm -rf build 
	rm -f recoll/__init__.pyc
	rm -rf recoll/__pycache__
distclean: clean
	rm -f setup.py 
