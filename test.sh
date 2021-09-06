#echo 1 > /proc/sys/vm/drop_caches
modprobe compbm path="$(pwd)/${4:-test_finnish_512}" format_name="${1:-vmalloc}" compression_name="${2:-dummy}" transformation_name="${3:-pointer_vmalloc}"
rmmod compbm

echo $(dmesg | grep compbm | tail -n1 | sed -e 's/.\+: //')
