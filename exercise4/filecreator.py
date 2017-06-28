import sys
from struct import pack

def create_mp3():
    p = pack('>I', 0xfff30000)
    return p

def create_wav():
    p = pack('>I', 0x52494646)
    p += pack('>I', 0x00000000)
    p += pack('>I', 0x57415645)
    return p;

def create_ogg():
    p = pack('>I', 0x4F676753)
    p += pack('>I', 0x00020000)
    p += pack('>I', 0x00000000)
    p += pack('>I', 0x00002905)
    p += pack('>I', 0x00000000)
    p += pack('>I', 0x00006846)
    p += pack('>I', 0x59ce011e)
    p += pack('>I', 0x01766f72)
    p += pack('>I', 0x62697300)
    p += pack('>I', 0x00000002)
    return p;

def usage():
    print ('Usage: python filecreator.py <filename> <fileformat>')
    print ('  Filename does not need extension!')
    print ('  Fileformat can be ogg, wav or mp3')

def main():
    if len(sys.argv) != 3:
        usage()
        return 1

    fileContents = ''
    fileName = ''
    if sys.argv[2] == 'ogg':
        fileContents = create_ogg();
        fileName = '{}.ogg'.format(sys.argv[1])
    elif sys.argv[2] == 'wav':
        fileContents = create_wav();
        fileName = '{}.wav'.format(sys.argv[1])
    elif sys.argv[2] == 'mp3':
        fileContents = create_mp3();
        fileName = '{}.mp3'.format(sys.argv[1])
    else:
        usage()
        return 1

    f = open(fileName, 'wb')
    f.write(fileContents)
    f.close()
    print ('Created spoof file named {}'.format(fileName))

if __name__ == '__main__':
    main()
