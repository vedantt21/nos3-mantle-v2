# Library for TCS Target
#
# Implementation notes:
# These helpers hide the command/telemetry handshake used by the OpenC3 tests.
# The changed behavior is intentionally documented here because this file is the
# fastest way to recreate the end-to-end scenario:
#   enable_tcs() opens the app UART link.
#   enable_heater() sends HEATER_ENABLE, which forces MANUAL mode and heater ON.
#   disable_heater() sends HEATER_DISABLE, which stays enabled but heater OFF.
#   enable_auto_control() sends HEATER_AUTO, returning hysteresis to the sim.
#   disable_tcs() sends TCS_DISABLE, which turns heater OFF before UART close.
require 'cosmos'
require 'cosmos/script'

#
# Definitions
#
TCS_CMD_SLEEP = 0.25
TCS_RESPONSE_TIMEOUT = 5
TCS_TEST_LOOP_COUNT = 1
TCS_DEVICE_LOOP_COUNT = 5

#
# Functions
#
def get_tcs_hk()
    # Request app housekeeping and wait for one fresh packet before checking it.
    cmd("TCS TCS_REQ_HK")
    wait_check_packet("TCS", "TCS_HK_TLM", 1, TCS_RESPONSE_TIMEOUT)
    sleep(TCS_CMD_SLEEP)
end

def get_tcs_data()
    # Request the TCS data packet carrying internal/skin temperatures and mode.
    cmd("TCS TCS_REQ_DATA")
    wait_check_packet("TCS", "TCS_DATA_TLM", 1, TCS_RESPONSE_TIMEOUT)
    sleep(TCS_CMD_SLEEP)
end

def tcs_cmd(*command)
    # Command count is uint8 in TCS_Hk_tlm_t, so wrap the expected count at 256.
    count = tlm("TCS TCS_HK_TLM CMD_COUNT") + 1

    if (count == 256)
        count = 0
    end

    cmd(*command)
    get_tcs_hk()
    current = tlm("TCS TCS_HK_TLM CMD_COUNT")
    if (current != count)
        # Retry once because command/telemetry can race in the live NOS3 stack.
        cmd(*command)
        get_tcs_hk()
        current = tlm("TCS TCS_HK_TLM CMD_COUNT")
        if (current != count)
            # Final retry preserves the old test behavior while catching drops.
            cmd(*command)
            get_tcs_hk()
            current = tlm("TCS TCS_HK_TLM CMD_COUNT")
        end
    end
    check("TCS TCS_HK_TLM CMD_COUNT >= #{count}")
end

def enable_tcs()
    # Send command
    tcs_cmd("TCS TCS_ENABLE")
    # Confirm
    check("TCS TCS_HK_TLM DEVICE_ENABLED == 'ENABLED'")
end

def enable_heater()
    # Flight side translates this to MANUAL mode then heater ON over UART.
    tcs_cmd("TCS HEATER_ENABLE")
end

def disable_heater()
    # Flight side translates this to MANUAL mode then heater OFF over UART.
    tcs_cmd("TCS HEATER_DISABLE")
end

def enable_auto_control()
    # Return heater decisions to the simulator's lower/upper threshold logic.
    tcs_cmd("TCS HEATER_AUTO")
end

def disable_tcs()
    # Send command
    tcs_cmd("TCS TCS_DISABLE")
    # Confirm
    check("TCS TCS_HK_TLM DEVICE_ENABLED == 'DISABLED'")
end

def safe_tcs()
    # Start tests from a disabled app-side UART so counters and state are known.
    get_tcs_hk()
    state = tlm("TCS TCS_HK_TLM DEVICE_ENABLED")
    if (state != "DISABLED")
        disable_tcs()
    end
end

def confirm_tcs_data(expected_current = nil, expected_heater = nil, expected_mode = nil)
    # Record device counters before the data request so we can prove telemetry
    # collection did not introduce a device error.
    dev_cmd_cnt = tlm("TCS TCS_HK_TLM DEVICE_COUNT")
    dev_cmd_err_cnt = tlm("TCS TCS_HK_TLM DEVICE_ERR_COUNT")
    
    get_tcs_data()
    # These values are Kelvin thresholds from the simulator reset state.
    check("TCS TCS_DATA_TLM LOWER_THRESHOLD == 273")
    check("TCS TCS_DATA_TLM UPPER_THRESHOLD == 283")
    check("TCS TCS_DATA_TLM SKIN_TEMPERATURE >= 150.0")
    check("TCS TCS_DATA_TLM SKIN_TEMPERATURE <= 400.0")
    check("TCS TCS_DATA_TLM CURRENT_TEMPERATURE >= 150.0")
    check("TCS TCS_DATA_TLM CURRENT_TEMPERATURE <= 400.0")

    if (!expected_current.nil?)
        # Optional exact check for tests that capture a known temperature.
        check("TCS TCS_DATA_TLM CURRENT_TEMPERATURE == #{expected_current}")
    end

    if (!expected_heater.nil?)
        # Optional state check: OpenC3 maps 0/1 to OFF/ON through TCS_TLM.txt.
        check("TCS TCS_DATA_TLM HEATER_STATE == '#{expected_heater}'")
    end

    if (!expected_mode.nil?)
        # Optional mode check: MANUAL for commanded heater, AUTO for hysteresis.
        check("TCS TCS_DATA_TLM CONTROL_MODE == '#{expected_mode}'")
    end

    get_tcs_hk()
    check("TCS TCS_HK_TLM DEVICE_COUNT >= #{dev_cmd_cnt}")
    check("TCS TCS_HK_TLM DEVICE_ERR_COUNT == #{dev_cmd_err_cnt}")
end

def confirm_tcs_data_loop()
    TCS_DEVICE_LOOP_COUNT.times do |n|
        confirm_tcs_data()
    end
end

#
# Simulator Functions
#
def tcs_prepare_ast()
    # Get to known state
    safe_tcs()

    # Enable
    enable_tcs()

    # Confirm data
    confirm_tcs_data_loop()
end

def tcs_sim_enable()
    cmd("SIM_CMDBUS_BRIDGE TCS_SIM_ENABLE")
end

def tcs_sim_disable()
    cmd("SIM_CMDBUS_BRIDGE TCS_SIM_DISABLE")
end

def tcs_sim_mode_auto()
    # Backdoor simulator command, useful for setup independent of flight UART.
    cmd("SIM_CMDBUS_BRIDGE TCS_SIM_MODE_AUTO")
end

def tcs_sim_mode_manual()
    cmd("SIM_CMDBUS_BRIDGE TCS_SIM_MODE_MANUAL")
end

def tcs_sim_heater_on()
    cmd("SIM_CMDBUS_BRIDGE TCS_SIM_HEATER_ON")
end

def tcs_sim_heater_off()
    cmd("SIM_CMDBUS_BRIDGE TCS_SIM_HEATER_OFF")
end

def tcs_sim_set_status(status)
    cmd("SIM_CMDBUS_BRIDGE TCS_SIM_SET_STATUS with STATUS #{status}")
end
