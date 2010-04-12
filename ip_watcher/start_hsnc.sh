#!/bin/bash
#
# start_hsnc.sh
#
# Copyright IBM Corp. 2003, 2006.
# Author(s): Utz Bacher <utz.bacher@de.ibm.com>
#
# wrapper start script for ip_watcher.pl, also cleanup, when ip_watcher.pl
# gets killed
#

#
# functions
#

function __usage
{
	echo ""
	if [ $kernel_version = 2.4 ]; then
		echo "For more information about HiperSocket Network Concentrator"
		echo "please refer to the 'Linux for zSeries and S/390 Device Drivers"
		echo "and Installation Commands' manual."
	else
		echo "For more information about HiperSocket Network Concentrator"
		echo "please refer to the 'Device Drivers, Features, and Commands'"
		echo "manual."
	fi
	echo ""
	echo "The latest version of this document can be found at:"
	echo ""
	echo "http://oss.software.ibm.com/linux390/index.shtml"
	echo ""
	exit 1

}

function PrintVersion
{
        echo "$script_name version %S390_TOOLS_VERSION%"
        echo "Copyright IBM Corp. 2003, 2006"
}
#
# main
#

script_name="HiperSocket Network Concentrator"      # name of this script
#
# what is the kernel version we are on ?
#
kernel_version=`uname -r`

if [ "${kernel_version:2:1}" \> 4 ]; then
	        kernel_version=2.6
else
	        kernel_version=2.4
fi


#
# parse options (currently none avail)  
#
case "$1" in
   -v | --version  ) PrintVersion
     	     exit 1 ;;
   -h | --help  ) __usage ;;               
esac

if [ X${1}X != XX ] && [ $kernel_version = 2.4 ] ; then
	if ! cat /proc/qeth | awk '{print $3}' | egrep "^$1$" > /dev/null; then
		echo interface $1 does not exist.
		exit
	fi
elif [ X${1}X != XX ] && [ $kernel_version = 2.6 ] ; then
	if ! ls /sys/class/net | grep "^$1$" > /dev/null; then
		echo interface $1 does not exist.
		exit
	fi
fi

ip_watcher.pl $*

echo ip_watcher.pl was terminated, cleaning up.

if [ X${1}X == XX ] ; then
	echo killing xcec-bridge
	killall xcec-bridge
fi

echo removing all parp entries from mc interfaces
if [ X${1}X == XX ] ; then
	if [ -e /proc/qeth ] ; then
		cat /proc/qeth | egrep ' mc | mc\+ ' | awk '{print $3}' | sed 's/$/\$/' > /tmp/ip_watcher.cleanup1
	else
		for DEV in $(ls /sys/devices/qeth/ | egrep '^.+\..+\..+')
        	do
                	if_name=`cat /sys/devices/qeth/$DEV/if_name | sed 's/$/\$/'`
                	rtr=`cat /sys/devices/qeth/$DEV/route4 2> /dev/null | egrep 'multicast'`
                	if [ -n "$rtr" ] ; then
                        	echo $if_name >> /tmp/ip_watcher.cleanup1
                	fi
        	done
	fi
else
	echo ${1}$ > /tmp/ip_watcher.cleanup1
fi

qethconf rxip list | sed 's/add/del/' | egrep -f /tmp/ip_watcher.cleanup1 > /tmp/ip_watcher.cleanup2


while read line; do
	qethconf $line > /dev/null 2>&1
done < /tmp/ip_watcher.cleanup2
rm /tmp/ip_watcher.cleanup1
rm /tmp/ip_watcher.cleanup2

echo removing all routes from connector interfaces
if [ -e /proc/qeth ] ; then
	cat /proc/qeth | egrep ' p.c | p+c | s.c | s+c ' | awk '{print $3}"$"' > /tmp/ip_watcher.cleanup1
else
	for DEV in $(ls /sys/devices/qeth/ | egrep '^.+\..+\..+')
        do
		if_name=`cat /sys/devices/qeth/$DEV/if_name | sed 's/$/\$/'`
		rtr=`cat /sys/devices/qeth/$DEV/route4 2> /dev/null | egrep 'connector'`
		if [ -n "$rtr" ] ; then
			echo $if_name >> /tmp/ip_watcher.cleanup1
		fi
	done
fi
route -n | egrep -f /tmp/ip_watcher.cleanup1 > /tmp/ip_watcher.cleanup2
while read line; do
	route del -net `echo $line | awk '{print $1 " netmask " $3 " dev " $8}'`
done < /tmp/ip_watcher.cleanup2
rm /tmp/ip_watcher.cleanup1
rm /tmp/ip_watcher.cleanup2
