all:
	gtkamlc --Xcc=-I../squeue ../squeue/squeue.c ../squeue/squeue.vapi gui.gtkaml --pkg gtk+-2.0 -o a.out

test:
	valac --save-temps --Xcc=-I.. squeue.vapi test.vala ../squeue.c -o v
	valac --save-temps --Xcc=-I.. squeue.vapi client.vala ../squeue.c -o c
