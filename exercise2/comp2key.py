#!/usr/bin/python2.7
##############################################################################
# Use this script to magically transform your key components into a fully-
# fledged GPG secret key.
# Note that the creation timestamp of a key is actually used to determine
# the key ID, so you have to pass the original timestamp - otherwise you'll
# get a different key ID and the signature will be rejected.
# (The key ID is printed as a diagnostic message while the script runs.
# Double-check this against the expected value.)
#
# Dump your calculated components into a file with the usual format (order
# doesn't matter, but completeness does):
# N = 0x1234....
# D = 0x5656....
# Then call this script, passing the timestamp (as Unix epoch) as second
# argument:
# ./comp2key.py keycomponents.txt 0x30403040
#
# You can optionally add a third argument which specifies the key validity
# in seconds:
# ./comp2key.py keycomponents.txt 0x30403040 0xffffff
#
# The resulting GPG secret key will be written to "./keyfile". You can then
# import that using `gpg --import`.
##############################################################################

import sys
import time
import re
import struct
import hashlib

TIME = 0
SIGVALIDITY = 0

if len(sys.argv) != 3 and len(sys.argv) != 4:
	print("Usage: comp2key.py COMPONENTS_FILE KEY_TIMESTAMP [KEY_VALIDITY]")
	sys.exit(1)

specfile = open(sys.argv[1])
try:
	TIME = int(sys.argv[2], 16)
except ValueError:
	print("Error: KEY_TIMESTAMP should be a hexadecimal number")
	sys.exit(1)

if len(sys.argv) == 4:
	try:
		SIGVALIDITY = int(sys.argv[3], 16)
	except ValueError:
		print("Error: KEY_TIMESTAMP should be a hexadecimal number")
		sys.exit(1)

if TIME == 0:
	print("Error: KEY_TIMESTAMP should be a non-zero number")
	sys.exit(1)
if SIGVALIDITY == 0:
	print("Setting default validity")
	SIGVALIDITY = 0x01f4fa00

components = [ None, None, None, None, None, None, None, None ]
component_names = [ "N", "E", "D", "P", "Q", "DP", "DQ", "PINV" ]
components_set = 0

line_regex = "^(" + "|".join(component_names) + ")\s*=\s*(0x([0-9a-fA-F]?[0-9a-fA-F]{2})*)$"

while True:
	line = specfile.readline()

	linematch = re.match(line_regex, line)
	if linematch == None:
		break

	comp_index = component_names.index(linematch.group(1))

	if components[comp_index] != None:
		print("Error: %s already set" % linematch.group(1))
		sys.exit(1)

	components[comp_index] = linematch.group(2)
	components_set += 1

if components_set < len(component_names):
	print("Error: not all components set")
	for c in range(len(component_names)):
		if components[c] == None:
			print("Missing: %s" % component_names[c])
	sys.exit(1)

component_bitlengths = []

for c in range(len(components)):
	bitlen = int(components[c], 16).bit_length()
	component_bitlengths.append(bitlen)
	# strip leading "0x"
	components[c] = components[c][2:]
	# strip leading stretch of 0s (if present)
	first_significant = len(components[c]) - (bitlen+3)/4
	components[c] = components[c][first_significant:]
	# add back a single one if we have odd length
	if len(components[c]) % 2 == 1:
		components[c] = "0" + components[c]
	### print("%s = %s len %d" % (component_names[c], components[c], component_bitlengths[c]))

pubkey = ""
seckey = ""

selfsighash = hashlib.sha1()

SIGTIME = int(time.time())

pubkey += struct.pack(">BHBIB", 0x99, 0, 0x04, TIME, 0x01)
pubkey += struct.pack(">H", component_bitlengths[0]) + components[0].decode("hex")
pubkey += struct.pack(">H", component_bitlengths[1]) + components[1].decode("hex")
pubkey = pubkey[0:1] + struct.pack(">H", len(pubkey) - 3) + pubkey[3:]

keyid = hashlib.sha1(pubkey).hexdigest()
print("KeyID: %s" % keyid)

selfsighash.update(pubkey)

def bytesum(string):
	v = 0
	for i in range(len(string)):
		v += int(string[i].encode("hex"), 16)
	return (v % 65536)

def pkcs_rsa_sha1_sig(sha1sig, n, d):
	PKCS_A = "0001"
	PKCS_B = "003021300906052b0e03021a05000414" + sha1sig
	nlength = int(n, 16).bit_length()
	if nlength % 8 != 0:
		nlength += 8 - (nlength % 8)
	PKCSMSG = PKCS_A + ((nlength/8 - len(PKCS_A)/2 - len(PKCS_B)/2) * "ff") + PKCS_B
	PKCSSIG = pow(int(PKCSMSG, 16), int(d, 16), int(n, 16))
	PKCSSIGSTR = ("%x" % PKCSSIG)
	if len(PKCSSIGSTR) % 2:
		PKCSSIGSTR = "0" + PKCSSIGSTR
	return PKCSSIGSTR

seckey += struct.pack(">BHBIB", 0x95, 0, 0x04, TIME, 0x01)
seckey += struct.pack(">H", component_bitlengths[0]) + components[0].decode("hex")
seckey += struct.pack(">H", component_bitlengths[1]) + components[1].decode("hex")
seckey += struct.pack(">B", 0x00)
seckey_priv = ""
seckey_priv += struct.pack(">H", component_bitlengths[2]) + components[2].decode("hex")
seckey_priv += struct.pack(">H", component_bitlengths[3]) + components[3].decode("hex")
seckey_priv += struct.pack(">H", component_bitlengths[4]) + components[4].decode("hex")
seckey_priv += struct.pack(">H", component_bitlengths[7]) + components[7].decode("hex")
seckey += seckey_priv + struct.pack(">H", bytesum(seckey_priv))

seckey = seckey[0:1] + struct.pack(">H", len(seckey) - 3) + seckey[3:]

KEYNAME = "Comp2Key <gpg@comp2key.local>"
uid = struct.pack(">BB", 0xb4, len(KEYNAME)) + KEYNAME

selfsighash.update(struct.pack(">BI", 0xb4, len(KEYNAME)) + KEYNAME)

STD_SIG = "00280502" + ("%08x" % SIGTIME)
STD_SIG += "021b03" + "0509" + ("%08x" % SIGVALIDITY)
STD_SIG += "060b090807030206150802090a0b0416020301021e01021780"
STD_SIG = STD_SIG.decode("hex")

sig = struct.pack(">BHBBBB", 0x89, 0, 0x04, 0x13, 0x01, 0x02)
sig += STD_SIG

selfsighash.update(sig[3:])

sig += struct.pack(">HBB", 0x0a, 0x09, 0x10) + keyid[24:].decode("hex")

selfsighash.update(struct.pack(">BBI", 0x04, 0xff, 0x2e))

sighash = selfsighash.hexdigest()

sig += sighash[0:4].decode("hex")

rsasig = pkcs_rsa_sha1_sig(sighash, components[0], components[2])

sig += struct.pack(">H", int(rsasig, 16).bit_length()) + rsasig.decode("hex")

sig = sig[0:1] + struct.pack(">H", len(sig) - 3) + sig[3:]

keyfile = open("./keyfile", "w")
keyfile.write(seckey)
keyfile.write(uid)
keyfile.write(sig)
keyfile.close()
