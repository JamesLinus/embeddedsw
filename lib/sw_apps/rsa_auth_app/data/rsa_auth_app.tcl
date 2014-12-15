proc swapp_get_name {} {
    return "RSA Authentication App";
}

proc swapp_get_description {} {
    return "Used to RSA authenticate a user application";
}

proc get_stdout {} {
    set os [get_os]
    if { $os == "" } {
        error "No Operating System specified in the Board Support Package.";
    }
    set stdout [get_property CONFIG.STDOUT $os];
    return $stdout;
}

proc check_stdout_hw {} {
	set slaves [get_property SLAVES [get_cells [get_sw_processor]]]
	foreach slave $slaves {
		set slave_type [get_property IP_NAME [get_cells $slave]];
		# Check for MDM-Uart peripheral. The MDM would be listed as a peripheral
		# only if it has a UART interface. So no further check is required
		if { $slave_type == "ps7_uart" || $slave_type == "axi_uartlite" ||
			 $slave_type == "axi_uart16550" || $slave_type == "iomodule" ||
			 $slave_type == "mdm" } {
			return;
		}
	}

	error "This application requires a Uart IP in the hardware."
}

proc check_stdout_sw {} {
    set stdout [get_stdout];
    if { $stdout == "none" } {
        error "The STDOUT parameter is not set on the OS. This app requires stdout to be set."
    }
}

proc swapp_is_supported_hw {} {

    # check processor type
    set proc_instance [get_sw_processor];
    set hw_processor [get_property HW_INSTANCE $proc_instance]

    set proc_type [get_property IP_NAME [get_cells $hw_processor]];
    
    if { $proc_type != "ps7_cortexa9" } {
                error "This application is supported only for CortexA9 processors.";
    }

    # check for uart peripheral
    check_stdout_hw;

    return 1;
}

proc swapp_is_supported_sw {} {
    # check for stdout being set
    check_stdout_sw;

	# make sure xilrsa is available
    set librarylist [get_libs -filter "NAME==xilrsa"];

	if { [llength $librarylist] == 0 } {
        	error "This application requires xilrsa library in the Board Support Package.";
    	}
    
    return 1;
}

# depending on the type of os (standalone|xilkernel), choose
# the correct source files
proc swapp_generate {} {

}

proc swapp_get_linker_constraints {} {
    return "";
}
