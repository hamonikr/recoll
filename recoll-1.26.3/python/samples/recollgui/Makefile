PYUIC = pyuic5
all: rclmain.py

rclmain.py: rclmain.ui
	$(PYUIC) -o rclmain.py rclmain.ui

clean:
	rm -f rclmain.py rclmain.pyc
