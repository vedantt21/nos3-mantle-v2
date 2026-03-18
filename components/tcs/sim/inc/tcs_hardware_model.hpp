#ifndef NOS3_TCSHARDWAREMODEL_HPP
#define NOS3_TCSHARDWAREMODEL_HPP

/*
** Includes
*/
#include <map>

#include <boost/tuple/tuple.hpp>
#include <boost/property_tree/ptree.hpp>

#include <Client/Bus.hpp>
#include <Uart/Client/Uart.hpp> /* TODO: Change if your protocol bus is different (e.g. SPI, I2C, etc.) */

#include <sim_i_data_provider.hpp>
#include <sim_i_hardware_model.hpp>


/*
** Defines
*/
#define TCS_SIM_SUCCESS 0
#define TCS_SIM_ERROR   1

#define TCS_HEATER_STATE_OFF 0
#define TCS_HEATER_STATE_ON  1

#define TCS_CONTROL_MODE_MANUAL 0
#define TCS_CONTROL_MODE_AUTO   1

#define TCS_INITIAL_TEMPERATURE_C 20
#define TCS_LOWER_THRESHOLD_C     0
#define TCS_UPPER_THRESHOLD_C     50


/*
** Namespace
*/
namespace Nos3
{
    /* Standard for a hardware model */
    class TcsHardwareModel : public SimIHardwareModel
    {
    public:
        /* Constructor and destructor */
        TcsHardwareModel(const boost::property_tree::ptree& config);
        ~TcsHardwareModel(void);

    private:
        /* Private helper methods */
        void create_tcs_hk(std::vector<uint8_t>& out_data); 
        void create_tcs_data(std::vector<uint8_t>& out_data); 
        void reset_thermal_state(void);
        void time_tick_callback(void);
        void update_thermal_state(double dt);
        void uart_read_callback(const uint8_t *buf, size_t len); /* Handle data the hardware receives from its protocol bus */
        void command_callback(NosEngine::Common::Message msg); /* Handle backdoor commands and time tick to the simulator */

        /* Private data members */
        std::unique_ptr<NosEngine::Uart::Uart>              _uart_connection; /* TODO: Change if your protocol bus is different (e.g. SPI, I2C, etc.) */
        std::unique_ptr<NosEngine::Client::Bus>             _time_bus; /* Standard */
        SimIDataProvider*                                   _tcs_dp; /* Keep standard simulator/provider wiring intact */

        /* Internal state data */
        std::uint8_t                                        _enabled;
        std::uint32_t                                       _count;
        std::uint32_t                                       _config;
        std::uint32_t                                       _status;
        double                                              _ambient_temperature_k;
        double                                              _skin_temperature_k;
        double                                              _internal_temperature_k;
        std::int16_t                                        _lower_threshold_c;
        std::int16_t                                        _upper_threshold_c;
        std::uint8_t                                        _heater_state;
        std::uint8_t                                        _control_mode;
        double                                              _simulation_time_seconds;
    };
}

#endif
