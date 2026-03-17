require 'cosmos'
require 'cosmos/script'
require "tcs_lib.rb"

##
## This script tests the cFS component device functionality.
## Currently this includes: 
##   Enable / disable, control hardware communications
##   Configuration, reconfigure tcs instrument register
##


##
## Enable / disable, control hardware communications
##
TCS_TEST_LOOP_COUNT.times do |n|
    # Get to known state
    safe_tcs()

    # Manually command heater enable when communications are disabled
    cmd_cnt = tlm("TCS TCS_HK_TLM CMD_COUNT")
    cmd_err_cnt = tlm("TCS TCS_HK_TLM CMD_ERR_COUNT")
    cmd("TCS HEATER_ENABLE")
    get_tcs_hk()
    check("TCS TCS_HK_TLM CMD_COUNT == #{cmd_cnt}")
    check("TCS TCS_HK_TLM CMD_ERR_COUNT == #{cmd_err_cnt+1}")

    # Manually command to disable when already disabled
    cmd_cnt = tlm("TCS TCS_HK_TLM CMD_COUNT")
    cmd_err_cnt = tlm("TCS TCS_HK_TLM CMD_ERR_COUNT")
    cmd("TCS HEATER_DISABLE")
    get_tcs_hk()
    check("TCS TCS_HK_TLM CMD_COUNT == #{cmd_cnt}")
    check("TCS TCS_HK_TLM CMD_ERR_COUNT == #{cmd_err_cnt+1}")

    # Enable TCS communications
    enable_tcs()
    confirm_tcs_data()

    # Command the heater on after TCS communications are enabled
    enable_heater()
    confirm_tcs_data(nil, "ON", "MANUAL")
    sleep(1.1)
    confirm_tcs_data(21, "ON", "MANUAL")

    # Manually command heater enable when the heater is already on
    cmd_cnt = tlm("TCS TCS_HK_TLM CMD_COUNT")
    cmd_err_cnt = tlm("TCS TCS_HK_TLM CMD_ERR_COUNT")
    cmd("TCS HEATER_ENABLE")
    get_tcs_hk()
    check("TCS TCS_HK_TLM CMD_COUNT == #{cmd_cnt+1}")
    check("TCS TCS_HK_TLM CMD_ERR_COUNT == #{cmd_err_cnt}")
    sleep(1.1)
    confirm_tcs_data(22, "ON", "MANUAL")

    # Disable
    disable_tcs()
end


##
##   Configuration, reconfigure tcs instrument register
##
TCS_TEST_LOOP_COUNT.times do |n|
    # Get to known state
    safe_tcs()

    # Confirm configuration command denied if disabled
    cmd_cnt = tlm("TCS TCS_HK_TLM CMD_COUNT")
    cmd_err_cnt = tlm("TCS TCS_HK_TLM CMD_ERR_COUNT")
    cmd("TCS TCS_CONFIG_CC with DEVICE_CONFIG 10")
    get_tcs_hk()
    check("TCS TCS_HK_TLM CMD_COUNT == #{cmd_cnt}")
    check("TCS TCS_HK_TLM CMD_ERR_COUNT == #{cmd_err_cnt+1}")
    
    # Enable
    enable_tcs()

    # Set configuration
    tcs_cmd("TCS TCS_CONFIG_CC with DEVICE_CONFIG #{n+1}")
    check("TCS TCS_HK_TLM DEVICE_CONFIG == #{n+1}")
end
