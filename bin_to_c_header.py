# Convert a binary input into a C header

import sys

if len(sys.argv) != 4:
    print("Usage: {} <name> <input.bin> <output.h>")
    sys.exit(1)

with open(sys.argv[2], "rb") as f:
    data = f.read()

arr = ""

l = 0
for c in data:
    if l > 0:
        arr = arr + ","
        if l % 8 == 0:
            arr = arr + "\n    "
    arr = arr + "0x{:02X}".format(c)
        
    l = l + 1


output = '''/* NOTE: THIS FILE WAS AUTOMATICALLY GENERATED */
static unsigned char {name}[] = {{
    {arr}
}};
'''.format(name_caps=sys.argv[1].upper(), name=sys.argv[1], arr=arr)

with open(sys.argv[3], "w") as f:
    f.write(output)
