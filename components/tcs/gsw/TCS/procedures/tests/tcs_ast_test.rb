require 'cosmos'
require 'cosmos/script'
require "tcs_lib.rb"

##
## This script tests the cFS component in an automated scenario.
## Currently this includes: 
##   Hardware failure
##   Hardware status reporting fault
##


##
## Hardware failure
##
TCS_TEST_LOOP_COUNT.times do |n|
    # Prepare
    tcs_prepare_ast()

    # Disable sim and confirm device error counts increase
    dev_cmd_cnt = tlm("TCS TCS_HK_TLM DEVICE_COUNT")
    dev_cmd_err_cnt = tlm("TCS TCS_HK_TLM DEVICE_ERR_COUNT")
    tcs_sim_disable()
    check("TCS TCS_HK_TLM DEVICE_COUNT == #{dev_cmd_cnt}")
    check("TCS TCS_HK_TLM DEVICE_ERR_COUNT >= #{dev_cmd_err_cnt}")

    # Enable sim and confirm return to nominal operation
    tcs_sim_enable()
    confirm_tcs_data_loop()
end


##
## Hardware status reporting fault
##
TCS_TEST_LOOP_COUNT.times do |n|
    # Prepare
    tcs_prepare_ast()

    # Add a fault to status in the simulator
    tcs_sim_set_status(255)

    # Confirm that status register and that app disabled itself
    get_tcs_hk()
    check("TCS TCS_HK_TLM DEVICE_STATUS == 255")
    get_tcs_hk()
    check("TCS TCS_HK_TLM DEVICE_ENABLED == 'DISABLED'")
    
    # Clear simulator status fault
    tcs_sim_set_status(0)
end
