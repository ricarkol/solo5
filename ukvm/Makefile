all: ukvm

ukvm: ukvm.c ukvm.h misc.h i386-stub.c
	gcc -o ukvm ukvm.c i386-stub.c -lpthread -lrt

clean:
	rm -f ukvm
