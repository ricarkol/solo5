import pexpect

from utils_test import TIMEOUT, expect, send


def test_ukvm_gdb_hello():
    UKVM_BIN = '%s/ukvm-bin' % 'test_hello'
    UNIKERNEL = '%s/%s.ukvm' % ('test_hello', 'test_hello')
    ukvm = pexpect.spawn ('%s --gdb --gdb-port=8888 %s ARG1 ARG2' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    ukvm.expect('Waiting for a debugger')
    gdb = pexpect.spawn ('gdb --ex="target remote localhost:8888" %s' % UNIKERNEL, timeout=TIMEOUT)
    ukvm.expect('Connection from debugger at 127.0.0.1')
    expect(gdb, 'Remote debugging using localhost')
    send(gdb, 'break solo5_app_main')
    expect(gdb, 'Breakpoint 1')
    send(gdb, 'c')
    expect(gdb, 'Breakpoint 1')
    send(gdb, 'info local')
    expect(gdb, 'len =')
    send(gdb, 's')
    expect(gdb, 'puts')
    send(gdb, 'print cmdline')
    expect(gdb, 'ARG1 ARG2')
    send(gdb, 'break platform_puts')
    expect(gdb, 'Breakpoint 2')
    send(gdb, 'c')
    expect(gdb, 'Breakpoint 2')
    send(gdb, 'bt')
    expect(gdb, 'at test_hello.c:')
    send(gdb, 'delete 2')
    expect(gdb, '\n')
    send(gdb, 'c')
    ukvm.expect('Hello, World')
    expect(gdb, 'exited normally')
    send(gdb, 'quit')


def test_ukvm_gdb_hello_quick_exit():
    UKVM_BIN = '%s/ukvm-bin' % 'test_hello'
    UNIKERNEL = '%s/%s.ukvm' % ('test_hello', 'test_hello')
    ukvm = pexpect.spawn ('%s --gdb --gdb-port=8888 %s ARG1 ARG2' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    ukvm.expect('Waiting for a debugger')
    gdb = pexpect.spawn ('gdb --ex="target remote localhost:8888" %s' % UNIKERNEL, timeout=TIMEOUT)
    ukvm.expect('Connection from debugger at 127.0.0.1')
    expect(gdb, 'Remote debugging using localhost')
    send(gdb, 'quit')
    expect(gdb, 'Quit anyway?')
    send(gdb, 'y')
    ukvm.expect('Debugger asked us to quit')


def test_ukvm_gdb_hello_continue():
    UKVM_BIN = '%s/ukvm-bin' % 'test_hello'
    UNIKERNEL = '%s/%s.ukvm' % ('test_hello', 'test_hello')
    ukvm = pexpect.spawn ('%s --gdb --gdb-port=8888 %s ARG1 ARG2' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    ukvm.expect('Waiting for a debugger')
    gdb = pexpect.spawn ('gdb --ex="target remote localhost:8888" %s' % UNIKERNEL, timeout=TIMEOUT)
    ukvm.expect('Connection from debugger at 127.0.0.1')
    expect(gdb, 'Remote debugging using localhost')
    send(gdb, 'c')
    ukvm.expect('Hello, World')
    expect(gdb, 'exited normally')
    send(gdb, 'quit')


def _test_ukvm_gdb_cookie():
    UKVM_BIN = '%s/ukvm-bin' % 'test_cookie'
    UNIKERNEL = '%s/%s.ukvm' % ('test_cookie', 'test_cookie')
    ukvm = pexpect.spawn ('%s --gdb --gdb-port=8888 %s ARG1 ARG2' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    ukvm.expect('Waiting for a debugger')
    gdb = pexpect.spawn ('gdb --ex="target remote localhost:8888" %s' % UNIKERNEL, timeout=TIMEOUT)
    ukvm.expect('Connection from debugger at 127.0.0.1')
    expect(gdb, 'Remote debugging using localhost')
    send(gdb, 'c')
    ukvm.expect('Hello, World')
