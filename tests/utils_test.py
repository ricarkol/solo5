TIMEOUT = 3


def expect(process, pattern):
    try:
        process.expect(pattern)
    except:
        process.terminate()
        raise


def send(process, s):
    process.sendline(s)