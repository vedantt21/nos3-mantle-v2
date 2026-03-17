#include <tcs_hardware_model.hpp>

namespace Nos3
{
    REGISTER_HARDWARE_MODEL(TcsHardwareModel,"TCS");

    extern ItcLogger::Logger *sim_logger;

    TcsHardwareModel::TcsHardwareModel(const boost::property_tree::ptree& config) : SimIHardwareModel(config), 
    _tcs_dp(nullptr), _enabled(TCS_SIM_SUCCESS), _count(0), _config(0), _status(0)
    {
        reset_thermal_state();

        /* Get the NOS engine connection string */
        std::string connection_string = config.get("common.nos-connection-string", "tcp://127.0.0.1:12001"); 
        sim_logger->info("TcsHardwareModel::TcsHardwareModel:  NOS Engine connection string: %s.", connection_string.c_str());

        /* Keep the standard data-provider lifecycle even though thermal state is device-owned. */
        std::string dp_name = config.get("simulator.hardware-model.data-provider.type", "TCS_PROVIDER");
        _tcs_dp = SimDataProviderFactory::Instance().Create(dp_name, config);
        sim_logger->info("TcsHardwareModel::TcsHardwareModel:  Data provider %s created.", dp_name.c_str());

        /* Get on a protocol bus */
        /* Note: Initialized defaults in case value not found in config file */
        std::string bus_name = "usart_29";
        int node_port = 29;
        if (config.get_child_optional("simulator.hardware-model.connections")) 
        {
            /* Loop through the connections for hardware model */
            BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, config.get_child("simulator.hardware-model.connections"))
            {
                /* v.second is the child tree (v.first is the name of the child) */
                if (v.second.get("type", "").compare("usart") == 0)
                {
                    /* Configuration found */
                    bus_name = v.second.get("bus-name", bus_name);
                    node_port = v.second.get("node-port", node_port);
                    break;
                }
            }
        }
        _uart_connection.reset(new NosEngine::Uart::Uart(_hub, config.get("simulator.name", "tcs_sim"), connection_string, bus_name));
        _uart_connection->open(node_port);
        sim_logger->info("TcsHardwareModel::TcsHardwareModel:  Now on UART bus name %s, port %d.", bus_name.c_str(), node_port);
    
        /* Configure protocol callback */
        _uart_connection->set_read_callback(std::bind(&TcsHardwareModel::uart_read_callback, this, std::placeholders::_1, std::placeholders::_2));

