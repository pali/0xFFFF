include ../../config.mk

ifeq (${HAVE_GUI},1)
all:
	gtkamlc --save-temps --Xcc=-I../squeue ../squeue/squeue.c ../fpid.c ../squeue/squeue.vapi gui.gtkaml extras.vapi --pkg gtk+-2.0 -o goxf

test:
	valac --save-temps --Xcc=-I.. squeue.vapi test.vala ../squeue.c -o v
	valac --save-temps --Xcc=-I.. squeue.vapi client.vala ../squeue.c -o c
else
all:
	@true
endif
