#include <tcs_data_provider.hpp>

namespace Nos3
{
    REGISTER_DATA_PROVIDER(TcsDataProvider,"TCS_PROVIDER");

    extern ItcLogger::Logger *sim_logger;

    TcsDataProvider::TcsDataProvider(const boost::property_tree::ptree& config) : SimIDataProvider(config)
    {
        sim_logger->trace("TcsDataProvider::TcsDataProvider:  Constructor executed");
        _request_count = 0;
    }

    boost::shared_ptr<SimIDataPoint> TcsDataProvider::get_data_point(void) const
    {
        sim_logger->trace("TcsDataProvider::get_data_point:  Executed");

        /* Prepare the provider data */
        _request_count++;

        /* Request a data point */
        SimIDataPoint *dp = new TcsDataPoint(_request_count);

        /* Return the data point */
        return boost::shared_ptr<SimIDataPoint>(dp);
    }
}
