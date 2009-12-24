"""\
@file lluuid.py
@brief UUID parser/generator.

$LicenseInfo:firstyear=2004&license=mit$

Copyright (c) 2004-2009, Linden Research, Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
$/LicenseInfo$
"""

import md5, random, socket, string, time, re
import uuid

def _int2binstr(i,l):
    s=''
    for a in range(l):
        s=chr(i&0xFF)+s
        i>>=8
    return s

def _binstr2int(s):
    i = long(0)
    for c in s:
        i = (i<<8) + ord(c)
    return i

class UUID(object):
    """
    A class which represents a 16 byte integer. Stored as a 16 byte 8
    bit character string.

    The string version is to be of the form:
    AAAAAAAA-AAAA-BBBB-BBBB-BBBBBBCCCCCC  (a 128-bit number in hex)
    where A=network address, B=timestamp, C=random.
    """

    NULL_STR = "00000000-0000-0000-0000-000000000000"

    # the UUIDREGEX_STRING is helpful for parsing UUID's in text
    hex_wildcard = r"[0-9a-fA-F]"
    word = hex_wildcard + r"{4,4}-"
    long_word = hex_wildcard + r"{8,8}-"
    very_long_word = hex_wildcard + r"{12,12}"
    UUID_REGEX_STRING = long_word + word + word + word + very_long_word
    uuid_regex = re.compile(UUID_REGEX_STRING)

    rand = random.Random()
    ip = ''
    try:
        ip = socket.gethostbyname(socket.gethostname())
    except(socket.gaierror):
        # no ip address, so just default to somewhere in 10.x.x.x
        ip = '10'
        for i in range(3):
            ip += '.' + str(rand.randrange(1,254))
    hexip = ''.join(["%04x" % long(i) for i in ip.split('.')])
    lastid = ''

    def __init__(self, possible_uuid=None):
        """
        Initialize to first valid UUID in argument (if a string),
        or to null UUID if none found or argument is not supplied.

        If the argument is a UUID, the constructed object will be a copy of it.
        """
        self._bits = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
        if possible_uuid is None:
            return

        if isinstance(possible_uuid, type(self)):
            self.set(possible_uuid)
            return

        uuid_match = UUID.uuid_regex.search(possible_uuid)
        if uuid_match:
            uuid_string = uuid_match.group()
            s = string.replace(uuid_string, '-', '')
            self._bits = _int2binstr(string.atol(s[:8],16),4) + \
                         _int2binstr(string.atol(s[8:16],16),4) + \
                         _int2binstr(string.atol(s[16:24],16),4) + \
                         _int2binstr(string.atol(s[24:],16),4) 

    def __len__(self):
        """
        Used by the len() builtin.
        """
        return 36

    def __nonzero__(self):
        return self._bits != "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"

    def __str__(self):
        uuid_string = self.toString()
        return uuid_string

    __repr__ = __str__

    def __getitem__(self, index):
        return str(self)[index]

    def __eq__(self, other):
        if isinstance(other, (str, unicode)):
            return other == str(self)
        return self._bits == getattr(other, '_bits', '')

    def __ne__(self, other):
        return not self.__eq__(other)

    def __le__(self, other):
        return self._bits <= other._bits

    def __ge__(self, other):
        return self._bits >= other._bits

    def __lt__(self, other):
        return self._bits < other._bits

    def __gt__(self, other):
        return self._bits > other._bits

    def __hash__(self):
        return hash(self._bits)

    def set(self, uuid):
        self._bits = uuid._bits

    def setFromString(self, uuid_string):
        """
        Given a string version of a uuid, set self bits
        appropriately. Returns self.
        """
        s = string.replace(uuid_string, '-', '')
        self._bits = _int2binstr(string.atol(s[:8],16),4) + \
                     _int2binstr(string.atol(s[8:16],16),4) + \
                     _int2binstr(string.atol(s[16:24],16),4) + \
                     _int2binstr(string.atol(s[24:],16),4) 
        return self

    def setFromMemoryDump(self, gdb_string):
        """
        We expect to get gdb_string as four hex units. eg:
        0x147d54db		0xc34b3f1b		0x714f989b		0x0a892fd2
        Which will be translated to:
        db547d14-1b3f4bc3-9b984f71-d22f890a
        Returns self.
        """
        s = string.replace(gdb_string, '0x', '')
        s = string.replace(s, ' ', '')
        t = ''
        for i in range(8,40,8):
            for j in range(0,8,2):
                t = t + s[i-j-2:i-j]
        self.setFromString(t)

    def toString(self):
        """
        Return as a string matching the LL standard
        AAAAAAAA-AAAA-BBBB-BBBB-BBBBBBCCCCCC  (a 128-bit number in hex)
        where A=network address, B=timestamp, C=random.
        """
        return uuid_bits_to_string(self._bits)

    def getAsString(self):
        """
        Return a different string representation of the form
        AAAAAAAA-AAAABBBB-BBBBBBBB-BBCCCCCC	 (a 128-bit number in hex)
        where A=network address, B=timestamp, C=random.
        """
        i1 = _binstr2int(self._bits[0:4])
        i2 = _binstr2int(self._bits[4:8])
        i3 = _binstr2int(self._bits[8:12])
        i4 = _binstr2int(self._bits[12:16])
        return '%08lx-%08lx-%08lx-%08lx' % (i1,i2,i3,i4)

    def generate(self):
        """
        Generate a new uuid. This algorithm is slightly different
        from c++ implementation for portability reasons.
        Returns self.
        """
        m = md5.new()
        m.update(uuid.uuid1().bytes)
        self._bits = m.digest()
        return self

    def isNull(self):
        """
        Returns 1 if the uuid is null - ie, equal to default uuid.
        """
        return (self._bits == "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0")

    def xor(self, rhs):
        """
        xors self with rhs.
        """
        v1 = _binstr2int(self._bits[0:4]) ^ _binstr2int(rhs._bits[0:4])
        v2 = _binstr2int(self._bits[4:8]) ^ _binstr2int(rhs._bits[4:8])
        v3 = _binstr2int(self._bits[8:12]) ^ _binstr2int(rhs._bits[8:12])
        v4 = _binstr2int(self._bits[12:16]) ^ _binstr2int(rhs._bits[12:16])
        self._bits = _int2binstr(v1,4) + \
                     _int2binstr(v2,4) + \
                     _int2binstr(v3,4) + \
                     _int2binstr(v4,4) 


