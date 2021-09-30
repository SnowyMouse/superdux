# Resize a binary file

import sys

if len(sys.argv) != 4:
    print("Usage: {} <name> <input.bin> <length>")
    sys.exit(1)

length = int(sys.argv[3])

with open(sys.argv[2], "rb") as f:
    data = f.read()

if len(data) > length:
    print("Error: {} is larger than {}".format(sys.argv[2], length), file=sys.stderr)
    sys.exit(1)
elif len(data) < length:
    output = bytearray()
    for i in range(0, length - len(data)):
        output.append(0)

    with open(sys.argv[2], "ab") as f:
        f.write(output)
