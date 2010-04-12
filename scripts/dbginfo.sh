#!/bin/bash 

###############################################################################
# collect some configuration, trace, and debug
# information about the S390 Linux system
#
# Copyright IBM Corp. 2002, 2006.
###############################################################################


# variables
#

SCRIPTNAME="dbginfo.sh"
WORKDIR=DBGINFO-`date +%Y-%m-%d-%H-%M-%S`-`hostname`
rc_check_zvm=0
# write output to following directory
WORKPFX=/tmp
WORKPATH=$WORKPFX/$WORKDIR
# write output to following files
CMDOUTPUT=runtime.out
VM_CMDOUTPUT=zvm_runtime.out
SYSFSFILELIST=sysfsfiles.out
FCPCONFOUTPUT=scsi
MOUNT_POINT_DEBUGFS="/sys/kernel/debug"
kernel_version_tmp=0

LOGFILE=dbginfo.log

# procfs entries to be collected (except s390dbf and scsi)
PROCFILES=" \
    /proc/sysinfo \
    /proc/version \
    /proc/cpuinfo \
    /proc/meminfo \
    /proc/buddyinfo \
    /proc/slabinfo \
    /proc/modules \
    /proc/mounts \
    /proc/partitions \
    /proc/stat \
    /proc/devices \
    /proc/misc \
    /proc/qeth \
    /proc/qeth_perf \
    /proc/qeth_ipa_takeover \
    /proc/cmdline \
    /proc/crypto \
    /proc/diskstats \
    /proc/dasd/devices \
    /proc/dasd/statistics \
    /proc/sys/vm/cmm_pages \
    /proc/sys/vm/cmm_timed_pages \
    /proc/sys/vm/cmm_timeout \
    "
PROCFILES_24=" \
    /proc/subchannels \
    /proc/chpids \
    /proc/chandev \
    /proc/ksyms \
    /proc/lvm/global \
    "

# log files to be collected
LOGFILES=" \
    /var/log/messages* \
    /var/log/boot.log* \
    /var/log/IBMtape.trace \
    /var/log/IBMtape.errorlog \
    /var/log/lin_tape.trace \
    /var/log/lin_tape.errorlog \
    /var/log/sa/* \
    "
# config files to be collected;
# additional all files "modules.dep" are collected
CONFIGFILES=" \
    /etc/ccwgroup.conf \
    /etc/chandev.conf \
    /etc/modules.conf \
    /etc/modprobe.conf* \ 
    /etc/fstab \
    /etc/syslog.conf \
    /etc/sysconfig \
    /etc/crontab \
    /etc/cron.* \
    /etc/exports \
    /etc/sysctl.conf \
    /etc/zipl.conf \
    /etc/lvm \
    /etc/IBMtaped.conf \
    /etc/lin_taped.conf \
    /etc/multipath.conf \
    /etc/rc.d \
    /etc/zfcp.conf \
    "

#sysfs files NOT to be collected
SYSFSFILEEXCLUDES="\
	trace_pipe\
	"

# collect output of following commands;
# commands are separated by ':'
CMDS="uname -a\
    :uptime\
    :iptables -L\
    :ulimit -a\
    :ps -eo pid,tid,nlwp,policy,user,tname,ni,pri,psr,sgi_p,stat,wchan,start_time,time,pcpu,pmem,vsize,size,rss,share,command\
    :ps axX\
    :dmesg -s 1048576\
    :ifconfig -a\
    :route -n\
    :ip route list\
    :ip rule list\
    :ip neigh list\
    :ip link show\
    :netstat -pantu\
    :netstat -s\
    :dmsetup ls\
    :dmsetup ls --tree\
    :dmsetup table\
    :dmsetup table --target multipath\
    :dmsetup status\
    :multipath -v6 -ll\
    :multipath -d\
    :lsqeth\
    :lscss\
    :lsdasd\
    :ziorep_config -ADM\
    :lsmod\
    :lsdev\
    :lsscsi\
    :lstape\
    :lszfcp\
    :lszfcp -D\
    :lszfcp -V\
    :SPident\
    :rpm -qa\
    :sysctl -a\
    :lsof\
    :mount\
    :df -h\
    :pvpath -qa
    :ls -la /boot\
    :java -version\
    :cat /root/.bash_history\
    "