# module-level null constant
NULL = UUID()

def printTranslatedMemory(four_hex_uints):
    """
    We expect to get the string as four hex units. eg:
    0x147d54db		0xc34b3f1b		0x714f989b		0x0a892fd2
    Which will be translated to:
    db547d14-1b3f4bc3-9b984f71-d22f890a
    """
    uuid = UUID()
    uuid.setFromMemoryDump(four_hex_uints)
    print uuid.toString()

def isUUID(id_str):
    """
    This function returns:
    - 1 if the string passed is a UUID
    - 0 is the string passed is not a UUID
    - None if it neither of the if's below is satisfied
    """
    if not id_str or len(id_str) <  5 or len(id_str) > 36:
        return 0

    if isinstance(id_str, UUID) or UUID.uuid_regex.match(id_str):
        return 1

    return None

def isPossiblyID(id_str):
    """
    This function returns 1 if the string passed has some uuid-like
    characteristics. Otherwise returns 0.
    """

    is_uuid = isUUID(id_str)
    if is_uuid is not None:
        return is_uuid

    # build a string which matches every character.
    hex_wildcard = r"[0-9a-fA-F]"
    chars = len(id_str)
    next = min(chars, 8)
    matcher = hex_wildcard+"{"+str(next)+","+str(next)+"}"
    chars = chars - next
    if chars > 0:
        matcher = matcher + "-"
        chars = chars - 1
    for block in range(3):
        next = max(min(chars, 4), 0)
        if next:
            matcher = matcher + hex_wildcard+"{"+str(next)+","+str(next)+"}"
            chars = chars - next
        if chars > 0:
            matcher = matcher + "-"
            chars = chars - 1
    if chars > 0:
        next = min(chars, 12)
        matcher = matcher + hex_wildcard+"{"+str(next)+","+str(next)+"}"
    #print matcher
    uuid_matcher = re.compile(matcher)
    if uuid_matcher.match(id_str):
        return 1
    return 0

def uuid_bits_to_string(bits):
    i1 = _binstr2int(bits[0:4])
    i2 = _binstr2int(bits[4:6])
    i3 = _binstr2int(bits[6:8])
    i4 = _binstr2int(bits[8:10])
    i5 = _binstr2int(bits[10:12])
    i6 = _binstr2int(bits[12:16])
    return '%08lx-%04lx-%04lx-%04lx-%04lx%08lx' % (i1,i2,i3,i4,i5,i6)

def uuid_bits_to_uuid(bits):
    return UUID(uuid_bits_to_string(bits))


try:
    from mulib import stacked
    stacked.NoProducer()  # just to exercise stacked
except:
    #print "Couldn't import mulib.stacked, not registering UUID converter"
    pass
else:
    def convertUUID(uuid, req):
        req.write(str(uuid))

    stacked.add_producer(UUID, convertUUID, "*/*")
    stacked.add_producer(UUID, convertUUID, "text/html")
