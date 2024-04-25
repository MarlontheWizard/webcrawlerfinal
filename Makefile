#Must install libcurl, including the header files, on system.       \
 If using linux-based system, try 							        \
 [ sudo apt-get install libcurl4-openssl-dev ] in the command line. \
																	\
 If still unfamiliar with Makefiles, follow the following steps:    \
	While in the Web_Crawler directory type [make] in your CLI. 	\
	[make] will create an executable called run.out 				\
	To run the executable, now input [./run.out]					\
																	\
	If you make changes to the Crawl.c file, you might have to run 	\
	[make clean], and then [make] again so that the changes are 	\
	updated. The Makefile can be configured to automatically update \
	the changes if wanted. 											\
	

CC = gcc
.PHONY: run

run:
	$(CC) -o run.out Crawl.c -lcurl -lcjson -ltidy -I/usr/include/libxml2 -lxml2

jinsu: 
	$(CC) -o jinsu.out Jinsu.c -lcurl -lcjson -ltidy -I/usr/include/libxml2 -lxml2


clean:
	rm -rf run.out

cleanJin:
	rm -rf Jinsu.out