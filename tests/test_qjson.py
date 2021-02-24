"""
testing qjson2json
"""

import qjson2json

def test_qjson():
    """
    test qjson2json
    """
    assert qjson2json.decode("a:b") == '{"a":"b"}'
    