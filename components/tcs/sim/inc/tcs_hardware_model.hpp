#ifndef NOS3_TCSHARDWAREMODEL_HPP
#define NOS3_TCSHARDWAREMODEL_HPP

/*
** Implementation Notes:
**   The hardware model now owns a two-node thermal simulation instead of a
**   simple ambient-temperature curve.  The two nodes are:
**     skin:     external spacecraft shell affected by Sun, albedo, Earth IR,
**               radiation to space, and conduction to the internal node
**     internal: equipment/heater node reported as CURRENT_TEMPERATURE
**
**   The cFS app commands this model over UART with the opcodes in
**   tcs_device.h.  Backdoor simulator commands still exist for OpenC3 test
**   setup, but flight behavior should be recreated through the UART command
**   path whenever possible.
*/

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

/*
** Reset temperatures and AUTO thresholds.  These match the values published in
** TCS_TLM.txt and tcs.xtce, so changing them requires updating ground checks.
*/
#define TCS_INITIAL_SKIN_TEMPERATURE_K     293.0
#define TCS_INITIAL_INTERNAL_TEMPERATURE_K 293.0
#define TCS_LOWER_THRESHOLD_K              273.0
#define TCS_UPPER_THRESHOLD_K              283.0


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
        /*
        ** create_tcs_hk() and create_tcs_data() serialize simulator state into
        ** the exact byte order consumed by components/tcs/fsw/shared/tcs_device.c.
        */
        void create_tcs_hk(std::vector<uint8_t>& out_data); 
        void create_tcs_data(std::vector<uint8_t>& out_data); 
        /* reset_thermal_state() restores the initial temperatures/mode. */
        void reset_thermal_state(void);
        /* time_tick_callback() advances the thermal model once per NOS3 tick. */
        void time_tick_callback(void);
        /* update_thermal_state() performs AUTO hysteresis plus RK4 integration. */
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
        double                                              _skin_temperature_k;     /* External/shell node, K */
        double                                              _internal_temperature_k; /* Equipment/heater node, K */
        double                                              _lower_threshold_k;      /* AUTO heater ON threshold, K */
        double                                              _upper_threshold_k;      /* AUTO heater OFF threshold, K */
        std::uint8_t                                        _heater_state;           /* 0 OFF, 1 ON */
        std::uint8_t                                        _control_mode;           /* 0 MANUAL, 1 AUTO */
        double                                              _simulation_time_seconds; /* Monotonic thermal model time */
    };
}

#endif
