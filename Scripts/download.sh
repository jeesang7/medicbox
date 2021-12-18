if [[ "$OSTYPE" == "darwin"* ]]; then
    cp ../Projects/B-L4S5I-IOT01A/Applications/Cloud/aws_demos/STM32CubeIDE/PostBuild/SBSFU_B-L4S5I-IOT01_aws_demos.bin /Volumes/DIS_L4S5VI
else
    echo "Failed to download. This script is only supported by OSX."
fi

