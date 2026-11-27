for f in sw-description test_uart update.sh; do
 
		echo $f;
	done | cpio -ov -H crc > uart_debug_v1.0.1.swu
