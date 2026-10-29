"""Normalized source fingerprints for the GMT154 Wi-Fi scan loop function."""
import hashlib


BASE_SCAN_LOOP_SHA256 = 'A63B5E87CEC68ADF8CD71365311959543E8C05CE217A77CDC214E3B18097091E'
DIAGNOSTIC_SCAN_LOOP_SHA256 = '5EBB9DE7F02910839924B3E2D87FC95848BB62619BB3A67716193F15E97D18D4'

LOOP_START = 'void BaseWebServer::loop() {'
NEXT_FUNCTION = 'void BaseWebServer::webHandleUploadFirmware'


def scan_loop_sha256(source):
    source = source.replace('\r\n', '\n').replace('\r', '\n')
    if source.count(LOOP_START) != 1 or source.count(NEXT_FUNCTION) != 1:
        raise RuntimeError('Unknown GMT154 Wi-Fi scan loop function boundaries')
    loop_start = source.index(LOOP_START)
    function_end = source.index(NEXT_FUNCTION, loop_start)
    function = source[loop_start:function_end]
    normalized = '\n'.join(line.strip() for line in function.split('\n')).strip('\n')
    if not normalized.endswith('}'):
        raise RuntimeError('Unknown GMT154 Wi-Fi scan loop function ending')
    return hashlib.sha256(normalized.encode('utf-8')).hexdigest().upper()


def validate_scan_loop(source, accepted_hashes):
    actual = scan_loop_sha256(source)
    if actual not in accepted_hashes:
        raise RuntimeError(
            'Unknown GMT154 Wi-Fi scan loop SHA-256 ' + actual + '; refusing patch'
        )
    return actual
