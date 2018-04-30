#!/bin/bash

########################################################################
# Variables                                                            #
########################################################################

PATH=/sbin:/bin:/usr/sbin:/usr/bin
LIBNAME=libnss_llmnr.so
LIBSONAME=libnss_llmnr.so.2
LIBREALNAME=libnss_llmnr.so.2.1
HEADERFILE=nss_llmnr.h
LIBDIR=/usr/local/lib
INCLUDEDIR=/usr/local/include

########################################################################
# Functions                                                            #
########################################################################

check_root()
{
    ID=`id -u`
    if [ "$ID" -ne "0" ]; then
        echo "Need root privileges to run this script"
        exit 1
    fi
    return 0
}

check_status()
{
    if [ "$?" -ne 0 ]; then
        echo "$1"
        exit 1
    fi
}

print_action()
{
    echo -n "$1..."
}

print_ok()
{
    echo -n "OK"
    echo ""
}

########################################################################
# Main                                                                 #
########################################################################

print_action "[Checking root]"
check_root
print_ok

print_action "[Checking '$LIBREALNAME' file]"
test -f $LIBREALNAME
check_status "Not found. Try recompile it"
chmod 755 $LIBREALNAME
print_ok

print_action "[Coping '$LIBREALNAME' into '$LIBDIR']"
cp $LIBREALNAME $LIBDIR
check_status "Fail Coping '$LIBREALNAME' into '$LIBDIR'"
print_ok

print_action "[Coping 'include/$HEADERFILE' into '$INCLUDEDIR']"
cp include/$HEADERFILE $INCLUDEDIR
check_status "Fail Coping '$HEADERFILE' into '$DIR/include'"
print_ok

print_action "[ldconfig]"
ldconfig
check_status "ldconfig fail"
print_ok

test -f $LIBDIR/$LIBNAME
if [ "$?" -ne 0 ]; then
    print_action "[Symbolic link: ln -s $LIBNAME $LIBSONAME into $LIBDIR]"
    ln -s $LIBDIR/$LIBSONAME $LIBDIR/$LIBNAME
    check_status "Fail making symbolic link"
    print_ok
fi

grep -e "hosts:" /etc/nsswitch.conf | grep llmnr > /dev/null
if [ "$?" -ne 0 ]; then
    print_action "[Setting nsswitch.conf]"
    sed -i '/^hosts:/ s/$/ llmnr/' /etc/nsswitch.conf
    check_status "Fail adding 'llmnr' entry into /etc/nsswitch.conf"
    print_ok
fi 

make cleanall
echo "All done."
exit 0

