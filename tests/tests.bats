#!/usr/bin/env bats

setup() {
  SCRIPT_DIR=`pwd`
  MAKECONF=${SCRIPT_DIR}/../Makeconf
  [ ! -f ${MAKECONF} ] && skip "Can't find Makeconf, looked in ${MAKECONF}"
  eval $(grep -E ^BUILD_.+=.+ ${MAKECONF})

  case ${BATS_TEST_NAME} in
  *ukvm)
    [ "${BUILD_UKVM}" = "no" ] && skip "Can't run ukvm"
    OS="$(uname -s)"
    case ${OS} in
    Linux)
      [ -c /dev/kvm -a -w /dev/kvm ] || skip "There is no /dev/kvm"
      ;;
    FreeBSD)
      # TODO, just try and run the test anyway
      ;;
    *)
      skip "Don't know how to run ${BATS_TEST_NAME} on ${OS}"
      ;;
    esac
    UKVM=${TEST_DIR}/ukvm-bin
    ;;
  *virtio)
    [ "${BUILD_VIRTIO}" = "no" ] && skip "Can't run virtio"
    VIRTIO=${SCRIPT_DIR}/../tools/run/solo5-run-virtio.sh
    #[ -n "${DISK}" ] && VIRTIO="${VIRTIO} -d ${DISK}"
    #[ -n "${NET}" ] && VIRTIO="${VIRTIO} -n ${NET}"
    #(set -x; timeout 30s ${VIRTIO} -- ${UNIKERNEL} "$@")
    #STATUS=$?
    ;;
  esac

  NET=tap100
  NET_IP=10.0.0.2
  dd if=/dev/zero of=${BATS_TMPDIR}/disk.img bs=4k count=1024
}

teardown() {
  rm -f ${BATS_TMPDIR}/disk.img
}

@test "hello ukvm" {
  run timeout --foreground 30s test_hello/ukvm-bin test_hello/test_hello.ukvm Hello_Solo5
  [ "$status" -eq 0 ]
  [[ "$output" == *"SUCCESS"* ]]
}

@test "quiet ukvm" {
  run timeout --foreground 30s test_quiet/ukvm-bin test_quiet/test_quiet.ukvm --solo5:quiet
  [ "$status" -eq 0 ]
  [[ "$output" == *"SUCCESS"* ]]
}

@test "hello virtio" {
  run timeout --foreground 30s "${VIRTIO}" -- test_hello/test_hello.virtio Hello_Solo5
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"SUCCESS"* ]]
}

@test "exception ukvm" {
  run timeout --foreground 30s test_exception/ukvm-bin test_exception/test_exception.ukvm
  [ "$status" -eq 0 ]
  [[ "$output" == *"ABORT"* ]]
}

@test "exception virtio" {
  run timeout --foreground 30s "${VIRTIO}" -- test_exception/test_exception.virtio
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"ABORT"* ]]
}

@test "blk ukvm" {
  run timeout --foreground 30s test_blk/ukvm-bin --disk=${BATS_TMPDIR}/disk.img test_blk/test_blk.ukvm
  [ "$status" -eq 0 ]
  [[ "$output" == *"SUCCESS"* ]]
}

@test "blk virtio" {
  run timeout --foreground 30s "${VIRTIO}" -d ${BATS_TMPDIR}/disk.img -- test_blk/test_blk.virtio
  echo $output
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"SUCCESS"* ]]
}

@test "ping-serve ukvm" {
  UKVM=test_ping_serve/ukvm-bin
  UNIKERNEL=test_ping_serve/test_ping_serve.ukvm

  [ $(id -u) -ne 0 ] && skip "Need root to run this test, for ping -f"

  (
    sleep 1
    timeout 30s ping -fq -c 100000 ${NET_IP} 
  ) &

  run timeout --foreground 30s $UKVM --net=${NET} -- $UNIKERNEL limit
  echo "$output"
  [ "$status" -eq 0 ]
  [[ "$output" == *"SUCCESS"* ]]
}

@test "ping-serve virtio" {
  UNIKERNEL=test_ping_serve/test_ping_serve.virtio

  [ $(id -u) -ne 0 ] && skip "Need root to run this test, for ping -f"

  (
    sleep 1
    timeout 30s ping -fq -c 100000 ${NET_IP} 
  ) &

  run timeout --foreground 30s "${VIRTIO}" -n "${NET}" -- $UNIKERNEL limit
  echo "$output"
  [ "$status" -eq 0 -o "$status" -eq 2 -o "$status" -eq 83 ]
  [[ "$output" == *"SUCCESS"* ]]
}
