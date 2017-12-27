import pexpect
from os import geteuid

# Timeout for all commands (in seconds)
TIMEOUT = 3

def expect(process, pattern):
    try:
        process.expect(pattern)
    except:
        process.terminate()
        raise

def send(process, s):
    process.sendline(s)

def test_ukvm_no_tap():
    UKVM_BIN = '%s/ukvm-bin' % 'test_ping_serve'
    UNIKERNEL = '%s/%s.ukvm' % ('test_ping_serve', 'test_ping_serve')
    ukvm = pexpect.spawn ('%s --net=tapxxx %s' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    expect(ukvm, 'Could not attach interface: tapxxx')

def _test_ukvm_ping_serve_leaked_ukvm():
    UKVM_BIN = '%s/ukvm-bin' % 'test_ping_serve'
    UNIKERNEL = '%s/%s.ukvm' % ('test_ping_serve', 'test_ping_serve')
    ukvm = pexpect.spawn ('%s --net=tap100 %s' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    expect(ukvm, 'Serving ping on 10.0.0.2')
    ping = pexpect.spawn('ping -c 10 -i 0.2 10.0.0.2', timeout=TIMEOUT)
    ping.expect_list(list(['64 bytes from 10.0.0.2: icmp_seq='] * 10))
    ukvm.terminate()

def test_ukvm_ping_serve():
    UKVM_BIN = '%s/ukvm-bin' % 'test_ping_serve'
    UNIKERNEL = '%s/%s.ukvm' % ('test_ping_serve', 'test_ping_serve')
    ukvm = pexpect.spawn ('%s --net=tap100 %s' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    expect(ukvm, 'Serving ping on 10.0.0.2')
    ping = pexpect.spawn('ping -c 10 -i 0.2 10.0.0.2', timeout=TIMEOUT)
    ping.expect('64 bytes from 10.0.0.2: icmp_seq=10')
    ukvm.terminate()

def test_ukvm_ping_serve_flood():
    assert(geteuid() == 0)
    UKVM_BIN = '%s/ukvm-bin' % 'test_ping_serve'
    UNIKERNEL = '%s/%s.ukvm' % ('test_ping_serve', 'test_ping_serve')
    ukvm = pexpect.spawn ('%s --net=tap100 %s limit' % (UKVM_BIN, UNIKERNEL), timeout=30)
    expect(ukvm, 'Serving ping on 10.0.0.2')
    ping = pexpect.spawn('ping -fq -c 100000 10.0.0.2', timeout=30)
    ping.expect('100000 packets transmitted, 100000 received, 0% packet loss')
    ukvm.expect('SUCCESS')

def _test_virtio_ping_serve_flood():
    assert(geteuid() == 0)
    VIRTIO = ''
    UNIKERNEL = '%s/%s.virtio' % ('test_ping_serve', 'test_ping_serve')
    ukvm = pexpect.spawn ('%s -n tap100 %s limit' % (VIRTIO, UNIKERNEL), timeout=30)
    expect(ukvm, 'Serving ping on 10.0.0.2')
    ping = pexpect.spawn('ping -fq -c 100000 10.0.0.2', timeout=30)
    ping.expect('100000 packets transmitted, 100000 received, 0% packet loss')
    ukvm.expect('SUCCESS')

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
