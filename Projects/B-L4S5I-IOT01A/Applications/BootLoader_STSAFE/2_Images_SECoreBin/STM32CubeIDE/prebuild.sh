#!/bin/bash -
#prebuild script
echo prebuild.sh : started > $1/output.txt
ecckey=$1"/../Binary/ECCKEY.txt"
echo "$ecckey" >> $1/output.txt
asmfile=$1/se_key.s
# comment this line to force python
# python is used if  executeable not found
current_directory=`pwd`
cd $1/../../../../../../Middlewares/ST/STM32_Secure_Engine/Utilities/KeysAndImages
basedir=`pwd`
cd $current_directory
SBSFUBootLoader=$1/../..

# test if window executeable useable
prepareimage=$basedir/win/prepareimage/prepareimage.exe
uname  | grep -i -e windows -e mingw > /dev/null 2>&1

if [ $? = 0 ] && [  -e "$prepareimage" ]; then
  echo "prepareimage with windows executeable" >> $1/output.txt
  echo "prepareimage with windows executeable"
  cmd=""
else
  # add venv
  echo "run python3 on venv"
  python3 -m venv venv
  source venv/bin/activate
  pip3 install -r $basedir/requirements.txt
  # line for python
  echo "prepareimage with python script" >> $1/output.txt
  echo "prepareimage with python script"
  prepareimage=$basedir/prepareimage.py
  cmd=python3
fi

echo "$cmd $prepareimage" >> $1/output.txt
crypto_h=$1/../Inc/se_crypto_config.h

#clean
if [ -e "$1/crypto.txt" ]; then
  rm $1"/crypto.txt"
fi

if [ -e "$asmfile" ]; then
  rm $asmfile
fi

if [ -e "$1"/postbuild.sh ]; then
  rm $1"/postbuild.sh"
fi

#get crypto name
command="$cmd $prepareimage conf "$crypto_h""
echo $command
crypto=`$command`
echo $crypto > $1"/crypto.txt"
echo "$crypto selected">> $1"/output.txt"
echo $crypto selected
ret=$?
if [ $ret = "0" ]; then
    if [ $crypto == "SECBOOT_X509_ECDSA_WITHOUT_ENCRYPT_SHA256" ]; then
    #ECCDSA_WITHOUT_ENCRYPT_SHA256
        oemkey="$1/../../STSAFE_Provisioning/Binary/STSAFE_PAIRING_keys.key"
        command=$cmd" "$prepareimage" trans -a GNU -k "$oemkey" -f SE_ReadKey_Pairing -s .SE_Key_Data -e 1 -v V7M"
        echo $command
        $command > $asmfile
        ret=$?
    fi
fi
if [ $ret = 0 ]; then
#no error proceed with next steps
  #Patch KMS embedded keys with Blob Keys
  kms_blob_ecckey=$SBSFUBootLoader"/2_Images_KMS_Blob/Binary/ECCKEY.txt"
  command=$cmd" "$prepareimage" inject -k "$kms_blob_ecckey" -f "$1"/../Inc/kms_platf_objects_config.h.pattern -p @ECDSA_BLOB_KEY@ "$1"/kms_platf_objects_config.h.tmp"
  echo $command
  $command
  ret=$?
  if [ $ret = 0 ]; then
    kms_blob_oemkey=$SBSFUBootLoader"/2_Images_KMS_Blob/Binary/OEM_KEY_COMPANY1_key_AES_CBC.key"
    command=$cmd" "$prepareimage" inject -k "$kms_blob_oemkey" -f "$1"/kms_platf_objects_config.h.tmp -p @AES_BLOB_KEY@ "$1"/kms_platf_objects_config.h"
    echo $command
    $command
    ret=$?
    rm $1"/kms_platf_objects_config.h.tmp"
  fi

fi

if [ $ret = 0 ]; then
#no error recopy post build script
    uname  | grep -i -e windows -e mingw > /dev/null 2>&1
    if [ $? = 0 ]; then
        echo "recopy postbuild.sh script with "$crypto".sh script"
        command="cat "$1"/"$crypto".sh"
        $command > "$1"/"postbuild.sh"
        ret=$?
    else
        echo "create symbolic link postbuild.sh to "$crypto".sh"
        command="ln -s ./"$crypto".sh "$1"/postbuild.sh"
        $command
        ret=$?
    fi
fi

if [ $ret != 0 ]; then
#an error
echo $command : failed >> $1/output.txt
echo $command : failed
read -n 1 -s
exit 1
fi
exit 0