#z/VM commands

VM_CMDS="q userid\
	:q users\
	:q v osa\
	:q v dasd\
	:q v crypto\
	:q v fcp\
	:q v pav\
	:q v st\
	:q st\
	:q xstore\
	:q sxspages\
	:q v sw\
	:q vmlan\
	:q vswitch\
	:q vswitch details\
	:q vswitch access\
	:q set\
	:q comm\
	:q controller all\
	:q cplevel\
	:q cpus\
	:q fcp\
	:q frames\
	:q lan\
	:q lan all details\
	:q lan all access\
	:q cache\
	:q nic\
	:q pav\
	:q privclass\
	:q proc\
	:q qioass\
	:q spaces\
	:q swch all\
	:q timezone\
	:q trace\
	:q vtod\
	:q srm\
	:q mdcache\
	:q alloc page\
	:q alloc spool\
	:q dump\
	:ind load\
	:ind sp\
	:ind user\
	"
#
# function definitions
#

# print version info
printversion()
{
    cat <<EOF
$SCRIPTNAME: Debug information script version %S390_TOOLS_VERSION%
Copyright IBM Corp. 2002, 2006
EOF
}

# print usage and help
printhelp()
{
    cat <<EOF
Usage: $SCRIPTNAME [OPTIONS]

This script collects some runtime, configuration and
trace information about your Linux for zSeries system
for debugging purposes. It also traces information 
about z/VM if the Linux runs under z/VM.
This information is written to a file
	/tmp/DBGINFO-[date]-[time]-[hostname].tgz
where [date] and [time] are the date and time when debug
data is collected. [hostname] indicates the hostname of the
system the data was collected from.

Options:

        -h, --help       print this help
        -v, --version    print version information

Please report bugs to: linux390@de.ibm.com
EOF
}

# copy file $1 to $WORKPATH
collect_file_contents()
{
    echo "  $1" >>  $LOGFILE
    if [ ! -e $1 ]
    then
	echo "  WARNING: No such file: \"$1\"" >> $LOGFILE
	return 1
    elif [ ! -r $1 ]
    then
	echo "  WARNING: Permission denied: \"$1\"" >> $LOGFILE
	return 1
    else
	if [ ! -e $WORKPATH`dirname $1` ]
	then
	    mkdir --parents $WORKPATH`dirname $1`
	fi
	cp -r -d -L --parents $1 $WORKPATH 2>> $LOGFILE
# 	head -c 10m $1 >> $2
	if [ $? -ne 0 ]
	then
	    echo "  WARNING: cp failed for file: \"$1\"" >> $LOGFILE
	    return 1
	else
	    return 0
	fi
    fi
}

# append output of command $1 to file $2
collect_cmd_output()
{
    local raw_cmd=`echo $1 | awk '{ print $1 }'`;

    echo "#######################################################">>$2

    # check if command exists
    which $raw_cmd >/dev/null 2>&1;
    if [ $? -ne 0 ]
    then
        # check if command is a builtin
        command -v $raw_cmd >/dev/null;
        [ $? -ne 0 ] && echo "command '$raw_cmd' not available">>$2 && return 1;
    fi

    echo "$USER@$HOST> $1">>$2
    $1 1>>$2 2>&1
    echo "" >>$2
    if [ $? -ne 0 ]
    then
	echo "  WARNING: Command not successfully completed: \"$1\"">>  $2
	return 1
    else
	return 0
    fi
    
}

