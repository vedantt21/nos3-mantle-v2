# Library for TCS Target
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
    cmd("TCS TCS_REQ_HK")
    wait_check_packet("TCS", "TCS_HK_TLM", 1, TCS_RESPONSE_TIMEOUT)
    sleep(TCS_CMD_SLEEP)
end

def get_tcs_data()
    cmd("TCS TCS_REQ_DATA")
    wait_check_packet("TCS", "TCS_DATA_TLM", 1, TCS_RESPONSE_TIMEOUT)
    sleep(TCS_CMD_SLEEP)
end

def tcs_cmd(*command)
    count = tlm("TCS TCS_HK_TLM CMD_COUNT") + 1

    if (count == 256)
        count = 0
    end

    cmd(*command)
    get_tcs_hk()
    current = tlm("TCS TCS_HK_TLM CMD_COUNT")
    if (current != count)
        # Try again
        cmd(*command)
        get_tcs_hk()
        current = tlm("TCS TCS_HK_TLM CMD_COUNT")
        if (current != count)
            # Third times the charm
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
    tcs_cmd("TCS HEATER_ENABLE")
end

def disable_tcs()
    # Send command
    tcs_cmd("TCS HEATER_DISABLE")
    # Confirm
    check("TCS TCS_HK_TLM DEVICE_ENABLED == 'DISABLED'")
end

def safe_tcs()
    get_tcs_hk()
    state = tlm("TCS TCS_HK_TLM DEVICE_ENABLED")
    if (state != "DISABLED")
        disable_tcs()
    end
end

def confirm_tcs_data(expected_current = nil, expected_heater = nil, expected_mode = nil)
    dev_cmd_cnt = tlm("TCS TCS_HK_TLM DEVICE_COUNT")
    dev_cmd_err_cnt = tlm("TCS TCS_HK_TLM DEVICE_ERR_COUNT")
    
    get_tcs_data()
    check("TCS TCS_DATA_TLM LOWER_THRESHOLD == 0")
    check("TCS TCS_DATA_TLM UPPER_THRESHOLD == 50")
    check("TCS TCS_DATA_TLM AMBIENT_TEMPERATURE == 20")
    check("TCS TCS_DATA_TLM CURRENT_TEMPERATURE >= 20")
    check("TCS TCS_DATA_TLM CURRENT_TEMPERATURE <= 50")

    if (!expected_current.nil?)
        check("TCS TCS_DATA_TLM CURRENT_TEMPERATURE == #{expected_current}")
    end

    if (!expected_heater.nil?)
        check("TCS TCS_DATA_TLM HEATER_STATE == '#{expected_heater}'")
    end

    if (!expected_mode.nil?)
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
