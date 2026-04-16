require 'cosmos'
require 'cosmos/script'
require "tcs_lib.rb"

##
## This script tests the cFS component device functionality.
## Currently this includes: 
##   Enable / disable, control hardware communications
##   Configuration, reconfigure tcs instrument register
##
## Implementation notes:
##   This test was expanded with the functioning thermal model.  It now proves:
##     1. Heater commands are rejected before TCS_ENABLE opens UART.
##     2. HEATER_ENABLE puts the model in MANUAL/ON and warms internal temp.
##     3. HEATER_DISABLE keeps TCS enabled while forcing MANUAL/OFF.
##     4. HEATER_AUTO returns control to simulator threshold hysteresis.
##     5. TCS_DISABLE is the app/device shutdown command, not heater disable.
##


##
## Enable / disable, control hardware communications
##
TCS_TEST_LOOP_COUNT.times do |n|
    # Get to known state
    safe_tcs()

    # Manually command heater enable when communications are disabled.
    # Expected recreation result: no command success is counted because the app
    # rejects the request before trying to use an unopened UART handle.
    cmd_cnt = tlm("TCS TCS_HK_TLM CMD_COUNT")
    cmd_err_cnt = tlm("TCS TCS_HK_TLM CMD_ERR_COUNT")
    cmd("TCS HEATER_ENABLE")
    get_tcs_hk()
    check("TCS TCS_HK_TLM CMD_COUNT == #{cmd_cnt}")
    check("TCS TCS_HK_TLM CMD_ERR_COUNT == #{cmd_err_cnt+1}")

    # Manually command heater disable when communications are disabled.
    # This mirrors the enable rejection path and proves the new OFF command uses
    # the same device-enabled guard.
    cmd_cnt = tlm("TCS TCS_HK_TLM CMD_COUNT")
    cmd_err_cnt = tlm("TCS TCS_HK_TLM CMD_ERR_COUNT")
    cmd("TCS HEATER_DISABLE")
    get_tcs_hk()
    check("TCS TCS_HK_TLM CMD_COUNT == #{cmd_cnt}")
    check("TCS TCS_HK_TLM CMD_ERR_COUNT == #{cmd_err_cnt+1}")

    # Automatic heater control command should also be rejected while disabled.
    # AUTO only changes simulator control mode, but it still travels over UART,
    # so it must fail before TCS_ENABLE.
    cmd_cnt = tlm("TCS TCS_HK_TLM CMD_COUNT")
    cmd_err_cnt = tlm("TCS TCS_HK_TLM CMD_ERR_COUNT")
    cmd("TCS HEATER_AUTO")
    get_tcs_hk()
    check("TCS TCS_HK_TLM CMD_COUNT == #{cmd_cnt}")
    check("TCS TCS_HK_TLM CMD_ERR_COUNT == #{cmd_err_cnt+1}")

    # Enable TCS communications, then confirm the reset state is AUTO.
    # reset_thermal_state() initializes _control_mode to AUTO in the simulator.
    enable_tcs()
    confirm_tcs_data(nil, nil, "AUTO")

    # Command the heater on after TCS communications are enabled.
    # The app sends two UART commands: SET_MODE(MANUAL) followed by
    # SET_HEATER(ON).  The temperature should rise measurably after 30.5 s.
    enable_heater()
    confirm_tcs_data(nil, "ON", "MANUAL")
    get_tcs_data()
    current_temperature = tlm("TCS TCS_DATA_TLM CURRENT_TEMPERATURE")
    sleep(30.5)
    get_tcs_data()
    check("TCS TCS_DATA_TLM CURRENT_TEMPERATURE >= #{current_temperature + 0.2}")
    confirm_tcs_data(nil, "ON", "MANUAL")

    # Manually command heater enable when the heater is already on.
    # This verifies idempotence: a repeated MANUAL/ON command should count as a
    # successful command and should not add a command error.
    cmd_cnt = tlm("TCS TCS_HK_TLM CMD_COUNT")
    cmd_err_cnt = tlm("TCS TCS_HK_TLM CMD_ERR_COUNT")
    cmd("TCS HEATER_ENABLE")
    get_tcs_hk()
    check("TCS TCS_HK_TLM CMD_COUNT == #{cmd_cnt+1}")
    check("TCS TCS_HK_TLM CMD_ERR_COUNT == #{cmd_err_cnt}")
    get_tcs_data()
    current_temperature = tlm("TCS TCS_DATA_TLM CURRENT_TEMPERATURE")
    sleep(30.5)
    get_tcs_data()
    check("TCS TCS_DATA_TLM CURRENT_TEMPERATURE >= #{current_temperature + 0.2}")
    confirm_tcs_data(nil, "ON", "MANUAL")

    # Command the heater off and confirm the internal node moves back toward the
    # skin node.  In the two-node model, disabling heat should reduce the
    # internal-skin temperature gap through conduction.
    disable_heater()
    confirm_tcs_data(nil, "OFF", "MANUAL")
    get_tcs_hk()
    check("TCS TCS_HK_TLM DEVICE_ENABLED == 'ENABLED'")
    get_tcs_data()
    current_temperature = tlm("TCS TCS_DATA_TLM CURRENT_TEMPERATURE")
    skin_temperature = tlm("TCS TCS_DATA_TLM SKIN_TEMPERATURE")
    initial_gap = (current_temperature - skin_temperature).abs
    sleep(30.5)
    get_tcs_data()
    updated_temperature = tlm("TCS TCS_DATA_TLM CURRENT_TEMPERATURE")
    updated_skin_temperature = tlm("TCS TCS_DATA_TLM SKIN_TEMPERATURE")
    updated_gap = (updated_temperature - updated_skin_temperature).abs
    if updated_gap >= initial_gap
        # Raise a descriptive failure instead of a bare check so the two gaps
        # needed to debug the thermal behavior are visible in OpenC3 output.
        raise "Current temperature did not move toward skin after heater disable: initial gap=#{initial_gap}, updated gap=#{updated_gap}"
    end
    confirm_tcs_data(nil, "OFF", "MANUAL")

    # Return heater control to AUTO and confirm the control mode changes back.
    # We do not assert an immediate heater state because AUTO waits for the next
    # thermal update to apply threshold hysteresis.
    enable_auto_control()
    confirm_tcs_data(nil, nil, "AUTO")

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
