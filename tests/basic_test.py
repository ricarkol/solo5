from os import geteuid

import pexpect
import time

from utils_test import TIMEOUT

VIRTIO = '../tools/run/solo5-run-virtio.sh'


def _test_expect_success(unikernel_cmd):
    (output, status) = pexpect.run(unikernel_cmd, withexitstatus=True, timeout=TIMEOUT)
    assert 'SUCCESS' in output
    assert status in [0, 2, 83]


def _test_expect_abort(unikernel_cmd):
    (output, status) = pexpect.run(unikernel_cmd, withexitstatus=True, timeout=TIMEOUT)
    assert 'ABORT' in output
    assert status in [0, 2, 83]


def _test_blk(unikernel_cmd):
    pexpect.run('dd if=/dev/zero of=/tmp/disk.img bs=4k count=1024')
    try:
        (output, status) = pexpect.run(unikernel_cmd, withexitstatus=True, timeout=TIMEOUT)
        assert 'SUCCESS' in output
        assert status in [0, 2, 83]
    finally:
        pexpect.run('rm -f /tmp/disk.img')


def _test_ping_serve_flood(unikernel_cmd):
    assert(geteuid() == 0)
    time.sleep(0.5)  # the tap shows as used if we immediately try to use it
    vm = pexpect.spawn(unikernel_cmd, timeout=30)
    vm.expect('Serving ping on 10.0.0.2')
    ping = pexpect.spawn('ping -fq -c 100000 10.0.0.2', timeout=30)
    ping.expect('100000 packets transmitted, 100000 received, 0% packet loss')
    vm.expect('SUCCESS')
    vm.expect(pexpect.EOF)
    vm.close()
    assert vm.exitstatus in [0, 2, 83]


def _test_ping_serve(unikernel_cmd):
    time.sleep(0.5)  # the tap shows as used if we immediately try to use it
    vm = pexpect.spawn(unikernel_cmd, timeout=30)
    vm.expect('Serving ping on 10.0.0.2')
    ping = pexpect.spawn('ping -c 5 -i 0.2 10.0.0.2', timeout=30)
    ping.expect('64 bytes from 10.0.0.2: icmp_seq=5')
    vm.terminate()
    # the vm process is terminated, so its exit status could be 1


def _test_exit_on_ctrl_c(unikernel_cmd):
    vm = pexpect.spawn(unikernel_cmd, timeout=30)
    vm.expect('Serving ping on 10.0.0.2')
    vm.sendcontrol('c')
    vm.expect('on signal 2')
    vm.expect(pexpect.EOF)
    vm.close()
    assert vm.exitstatus in [0, 1, 2, 83]


def test_ukvm_hello():
    _test_expect_success('./test_hello/ukvm-bin test_hello/test_hello.ukvm Hello_Solo5')


def test_ukvm_exit_on_ctrl_c():
    _test_exit_on_ctrl_c('./test_ping_serve/ukvm-bin --net=tap100 test_ping_serve/test_ping_serve.ukvm')


def test_ukvm_globals():
    _test_expect_success('./test_globals/ukvm-bin test_globals/test_globals.ukvm')


def test_ukvm_fpu():
    _test_expect_success('./test_fpu/ukvm-bin test_fpu/test_fpu.ukvm')


def test_ukvm_exception():
    _test_expect_abort('./test_exception/ukvm-bin test_exception/test_exception.ukvm')


def test_ukvm_blk():
    _test_blk('./test_blk/ukvm-bin --disk=/tmp/disk.img -- test_blk/test_blk.ukvm')


def test_ukvm_quiet():
    (output, status) = pexpect.run('./test_quiet/ukvm-bin test_quiet/test_quiet.ukvm --solo5:quiet',
                                   withexitstatus=True, timeout=TIMEOUT)
    assert output == '\r\n**** Solo5 standalone test_verbose ****\r\n\r\nSUCCESS\r\n'
    assert status in [0]


def test_ukvm_ping_serve_flood():
    _test_ping_serve_flood('./test_ping_serve/ukvm-bin --net=tap100 test_ping_serve/test_ping_serve.ukvm limit')


def test_ukvm_ping_serve():
    _test_ping_serve('./test_ping_serve/ukvm-bin --net=tap100 test_ping_serve/test_ping_serve.ukvm')


def test_virtio_quiet():
    (output, status) = pexpect.run('%s -- test_quiet/test_quiet.virtio --solo5:quiet' % VIRTIO,
                                   withexitstatus=True, timeout=TIMEOUT)
    assert 'Solo5:' not in output
    assert output.endswith('\r\r\n**** Solo5 standalone test_verbose ****\r\r\n\r\r\nSUCCESS\r\r\n')
    assert status in [0, 2, 83]


def test_virtio_hello():
    _test_expect_success('%s -- test_hello/test_hello.virtio Hello_Solo5' % VIRTIO)


def test_virtio_exit_on_ctrl_c():
    _test_exit_on_ctrl_c('%s -n tap100 -- test_ping_serve/test_ping_serve.virtio' % VIRTIO)


def test_virtio_globals():
    _test_expect_success('%s -- test_globals/test_globals.virtio' % VIRTIO)


def test_virtio_fpu():
    _test_expect_success('%s -- test_fpu/test_fpu.virtio' % VIRTIO)


def test_virtio_exception():
    _test_expect_abort('%s -- test_exception/test_exception.virtio' % VIRTIO)


def test_virtio_blk():
    _test_blk('%s -d /tmp/disk.img -- test_blk/test_blk.virtio' % VIRTIO)


def test_virtio_ping_serve_flood():
    _test_ping_serve_flood('%s -n tap100 -- test_ping_serve/test_ping_serve.virtio limit' % VIRTIO)


def test_virtio_ping_serve():
    _test_ping_serve('%s -n tap100 -- test_ping_serve/test_ping_serve.virtio limit' % VIRTIO)
