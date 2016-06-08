all: ukvm gdb-chain

gdb-chain: gdb-chain.c gdb-chain.h gdb-proxy.c
	gcc -o gdb-chain gdb-chain.c gdb-proxy.c

ukvm: ukvm.c ukvm.h misc.h gdb-stub.c gdb-chain.h
	gcc -o ukvm ukvm.c gdb-stub.c -lpthread -lrt

clean:
	rm -f ukvm gdb-chain
