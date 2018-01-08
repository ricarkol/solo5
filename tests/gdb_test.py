import pexpect
import pytest
import os
from time import sleep

TESTS_DIR = os.path.join(os.path.dirname( __file__ ))
if TESTS_DIR != os.getcwd():
    print 'Please run from %s' % TESTS_DIR
    exit(1)

TIMEOUT = 3


def test_ukvm_gdb_hello():
    UKVM_BIN = '%s/ukvm-bin' % 'test_hello'
    UNIKERNEL = '%s/%s.ukvm' % ('test_hello', 'test_hello')
    ukvm = pexpect.spawn ('%s --gdb --gdb-port=8888 %s ARG1 ARG2' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    ukvm.expect('Waiting for a debugger')
    gdb = pexpect.spawn ('gdb --ex="target remote localhost:8888" %s' % UNIKERNEL, timeout=TIMEOUT)

    try:
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
    finally:
        ukvm.close()
        gdb.close()


def test_ukvm_gdb_hello_quit_from_gdb():
    UKVM_BIN = '%s/ukvm-bin' % 'test_hello'
    UNIKERNEL = '%s/%s.ukvm' % ('test_hello', 'test_hello')
    ukvm = pexpect.spawn ('%s --gdb --gdb-port=8888 %s ARG1 ARG2' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    ukvm.expect('Waiting for a debugger')
    gdb = pexpect.spawn ('gdb --ex="target remote localhost:8888" %s' % UNIKERNEL, timeout=TIMEOUT)

    try:
        ukvm.expect('Connection from debugger at 127.0.0.1')
        gdb.expect('Remote debugging using localhost')
        gdb.sendline('quit')
        gdb.expect('Quit anyway?')
        gdb.sendline('y')
        ukvm.expect('Debugger asked us to quit')
    finally:
        ukvm.close()
        gdb.close()


@pytest.mark.skipif(reason='Not implemented (yet)')
def test_ukvm_gdb_hello_exit_with_control_c():
    UKVM_BIN = '%s/ukvm-bin' % 'test_hello'
    UNIKERNEL = '%s/%s.ukvm' % ('test_hello', 'test_hello')

    ukvm = pexpect.spawn ('%s --gdb --gdb-port=8888 %s ARG1 ARG2' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    ukvm.expect('Waiting for a debugger')

    gdb = pexpect.spawn ('gdb --ex="target remote localhost:8888" %s' % UNIKERNEL, timeout=TIMEOUT)

    try:
        ukvm.expect('Connection from debugger at 127.0.0.1')
        ukvm.sendcontrol('c')
        ukvm.expect('Exiting on signal 2')
        ukvm.expect(pexpect.EOF)
        gdb.expect('\[Inferior 1 \(Remote target\) exited normally\]')
        gdb.sendline('quit')
    finally:
        ukvm.close()
        gdb.close()


def test_ukvm_gdb_hello_continue():
    UKVM_BIN = '%s/ukvm-bin' % 'test_hello'
    UNIKERNEL = '%s/%s.ukvm' % ('test_hello', 'test_hello')
    ukvm = pexpect.spawn ('%s --gdb --gdb-port=8888 %s ARG1 ARG2' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    ukvm.expect('Waiting for a debugger')
    gdb = pexpect.spawn ('gdb --ex="target remote localhost:8888" %s' % UNIKERNEL, timeout=TIMEOUT)
    try:
        ukvm.expect('Connection from debugger at 127.0.0.1')
        gdb.expect('Remote debugging using localhost')
        gdb.sendline('c')
        ukvm.expect('Hello, World')
        gdb.expect('\[Inferior 1 \(Remote target\) exited normally\]')
        gdb.sendline('quit')
    finally:
        ukvm.close()
        gdb.close()


def test_ukvm_gdb_time_break_on_clock_monotonic():
    UKVM_BIN = '%s/ukvm-bin' % 'test_time'
    UNIKERNEL = '%s/%s.ukvm' % ('test_time', 'test_time')
    ukvm = pexpect.spawn ('%s --gdb --gdb-port=8888 %s' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    ukvm.expect('Waiting for a debugger')
    gdb = pexpect.spawn ('gdb --ex="target remote localhost:8888" %s' % UNIKERNEL, timeout=TIMEOUT)
    try:
        ukvm.expect('Connection from debugger at 127.0.0.1')
        gdb.expect('Remote debugging using localhost')
        gdb.sendline('break solo5_clock_monotonic')
        for _ in range(6):
            gdb.sendline('c')
            gdb.expect('Breakpoint 1')
        ukvm.expect(['SUCCESS', 'ERROR'])
        gdb.expect('\[Inferior 1 \(Remote target\) exited normally\]')
        gdb.sendline('quit')
    finally:
        ukvm.close()
        gdb.close()


def test_ukvm_gdb_time_break_on_rdtsc():
    UKVM_BIN = '%s/ukvm-bin' % 'test_time'
    UNIKERNEL = '%s/%s.ukvm' % ('test_time', 'test_time')
    ukvm = pexpect.spawn ('%s --gdb --gdb-port=8888 %s' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    ukvm.expect('Waiting for a debugger')
    gdb = pexpect.spawn ('gdb --ex="target remote localhost:8888" %s' % UNIKERNEL, timeout=TIMEOUT)
    try:
        ukvm.expect('Connection from debugger at 127.0.0.1')
        gdb.expect('Remote debugging using localhost')
        gdb.sendline('break cpu_rdtsc')
        for _ in range(8):
            gdb.sendline('c')
            gdb.expect('Breakpoint 1')
        ukvm.expect(['SUCCESS', 'ERROR'])
        gdb.expect('\[Inferior 1 \(Remote target\) exited normally\]')
        gdb.sendline('quit')
    finally:
        ukvm.close()
        gdb.close()


def _test_ukvm_gdb_cookie():
    UKVM_BIN = '%s/ukvm-bin' % 'test_cookie'
    UNIKERNEL = '%s/%s.ukvm' % ('test_cookie', 'test_cookie')
    ukvm = pexpect.spawn ('%s --gdb --gdb-port=8888 %s ARG1 ARG2' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    ukvm.expect('Waiting for a debugger')
    gdb = pexpect.spawn ('gdb --ex="target remote localhost:8888" %s' % UNIKERNEL, timeout=TIMEOUT)
    try:
        ukvm.expect('Connection from debugger at 127.0.0.1')
        gdb.expect('Remote debugging using localhost')
        gdb.sendline('c')
        ukvm.expect('Hello, World')
    finally:
        ukvm.close()
        gdb.close()