        /* Get on the command bus*/
        std::string time_bus_name = "command";
        if (config.get_child_optional("hardware-model.connections")) 
        {
            /* Loop through the connections for the hardware model */
            BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, config.get_child("hardware-model.connections"))
            {
                /* v.first is the name of the child */
                /* v.second is the child tree */
                if (v.second.get("type", "").compare("time") == 0) // 
                {
                    time_bus_name = v.second.get("bus-name", "command");
                    /* Found it... don't need to go through any more items*/
                    break; 
                }
            }
        }
        _time_bus.reset(new NosEngine::Client::Bus(_hub, connection_string, time_bus_name));
        sim_logger->info("TcsHardwareModel::TcsHardwareModel:  Now on time bus named %s.", time_bus_name.c_str());

        /* Construction complete */
        sim_logger->info("TcsHardwareModel::TcsHardwareModel:  Construction complete.");
    }


    TcsHardwareModel::~TcsHardwareModel(void)
    {        
        /* Close the protocol bus */
        _uart_connection->close();

        /* Clean up the data provider */
        delete _tcs_dp;
        _tcs_dp = nullptr;

        /* The bus will clean up the time node */
    }


    /* Automagically set up by the base class to be called */
    void TcsHardwareModel::command_callback(NosEngine::Common::Message msg)
    {
        /* Get the data out of the message */
        NosEngine::Common::DataBufferOverlay dbf(const_cast<NosEngine::Utility::Buffer&>(msg.buffer));
        sim_logger->info("TcsHardwareModel::command_callback:  Received command: %s.", dbf.data);

        /* Do something with the data */
        std::string command = dbf.data;
        std::string response = "TcsHardwareModel::command_callback:  INVALID COMMAND! (Try HELP)";
        boost::to_upper(command);
        if (command.compare("HELP") == 0) 
        {
            response = "TcsHardwareModel::command_callback: Valid commands are HELP, ENABLE, DISABLE, STATUS=X, MODE=AUTO, MODE=MANUAL, HEATER=ON, HEATER=OFF, or STOP";
        }
        else if (command.compare(0,6,"ENABLE") == 0) 
        {
            _enabled = TCS_SIM_SUCCESS;
            response = "TcsHardwareModel::command_callback:  Enabled\n";
        }
        else if (command.compare(0,7,"DISABLE") == 0) 
        {
            _enabled = TCS_SIM_ERROR;
            _count = 0;
            _config = 0;
            _status = 0;
            reset_thermal_state();
            response = "TcsHardwareModel::command_callback:  Disabled";
        }
        else if (command.substr(0,7).compare("STATUS=") == 0)
        {
            try
            {
                _status = std::stod(command.substr(7));
                response = "TcsHardwareModel::command_callback:  Status set";
            }
            catch (...)
            {
                response = "TcsHardwareModel::command_callback:  Status invalid";
            }            
        }
        else if (command.compare("MODE=AUTO") == 0)
        {
            _control_mode = TCS_CONTROL_MODE_AUTO;
            response = "TcsHardwareModel::command_callback:  Control mode set to AUTO";
        }
        else if (command.compare("MODE=MANUAL") == 0)
        {
            _control_mode = TCS_CONTROL_MODE_MANUAL;
            response = "TcsHardwareModel::command_callback:  Control mode set to MANUAL";
        }
        else if (command.compare("HEATER=ON") == 0)
        {
            if (_control_mode == TCS_CONTROL_MODE_MANUAL)
            {
                _heater_state = TCS_HEATER_STATE_ON;
                response = "TcsHardwareModel::command_callback:  Heater set to ON";
            }
            else
            {
                response = "TcsHardwareModel::command_callback:  Heater command rejected in AUTO mode";
            }
        }
        else if (command.compare("HEATER=OFF") == 0)
        {
            if (_control_mode == TCS_CONTROL_MODE_MANUAL)
            {
                _heater_state = TCS_HEATER_STATE_OFF;
                response = "TcsHardwareModel::command_callback:  Heater set to OFF";
            }
            else
            {
                response = "TcsHardwareModel::command_callback:  Heater command rejected in AUTO mode";
            }
        }
        else if (command.compare(0,4,"STOP") == 0) 
        {
            _keep_running = false;
            response = "TcsHardwareModel::command_callback:  Stopping";
        }
        /* TODO: Add anything additional commands here */

        /* Send a reply */
        sim_logger->info("TcsHardwareModel::command_callback:  Sending reply: %s", response.c_str());
        _command_node->send_reply_message_async(msg, response.size(), response.c_str());
    }


    void TcsHardwareModel::reset_thermal_state(void)
    {
        _ambient_temperature_c = TCS_AMBIENT_TEMPERATURE_C;
        _current_temperature_c = TCS_INITIAL_TEMPERATURE_C;
        _lower_threshold_c     = TCS_LOWER_THRESHOLD_C;
        _upper_threshold_c     = TCS_UPPER_THRESHOLD_C;
        _heater_state          = TCS_HEATER_STATE_OFF;
        _control_mode          = TCS_CONTROL_MODE_AUTO;
    }


    void TcsHardwareModel::update_thermal_state(void)
    {
        if (_control_mode == TCS_CONTROL_MODE_AUTO)
        {
            if (_current_temperature_c <= _lower_threshold_c)
            {
                _heater_state = TCS_HEATER_STATE_ON;
            }
            else if (_current_temperature_c >= _upper_threshold_c)
            {
                _heater_state = TCS_HEATER_STATE_OFF;
            }
        }

        if (_heater_state == TCS_HEATER_STATE_ON)
        {
            _current_temperature_c++;
        }
        else
        {
            _current_temperature_c--;
        }

        if (_current_temperature_c < _ambient_temperature_c)
        {
            _current_temperature_c = _ambient_temperature_c;
        }
        else if (_current_temperature_c > _upper_threshold_c)
        {
            _current_temperature_c = _upper_threshold_c;
        }
    }


    /* Custom function to prepare the Tcs HK telemetry */
    void TcsHardwareModel::create_tcs_hk(std::vector<uint8_t>& out_data)
    {
        /* Prepare data size */
        out_data.resize(16, 0x00);

        /* Streaming data header - 0xDEAD */
        out_data[0] = 0xDE;
        out_data[1] = 0xAD;
        
        /* Sequence count */
        out_data[2] = (_count >> 24) & 0x000000FF; 
        out_data[3] = (_count >> 16) & 0x000000FF; 
        out_data[4] = (_count >>  8) & 0x000000FF; 
        out_data[5] =  _count & 0x000000FF;
        
        /* Configuration */
        out_data[6] = (_config >> 24) & 0x000000FF; 
        out_data[7] = (_config >> 16) & 0x000000FF; 
        out_data[8] = (_config >>  8) & 0x000000FF; 
        out_data[9] =  _config & 0x000000FF;

        /* Device Status */
        out_data[10] = (_status >> 24) & 0x000000FF; 
        out_data[11] = (_status >> 16) & 0x000000FF; 
        out_data[12] = (_status >>  8) & 0x000000FF; 
        out_data[13] =  _status & 0x000000FF;

        /* Streaming data trailer - 0xBEEF */
        out_data[14] = 0xBE;
        out_data[15] = 0xEF;
    }


    /* Custom function to prepare the Tcs Data */
    void TcsHardwareModel::create_tcs_data(std::vector<uint8_t>& out_data)
    {
        update_thermal_state();
        std::uint16_t current_temperature = static_cast<std::uint16_t>(_current_temperature_c);
        std::uint16_t lower_threshold     = static_cast<std::uint16_t>(_lower_threshold_c);
        std::uint16_t upper_threshold     = static_cast<std::uint16_t>(_upper_threshold_c);
        std::uint16_t ambient_temperature = static_cast<std::uint16_t>(_ambient_temperature_c);

        /* Prepare data size */
        out_data.resize(18, 0x00);

        /* Streaming data header - 0xDEAD */
        out_data[0] = 0xDE;
        out_data[1] = 0xAD;
        
        /* Sequence count */
        out_data[2] = (_count >> 24) & 0x000000FF; 
        out_data[3] = (_count >> 16) & 0x000000FF; 
        out_data[4] = (_count >>  8) & 0x000000FF; 
        out_data[5] =  _count & 0x000000FF;
        
        /* Thermal payload is transmitted big-endian on the device UART link. */
        out_data[6]  = (current_temperature >> 8) & 0x00FF;
        out_data[7]  = current_temperature & 0x00FF;
        out_data[8]  = (lower_threshold >> 8) & 0x00FF;
        out_data[9]  = lower_threshold & 0x00FF;
        out_data[10] = (upper_threshold >> 8) & 0x00FF;
        out_data[11] = upper_threshold & 0x00FF;
        out_data[12] = _heater_state;
        out_data[13] = _control_mode;
        out_data[14] = (ambient_temperature >> 8) & 0x00FF;
        out_data[15] = ambient_temperature & 0x00FF;

        sim_logger->debug("TcsHardwareModel::create_tcs_data: current=%dC lower=%dC upper=%dC heater=%u mode=%u ambient=%dC.",
            _current_temperature_c, _lower_threshold_c, _upper_threshold_c, _heater_state, _control_mode, _ambient_temperature_c);

        /* Streaming data trailer - 0xBEEF */
        out_data[16] = 0xBE;
        out_data[17] = 0xEF;
    }


    /* Protocol callback */
    void TcsHardwareModel::uart_read_callback(const uint8_t *buf, size_t len)
    {
        std::vector<uint8_t> out_data; 
        std::uint8_t valid = TCS_SIM_SUCCESS;
        
        /* Retrieve data and log in man readable format */
        std::vector<uint8_t> in_data(buf, buf + len);
        sim_logger->debug("TcsHardwareModel::uart_read_callback:  REQUEST %s",
            SimIHardwareModel::uint8_vector_to_hex_string(in_data).c_str());

        /* Check simulator is enabled */
        if (_enabled != TCS_SIM_SUCCESS)
        {
            sim_logger->debug("TcsHardwareModel::uart_read_callback:  Tcs sim disabled!");
            valid = TCS_SIM_ERROR;
        }
        else
        {
            /* Check if message is incorrect size */
            if (in_data.size() != 9)
            {
                sim_logger->debug("TcsHardwareModel::uart_read_callback:  Invalid command size of %ld received!", in_data.size());
                valid = TCS_SIM_ERROR;
            }
            else
            {
                /* Check header - 0xDEAD */
                if ((in_data[0] != 0xDE) || (in_data[1] !=0xAD))
                {
                    sim_logger->debug("TcsHardwareModel::uart_read_callback:  Header incorrect!");
                    valid = TCS_SIM_ERROR;
                }
                else
                {
                    /* Check trailer - 0xBEEF */
                    if ((in_data[7] != 0xBE) || (in_data[8] !=0xEF))
                    {
                        sim_logger->debug("TcsHardwareModel::uart_read_callback:  Trailer incorrect!");
                        valid = TCS_SIM_ERROR;
                    }
                    else
                    {
                        /* Increment count as valid command format received */
                        _count++;
                    }
                }
            }

            if (valid == TCS_SIM_SUCCESS)
            {   
                /* Process command */
                switch (in_data[2])
                {
                    case 0:
                        /* NOOP */
                        sim_logger->debug("TcsHardwareModel::uart_read_callback:  NOOP command received!");
                        break;

                case 1:
                        /* Request HK */
                        sim_logger->debug("TcsHardwareModel::uart_read_callback:  Send HK command received!");
                        create_tcs_hk(out_data);
                        break;

                    case 2:
                        /* Request data */
                        sim_logger->debug("TcsHardwareModel::uart_read_callback:  Send data command received!");
                        create_tcs_data(out_data);
                        break;

                    case 3:
                        /* Configuration */
                        sim_logger->debug("TcsHardwareModel::uart_read_callback:  Configuration command received!");
                        _config  = in_data[3] << 24;
                        _config |= in_data[4] << 16;
                        _config |= in_data[5] << 8;
                        _config |= in_data[6];
                        break;
                    
                    default:
                        /* Unused command code */
                        valid = TCS_SIM_ERROR;
                        sim_logger->debug("TcsHardwareModel::uart_read_callback:  Unused command %d received!", in_data[2]);
                        break;
                }
            }
        }

        /* Echo command since format valid */
        if (valid == TCS_SIM_SUCCESS)
        {
            _uart_connection->write(&in_data[0], in_data.size());

            /* Send response if existing */
            if (out_data.size() > 0)
            {
                sim_logger->debug("TcsHardwareModel::uart_read_callback:  REPLY %s",
                    SimIHardwareModel::uint8_vector_to_hex_string(out_data).c_str());
                _uart_connection->write(&out_data[0], out_data.size());
            }
        }
    }
}
