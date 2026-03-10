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

    # Manually command to disable when already disabled
    cmd_cnt = tlm("TCS TCS_HK_TLM CMD_COUNT")
    cmd_err_cnt = tlm("TCS TCS_HK_TLM CMD_ERR_COUNT")
    cmd("TCS TCS_DISABLE_CC")
    get_tcs_hk()
    check("TCS TCS_HK_TLM CMD_COUNT == #{cmd_cnt}")
    check("TCS TCS_HK_TLM CMD_ERR_COUNT == #{cmd_err_cnt+1}")

    # Enable
    enable_tcs()

    # Confirm device counters increment without errors
    confirm_tcs_data_loop()

    # Manually command to enable when already enabled
    cmd_cnt = tlm("TCS TCS_HK_TLM CMD_COUNT")
    cmd_err_cnt = tlm("TCS TCS_HK_TLM CMD_ERR_COUNT")
    cmd("TCS TCS_ENABLE_CC")
    get_tcs_hk()
    check("TCS TCS_HK_TLM CMD_COUNT == #{cmd_cnt}")
    check("TCS TCS_HK_TLM CMD_ERR_COUNT == #{cmd_err_cnt+1}")

    # Reconfirm data remains as expected
    confirm_tcs_data_loop()

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
