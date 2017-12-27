import pexpect

TIMEOUT = 1

def test_gdb_hello():
    UKVM_BIN = '%s/ukvm-bin' % 'test_hello'
    UNIKERNEL = '%s/%s.ukvm' % ('test_hello', 'test_hello')
    ukvm = pexpect.spawn ('%s --gdb --gdb-port=8888 %s ARG1 ARG2' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    ukvm.expect('Waiting for a debugger')
    gdb = pexpect.spawn ('gdb --ex="target remote localhost:8888" %s' % UNIKERNEL, timeout=TIMEOUT)
    ukvm.expect('Connection from debugger at 127.0.0.1')
    gdb.expect('Remote debugging using localhost')
    gdb.sendline('break solo5_app_main')
    gdb.expect('Breakpoint 1')
    gdb.sendline('c')
    gdb.expect('Breakpoint 1')
    gdb.sendline('info local')
    gdb.expect('len =')
    gdb.sendline('s')
    gdb.expect('puts')
    gdb.sendline('print cmdline')
    gdb.expect('ARG1 ARG2')
    gdb.sendline('break platform_puts')
    gdb.expect('Breakpoint 2')
    gdb.sendline('c')
    gdb.expect('Breakpoint 2')
    gdb.sendline('bt')
    gdb.expect('at test_hello.c:')
    gdb.sendline('delete 2')
    gdb.expect('\n')
    gdb.sendline('c')
    ukvm.expect('Hello, World')
    gdb.expect('exited normally')
    gdb.sendline('quit')

def test_gdb_hello_quick_exit():
    UKVM_BIN = '%s/ukvm-bin' % 'test_hello'
    UNIKERNEL = '%s/%s.ukvm' % ('test_hello', 'test_hello')
    ukvm = pexpect.spawn ('%s --gdb --gdb-port=8888 %s ARG1 ARG2' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    ukvm.expect('Waiting for a debugger')
    gdb = pexpect.spawn ('gdb --ex="target remote localhost:8888" %s' % UNIKERNEL, timeout=TIMEOUT)
    ukvm.expect('Connection from debugger at 127.0.0.1')
    gdb.expect('Remote debugging using localhost')
    gdb.sendline('quit')
    gdb.expect('Quit anyway?')
    gdb.sendline('y')
    ukvm.expect('Debugger asked us to quit')

def test_gdb_hello_continue():
    UKVM_BIN = '%s/ukvm-bin' % 'test_hello'
    UNIKERNEL = '%s/%s.ukvm' % ('test_hello', 'test_hello')
    ukvm = pexpect.spawn ('%s --gdb --gdb-port=8888 %s ARG1 ARG2' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    ukvm.expect('Waiting for a debugger')
    gdb = pexpect.spawn ('gdb --ex="target remote localhost:8888" %s' % UNIKERNEL, timeout=TIMEOUT)
    ukvm.expect('Connection from debugger at 127.0.0.1')
    gdb.expect('Remote debugging using localhost')
    gdb.sendline('c')
    ukvm.expect('Hello, World')
    gdb.expect('exited normally')
    gdb.sendline('quit')

def test_gdb_random():
    UKVM_BIN = '%s/ukvm-bin' % 'test_random'
    UNIKERNEL = '%s/%s.ukvm' % ('test_random', 'test_random')
    ukvm = pexpect.spawn ('%s --gdb --gdb-port=8888 %s ARG1 ARG2' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    ukvm.expect('Waiting for a debugger')
    gdb = pexpect.spawn ('gdb --ex="target remote localhost:8888" %s' % UNIKERNEL, timeout=TIMEOUT)
    ukvm.expect('Connection from debugger at 127.0.0.1')
    gdb.expect('Remote debugging using localhost')
    gdb.sendline('c')
    ukvm.expect('Hello, World')
    gdb.expect('exited normally')
    gdb.sendline('quit')
