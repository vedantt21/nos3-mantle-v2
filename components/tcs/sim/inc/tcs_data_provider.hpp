#ifndef NOS3_TCSDATAPROVIDER_HPP
#define NOS3_TCSDATAPROVIDER_HPP

#include <boost/property_tree/xml_parser.hpp>
#include <ItcLogger/Logger.hpp>
#include <tcs_data_point.hpp>
#include <sim_i_data_provider.hpp>

namespace Nos3
{
    class TcsDataProvider : public SimIDataProvider
    {
    public:
        /* Constructors */
        TcsDataProvider(const boost::property_tree::ptree& config);

        /* Accessors */
        boost::shared_ptr<SimIDataPoint> get_data_point(void) const;

    private:
        /* Disallow these */
        ~TcsDataProvider(void) {};
        TcsDataProvider& operator=(const TcsDataProvider&) {return *this;};

        mutable double _request_count;
    };
}

#endif
