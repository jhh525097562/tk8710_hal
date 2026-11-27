#!/bin/sh 
if [ "$1" = "preinst" ]; then
echo "Stopping service..."
# /etc/init.d/S99mypackage stop || killall mypackage
exit 0
fi

if [ "$1" = "postinst" ]; then
echo "Updating binary..."
mv /tmp/test_uart /userdata/test_uart
chmod 755 /userdata/test_uart
echo "Restarting service..."
#/etc/init.d/S99mypackage start
#/usr/bin/test_uart /dev/ttyS1 921600 -v
echo "Update Success!"
exit 0
fi

exit 0
