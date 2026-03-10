#include <ItcLogger/Logger.hpp>
#include <tcs_data_point.hpp>

namespace Nos3
{
    extern ItcLogger::Logger *sim_logger;

    TcsDataPoint::TcsDataPoint(double count) : _not_parsed(false)
    {
        sim_logger->trace("TcsDataPoint::TcsDataPoint:  Defined Constructor executed");

        /* Do calculations based on provided data - also preparing like ADC data to checkout is obvious */
        _tcs_data_is_valid = true;
        _tcs_data[0] = (((count * 1) / 32767.0) - 32768.0);
        _tcs_data[1] = (((count * 2) / 32767.0) - 32768.0);
        _tcs_data[2] = (((count * 3) / 32767.0) - 32768.0);
    }

    TcsDataPoint::TcsDataPoint(int16_t spacecraft, const boost::shared_ptr<Sim42DataPoint> dp) : _dp(*dp), _sc(spacecraft), _not_parsed(true)
    {
        sim_logger->trace("TcsDataPoint::TcsDataPoint:  42 Constructor executed");

        /* Initialize data */
        _tcs_data_is_valid = false;
        _tcs_data[0] = _tcs_data[1] = _tcs_data[2] = 0.0;
    }
    
    void TcsDataPoint::do_parsing(void) const
    {
        try {
            /*
            ** Declare 42 telemetry string prefix
            ** 42 variables defined in `42/Include/42types.h`
            ** 42 data stream defined in `42/Source/IPC/SimWriteToSocket.c`
            */
            std::string key;
            key.append("SC[").append(std::to_string(_sc)).append("].svb"); // SC[N].svb

            /* Parse 42 telemetry */
            std::string values = _dp.get_value_for_key(key);

            std::vector<double> data;
            data.reserve(3);
            parse_double_vector(values, data);

            if (data.size() < 3) {
                _tcs_data_is_valid = false;
            } else {
                _tcs_data[0] = data[0];
                _tcs_data[1] = data[1];
                _tcs_data[2] = data[2];
                /* Mark data as valid */
                _tcs_data_is_valid = true;
            }

            _not_parsed = false;

            /* Debug print */
            sim_logger->trace("TcsDataPoint::TcsDataPoint:  Parsed svb = %f %f %f", _tcs_data[0], _tcs_data[1], _tcs_data[2]);
        } catch (const std::exception &e) {
            sim_logger->error("TcsDataPoint::TcsDataPoint:  Error parsing svb.  Error=%s", e.what());
        }
    }

    /* Used for printing a representation of the data point */
    std::string TcsDataPoint::to_string(void) const
    {
        sim_logger->trace("TcsDataPoint::to_string:  Executed");
        
        std::stringstream ss;

        ss << std::fixed << std::setfill(' ');
        ss << "Tcs Data Point:   Valid: ";
        ss << (_tcs_data_is_valid ? "Valid" : "INVALID");
        ss << std::setprecision(std::numeric_limits<double>::digits10); /* Full double precision */
        ss << " Tcs Data: "
           << _tcs_data[0]
           << " "
           << _tcs_data[1]
           << " "
           << _tcs_data[2];

        return ss.str();
    }
} /* namespace Nos3 */
