#!/bin/sh

sed -e 's/$//' <<EOH
Content-Type: text/html

<html>
  <head>
    <title>List of Files</title>
  </head>
  <body>
    <h1>List of Files</h1>
EOH

umask 0000
echo 'list' | nc 127.0.0.1 1337 | \
while read; do
  echo "    <p>$REPLY</p>"
done

sed -e 's/$//' <<EOH
  </body>
</html>
EOH

exit 0
