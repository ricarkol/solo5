#include "gdb-chain.h"

#include <sys/select.h>

#define MAX_CHAIN_LEN 10
#define ERROR(x...) do {                        \
        printf(x);                              \
        exit(1);                                \
    } while(0)


int main(int argc, char **argv) {
    int i;
    int data_fd[MAX_CHAIN_LEN];
    char buf[GDB_CHAIN_BUF_LEN];
    char *tosend;
        
    if ( argc < 3 )
        ERROR("Usage %s <first arg (num)> [socket paths...]\n", argv[0]);
    
    if ( argc - 2 > MAX_CHAIN_LEN )
        ERROR("max chain len is %d\n", MAX_CHAIN_LEN);

    for ( i = 0; i < argc - 2; i++ ) {
        struct sockaddr_un addr;

        printf("setting up [%d] at %s\n", i, argv[i + 2]);
        
        data_fd[i] = socket(AF_UNIX, SOCK_STREAM, 0);
        if ( data_fd[i] < 0 )
            ERROR("couldn't set up socket for %s\n", argv[i + 2]);

        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, argv[i + 2], sizeof(addr.sun_path) - 1);
        unlink(argv[i + 2]);
        
        if ( bind(data_fd[i], (struct sockaddr *)&addr, sizeof(addr)) )
            ERROR("couldn't bind to socket for %s\n", argv[i + 2]);

        if ( listen(data_fd[i], 0) )
            ERROR("couldn't listen to socket for %s\n", argv[i + 2]);

        if (gdb_proxy_connect_to_ukvm(i, 1235 + i) != 0)
            ERROR("Could not connect to ukvm %d on port %d\n", i, 1235 + i);
    }

    gdb_proxy_wait_for_connect(1234);
    gdb_proxy_handle_exception(0);
    
    tosend = argv[1];
    memset(buf, 0, GDB_CHAIN_BUF_LEN);

    for ( i = 0; i < argc - 2; i++ ) {    
        int ukvm;
        int ret;
        int len;
     
        printf("waiting to send %s to [%d] at %s\n", tosend, i, argv[i + 2]);
        ukvm = accept(data_fd[i], NULL, NULL);
        if ( ukvm < 0 )
            ERROR("couldn't accept socket for %s\n", argv[i + 2]);

        len = strlen(tosend);
        ret = write(ukvm, tosend, len);
        if ( ret != len ){
            perror("write");
            ERROR("couldn't write %s (len %d) ret=%d\n", tosend, len, ret);
        }

        memset(buf, 0, GDB_CHAIN_BUF_LEN);
        ret = read(ukvm, buf, sizeof(buf) - 1);
        if ( ret <= 0 )
            ERROR("couldn't read (ret %d)\n", ret);

        tosend = buf;
    }

    return 0;
}
