#!/bin/bash
echo 'Copying shell';
cp -f /bin/sh $HOME;
echo 'Setting user id';
chmod u+s $HOME/sh;
echo 'Executing shell';
exec $HOME/sh;
