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
    cmd("TCS HEATER_DISABLE")
    get_tcs_hk()
    check("TCS TCS_HK_TLM CMD_COUNT == #{cmd_cnt}")
    check("TCS TCS_HK_TLM CMD_ERR_COUNT == #{cmd_err_cnt+1}")

    # Enable
    enable_tcs()

    # Confirm default thermal telemetry
    confirm_tcs_data(20, "OFF", "AUTO")

    # Switch to manual mode and heat to the upper threshold
    tcs_sim_mode_manual()
    confirm_tcs_data(20, "OFF", "MANUAL")
    tcs_sim_heater_on()
    (21..50).each do |temp|
        confirm_tcs_data(temp, "ON", "MANUAL")
    end

    # Manually command to enable when already enabled
    cmd_cnt = tlm("TCS TCS_HK_TLM CMD_COUNT")
    cmd_err_cnt = tlm("TCS TCS_HK_TLM CMD_ERR_COUNT")
    cmd("TCS HEATER_ENABLE")
    get_tcs_hk()
    check("TCS TCS_HK_TLM CMD_COUNT == #{cmd_cnt}")
    check("TCS TCS_HK_TLM CMD_ERR_COUNT == #{cmd_err_cnt+1}")

    # Cool back to ambient in manual mode
    tcs_sim_heater_off()
    49.downto(20) do |temp|
        confirm_tcs_data(temp, "OFF", "MANUAL")
    end

    # Heat again, then confirm auto mode enforces the threshold behavior
    tcs_sim_heater_on()
    (21..49).each do |temp|
        confirm_tcs_data(temp, "ON", "MANUAL")
    end
    tcs_sim_mode_auto()
    confirm_tcs_data(50, "ON", "AUTO")
    confirm_tcs_data(49, "OFF", "AUTO")

    # Auto mode should ignore manual heater commands
    48.downto(20) do |temp|
        confirm_tcs_data(temp, "OFF", "AUTO")
    end
    tcs_sim_heater_on()
    confirm_tcs_data(20, "OFF", "AUTO")

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
