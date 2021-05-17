# This Python file uses the following encoding: utf-8

import sys
import os


def linecount(folder, sz):
    fs = os.listdir(folder)
    for f in fs:
        name = os.path.join(folder, f)
        if os.path.isdir(name):
            sz = linecount(name, sz)
        else:
            if f[-2:].lower() == '.h' or \
               f[-2:].lower() == '.c' or \
               f[-4:].lower() == '.cpp':
                with open(name, 'r') as fn:
                    sz += len(fn.readlines())
                    fn.close()
    return sz


if __name__ == "__main__":
    if len(sys.argv) < 3:
        exit()

    if sys.argv[-2] == "-linecount":
        sz = linecount(sys.argv[-1], 0)
        print ("linecount %s: %d" % (sys.argv[-1], sz) )
