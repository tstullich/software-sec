from fractions import gcd

"""Constants needed to make this work"""
n = 0xb7c2ebc3e0c80da5c6d258e7d6e39995ad4258a7eae1d4d1534f987c14820b3f54695d7fcc9c4a019a9ddf97250c1b5907ba0a5ac2e5a281914a91f5a4a37906290c502302d4ae15ab9f7c646283b51d3fe1676587defd1b10c98e8931dda50a0e2c71a6fdd6f73441105dc401f9ef4fe97014ee5fde0bd43029fb20b37a29fd9a1bff82bcde8b412f81791eed5f453a8d195f5d23be0ef7a6a6a93cbea2cfea2b8cdd50174ab9c58ad896adb20bfc26ebae92baae66abf10c54f47c7fb5ea57078ab1a75777524df6de3144af8d6db2d42f4df598fb29ced8364b53e21f1cd07e3940f00d6d08eddb060ba7192e50c6cf842667b86f53a0cbdb908635f7758f70b090984934923802ac88355fd0c52ba4b09148782737d98fcb1cb2197655961d9b318aa91a788bbac0d7e66aa0b6c0f11e7f8b072cbdb35ef611685b9b5f7280225e98f8e5642a7a36c348e89645391ebf2bf535844fb1fbd0861aae69c3ce8d9e5aed92c4e74a263f0e1cf2b48a539a4979ca0deb74f66941d7a8a7fe7fc4964d569988973a4a40224774ce6d6f4c99f272f055eb96603bc92885f18333a52e7eda5fda432b9a8c28c2e3feda57f65634d7740c79b646bb18f57ac7609a2c52d044cce7894436772568f3d9f3d760bc1cee6852b2558fa7c98a5813f39eb574b47306068f65710ba1cfeac2e623a07e137fc7942c5328789ddf805d13e6bd
#q = 0xf59cc31339d001d37570dc0ccd986f3f5ea737fa9185c15dbc17e6bfef29435c79a7c22e8616738947cab8711b6a6e7b5704e5283b57892adad3b170c726f34d3a9859b1504e005ee4b69d4803cd56773c50ab01d6546ce66dcdb2be4a34e15160d8e0eb69184b699246b4228f6f25bfcc91970fa99ea3123409f6865b161423581a5f9522ef774f09818bfef6c2b1c51d06218a07dc717ec94bb231b062936bfd8794cb39bdf8dc05cd2c8bd74b1d0acb14d39bc293deb45fa52de89af30e4bc5688fb8116be7e7ad4332810c04903939ee2a356ea254b83fb811c76898672d24997d8647f969a8e02aa2f2016cb1e0c8a9afe99760cd37bf2794d4ea58951f
q = 0xeeaec98690e6e5041ac6bf2bdbb351362d4df524b8f44576b67e4a7beb35a6893a70482b5fcd21ca376fff0d42084ccd52c1c57ceaa43d7ec862b1ebc6ff22be383861d2dd58384f771d968d2c1c44d59f8f07cfbe5d7f6414725d0e0f0054036da3f117e5749a753f8ac91b6e663d33f210576265d78e065e6fe6fba5f2b21e1b2408760962c2f6ca93046e0b2e405c9171b8c5dc4a48b357dab9dfb9250483b81a72119fde7e03fdabd2e289007187a04d62155bc078620434e0b69cee7a5bd99b1e1a86ded562cc42f41396ec287f8a108ad77eda63939fb3409ab53f75228d7a40fc5e5911e3032a93e15f93bf36d6dc9962354a23cce6b7012981909267
e = 0x010001

def lcm(*numbers):
    """Return lowest common multiple."""
    def lcm(a, b):
        return (a * b) // gcd(a, b)
    return reduce(lcm, numbers, 1)

def egcd(a, b):
    prevx, x = 1, 0; prevy, y = 0, 1
    while b:
        q = a / b
        x, prevx = prevx - q * x, x
        y, prevy = prevy - q * y, y
        a, b = b, a % b
    return a, prevx, prevy

"""Function from SO to calculate multiplicative inverse"""
def modinv(a, m):
    g, x, y = egcd(a, m)
    if g != 1:
        raise Exception('modular inverse does not exist')
    else:
        return x % m

"""Computing p by dividing n with q"""
p = n / q

"""Sanity check to make sure we are doing things right"""
if p < q:
    print "p < q Smaller"

"""Calculate the totient to get d"""
totient = lcm(p - 1, q - 1)
d = modinv(e, totient)

if gcd(e, totient) == 1:
    print "Calculated totient and e have GCD = 1\n"

"""Calculating values for CRT (Note: P needs to be less than Q)"""
dP = modinv(e, p - 1)
dQ = modinv(e, q - 1)
pInv = modinv(p, q)

"""Printing all required values"""
print "P:", hex(p), "\n"
print "Q:", hex(q), "\n"
print "E:", hex(n), "\n"
print "N:", hex(n), "\n"
print "D:", hex(d), "\n"
print "dP:", hex(dP), "\n"
print "dQ:", hex(dQ), "\n"
print "pInv:", hex(pInv), "\n"
output = open('exercise2d.txt', 'w')
output.write('N = {}\n'.format(hex(n).rstrip("L")))
output.write('P = {}\n'.format(hex(p).rstrip("L")))
output.write('Q = {}\n'.format(hex(q).rstrip("L")))
output.write('E = {}\n'.format(hex(e).rstrip("L")))
output.write('D = {}\n'.format(hex(d).rstrip("L")))
output.write('DP = {}\n'.format(hex(dP).rstrip("L")))
output.write('DQ = {}\n'.format(hex(dQ).rstrip("L")))
output.write('PINV = {}\n'.format(hex(pInv).rstrip("L")))
