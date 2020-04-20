#!/usr/bin/env python

import sys
import os
import subprocess

tlns = []

srcdir=sys.argv[1]
utildir='projects/' + os.environ.get('PROJECT') + '/build/utils'

ns = {}
rootnid = 0

def str_to_objid(s):
    return int(s.replace(':', ''), 16)

subprocess.run([utildir + '/file2obj', '-i', '/dev/null', '-o', srcdir + '/__ns', '-p', 'rw'])
rootnid = subprocess.run([utildir + '/objstat', '-i', srcdir + '/__ns'],
        stdout=subprocess.PIPE).stdout.decode('utf-8').strip()


print("%s d %s %s" % (srcdir + "/__ns.dat", rootnid, "."))
print("%s d %s %s" % (srcdir + "/__ns.dat", rootnid, ".."))

def print_namespaces(n):
    for (k, v) in n.items():
        print("# %s %s.dat" % (v[1], v[1]))
        print_namespaces(v[2])

for line in sys.stdin.readlines():
    line = line.strip()
    if len(line) == 0:
        continue
    _s = line.split('*')
    name = _s[0]
    oid = _s[1]
    elems = name.split('/')
    sym = False
    sym_target = ""
    if oid == "SYM":
        sym = True
        sym_target = _s[2]
    #print(str(elems) + " => " + str(oid))
    #if not elems[0] in tlns:
    #    tlns.append(elems[0])
    
    n = ns
    nid = rootnid
    tn = ''
    nn = srcdir + "/__ns"
    for e in elems[0:-1]:
        tn += e + '_'
        if not e in n:
            outf=srcdir + '/__ns_' + tn
            subprocess.run([utildir + '/file2obj', '-i', '/dev/null', '-o', outf,
                '-p', 'rw'])
            nsid = subprocess.run([utildir + '/objstat', '-i', outf],
                    stdout=subprocess.PIPE).stdout.decode('utf-8').strip()

            n[e] = (nsid, outf, {})
            print("%s d %s %s" % (nn + ".dat", nsid, e))
            print("%s d %s %s" % (outf + ".dat", nsid, "."))
            print("%s d %s %s" % (outf + ".dat", nid, ".."))
        nid = n[e][0]
        nn = n[e][1]
        n = n[e][2]

    outf=srcdir + '/__ns_' + tn + '.dat'
    if sym:
        print("%s s %s %s" % (outf, sym_target, elems[-1]))
    else:
        print("%s r %s %s" % (outf, oid, elems[-1]))


#print_namespaces(ns)
