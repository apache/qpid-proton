import sys
from jarray import zeros, array as _array

if (sys.version_info[0] == 2 and sys.version_info[1] == 5):
    array = _array
else:
    def array(obj, code):
        return obj