# check cmd line arguments
check_cmdline()
{
    # currently no options available
    if [ $# -eq 1 ]
    then
	if [ $1 = '-h' ] ||  [ $1 = '--help' ]
	then
	    printhelp
	elif [ $1 = '-v' ] ||  [ $1 = '--version' ]
	then
	    printversion
	else
	    echo 
	    echo "$SCRIPTNAME: invalid option $1"
	    echo "Try '$SCRIPTNAME --help' for more information"
	    echo 
	fi
	exit 0
    elif [[ $# -ge 1 ]]
    then
	echo "ERROR: Invalid number of arguments"
	echo 
	printhelp
	exit 1
    fi    
}

# change into temporary directory; if necessary create the directory
prepare_workdir()
{
    if [ ! -e $WORKPFX ]
    then
    	mkdir $WORKPFX
    fi
	    	
    if [ -e $WORKPATH ]
    then
	# remove old stuff
	echo "Clean up target directory $WORKPATH"
	rm -rf $WORKPATH/*
    else
	echo "Create target directory $WORKPATH"
	mkdir $WORKPATH
    fi
    echo "Change to target directory $WORKPATH"
    cd $WORKPATH
}

# collect single proc fs entries
# (PRCFILES should not contain /proc/scsi and /proc/s390dbf)
collect_procfs()
{
    echo "Get procfs entries" | tee -a $LOGFILE
    for i in $*
    do
	collect_file_contents $i
    done
}

# collect procfs entries of /proc/s390dbf
collect_s390dbf()
{
    echo "Get entries of /proc/s390dbf" | tee -a $LOGFILE
    if [ -e /proc/s390dbf ]
    then
	for i in `find /proc/s390dbf -type f \
                  -not -path "*/raw" -not -path "*/flush"`
	do
	    collect_file_contents $i
 	done
    else
	echo "  WARNING: /proc/s390dbf not found" | tee -a $LOGFILE
    fi
}

# collect procfs entries of /proc/scsi
collect_procfs_scsi()
{
    echo "Get entries of /proc/scsi" | tee -a $LOGFILE
    if [ -e /proc/scsi ]
    then
	for i in `find /proc/scsi -type f \
                  -perm +0444`
	do
	    collect_file_contents $i
	done
    else
	echo "  WARNING: /proc/scsi not found" >> $LOGFILE
    fi
}

#check for excluded files in sysfs
check_for_excludes()
{
	for filename in ${SYSFSFILEEXCLUDES[@]}
	do
		if [ `basename $1` = $filename ]
		then
			return 1
		fi
	done
	return 0
}

# collect sysfs entries
collect_sysfs()
{
    local rc_mount=""
    # check if debugfs is mounted
    mount | grep -q $MOUNT_POINT_DEBUGFS
    rc_mount=$?
    if [ $rc_mount -eq 1 ] && [ $kernel_version_tmp -ge 13 ]; then
	mount -t debugfs debugfs $MOUNT_POINT_DEBUGFS
    fi

    echo "Get file list of /sys" | tee -a $LOGFILE
	collect_cmd_output "ls -Rl /sys" $SYSFSFILELIST
    
    echo "Get entries of /sys" | tee -a $LOGFILE
    for i in `find /sys -noleaf -type f -perm +444`
    	do
    		if [ -e $i ]
		then
			if check_for_excludes $i
			then
		    		collect_file_contents $i
			fi
		else
			echo "  WARNING: $i not found" | tee -a $LOGFILE
    		fi
	done
    #unmount debugfs if not mounted at the beginning
    if [ $rc_mount -eq 1 ] && [ $kernel_version_tmp -ge 13 ]; then
	umount $MOUNT_POINT_DEBUGFS
    fi

}

# collect output of commands
collect_cmdsout()
{
    local commands=$1
    local outputfile=$2
    if [ $rc_check_zvm -eq 1 ]; then
    	echo "Saving z/VM runtime information into $outputfile" | tee -a $LOGFILE
    else
    	echo "Saving runtime information into $outputfile" | tee -a $LOGFILE
    fi
    _IFS_ORIG=$IFS
    IFS=:
    for i in $commands
    do
	IFS=$_IFS_ORIG
	if [ $rc_check_zvm -eq 1 ]; then 
		i="$cp_tool $i"
	fi
	collect_cmd_output "$i" "$outputfile"
	IFS=:
    done
    IFS=$_IFS_ORIG
}

# config files and module dependencies
collect_config()
{
    echo "Copy config files" | tee -a $LOGFILE
    for i in $CONFIGFILES
    do
	collect_file_contents $i
    done
    for i in `find /lib/modules -name modules.dep`
    do
	collect_file_contents $i
    done
}

# Check if we run under z/VM and which Linux cp tool is installed
check_zvm()
{
    cp_tool=''
    echo "Check if we run under z/VM" | tee -a $LOGFILE
    # Are we running under z/VM 
    cat /proc/sysinfo | grep -q "z/VM"
    if [ $? -eq 1 ] 
    then
	echo " Running in LPAR" | tee -a $LOGFILE
	return 0 
    else
	echo " Running under z/VM" | tee -a $LOGFILE
	# Is vmcp installed
	which vmcp > /dev/null 2>&1
    	if [ $? -eq 1 ] 
    	then
		# is hcp installed
		which hcp > /dev/null 2>&1
		if [ $? -eq 1 ]
 		then
			echo " No cp tool installed" | tee -a $LOGFILE 
			return 0
		else
			cp_tool=hcp
			echo " Installed CP tool: $cp_tool" | tee -a $LOGFILE 
			return 1
		fi
	else
		cp_tool=vmcp
		echo " Installed CP tool: $cp_tool" | tee -a $LOGFILE 
		return 1
	fi
     fi
}

# Capture z/VM data
get_zvm_data()
{
	lsmod | grep -q $cp_tool
	rc_lsmod=$?
   	if [ $rc_lsmod -eq 1 ]; then
                if [ $cp_tool = "hcp" ]; then
                        modprobe cpint
                        sleep 2
                else
                        modprobe $cp_tool
                        sleep 2
                fi
        fi
        collect_cmdsout "$VM_CMDS" "$VM_CMDOUTPUT"
        if [ $rc_lsmod -eq 1 ]; then
                if [ $cp_tool = "hcp" ]; then
                        rmmod cpint
                else
                        rmmod $cp_tool
                fi
        fi
}

# Caputure FCP configuration data if possible
get_fcp_data()
{
    which ziomon_fcpconf >/dev/null 2>&1;
    [ $? -eq 0 ] && ziomon_fcpconf -o $FCPCONFOUTPUT;
}

# log files
collect_log()
{
    echo "Copy log files" | tee -a $LOGFILE
    for i in $LOGFILES
    do
	collect_file_contents $i
    done
}

# create gzip-ped tar file
create_package()
{
    cd $WORKPATH/..
    tar -czf $WORKDIR.tgz $WORKDIR 2>/dev/null
    cleanup;
    echo
    echo "Collected data was saved to:"
    echo "  $WORKPFX/$WORKDIR.tgz"
}

cleanup()
{
    [ -d $WORKPATH ] && rm -rf $WORKPATH;
}

emergency_exit()
{
    cleanup;
    exit;
}

#
# start of script
#

trap emergency_exit SIGHUP SIGTERM SIGINT

kernel_version=`uname -r`
kernel_version_tmp=`echo ${kernel_version:4:2} | sed s/[^0-9]//g`
check_cmdline $*
prepare_workdir
printversion >$LOGFILE
if [ $kernel_version_tmp -lt 13 ]
	then
		collect_s390dbf
	fi
collect_procfs $PROCFILES
collect_cmdsout "$CMDS" "$CMDOUTPUT"
if [ "${kernel_version:2:1}" \> 4 ]
    then
	collect_sysfs
    else
	collect_procfs $PROCFILES_24
fi
collect_procfs_scsi
check_zvm
rc_check_zvm=$?
if [ $rc_check_zvm -eq 1 ]; then
	get_zvm_data
fi
get_fcp_data
collect_config
collect_log
create_package

#
# end of script
#

#EOF
