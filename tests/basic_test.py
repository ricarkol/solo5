from os import geteuid

import pexpect

from utils_test import TIMEOUT, expect


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
    VIRTIO = ''
    UNIKERNEL = '%s/%s.virtio' % ('test_ping_serve', 'test_ping_serve')
    ukvm = pexpect.spawn ('%s -n tap100 %s limit' % (VIRTIO, UNIKERNEL), timeout=30)
    expect(ukvm, 'Serving ping on 10.0.0.2')
    ping = pexpect.spawn('ping -fq -c 100000 10.0.0.2', timeout=30)
    ping.expect('100000 packets transmitted, 100000 received, 0% packet loss')
    ukvm.expect('SUCCESS')


def test_ukvm_ping_serve():
    UKVM_BIN = '%s/ukvm-bin' % 'test_ping_serve'
    UNIKERNEL = '%s/%s.ukvm' % ('test_ping_serve', 'test_ping_serve')
    ukvm = pexpect.spawn ('%s --net=tap100 %s' % (UKVM_BIN, UNIKERNEL), timeout=TIMEOUT)
    expect(ukvm, 'Serving ping on 10.0.0.2')
    ping = pexpect.spawn('ping -c 10 -i 0.2 10.0.0.2', timeout=TIMEOUT)
    ping.expect('64 bytes from 10.0.0.2: icmp_seq=10')
    ukvm.terminate()


def test_virtio_ping_serve():
    VIRTIO = ''
    UNIKERNEL = '%s/%s.virtio' % ('test_ping_serve', 'test_ping_serve')
    ukvm = pexpect.spawn ('%s -n tap100 %s limit' % (VIRTIO, UNIKERNEL), timeout=30)
    expect(ukvm, 'Serving ping on 10.0.0.2')
    ping = pexpect.spawn('ping -c 10 -i 0.2 10.0.0.2', timeout=TIMEOUT)
    ping.expect('64 bytes from 10.0.0.2: icmp_seq=10')
    ukvm.terminate()


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
