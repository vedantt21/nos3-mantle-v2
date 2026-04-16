#include <tcs_hardware_model.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <math.h>

/*
** Implementation Notes:
**   This file replaced the earlier single ambient-temperature sine curve with a
**   small physics-based orbital thermal model.  To recreate the behavior:
**   1. Build constant thermal/orbital parameters in build_thermal_model().
**   2. On every NOS3 time tick, choose heater ON/OFF if AUTO mode is active.
**   3. Integrate skin and internal temperatures with rk4_step().
**   4. Serialize the same internal/skin temperatures and thresholds into the
**      UART packet layout consumed by tcs_device.c.
**
**   The anonymous namespace keeps the math private to this simulator while
**   leaving the public TcsHardwareModel interface unchanged for NOS3.
*/
namespace
{
    /*
    ** Orbital/environment constants used by the new two-node thermal model.
    ** Units are encoded in the names so later changes can be made without
    ** reverse-engineering whether a value is seconds, meters, Kelvin, or watts.
    */
    constexpr double TCS_PI = 3.14159265358979323846;
    /* Limit each RK4 integration sub-step to one second for stable hysteresis. */
    constexpr double TCS_INTEGRATION_STEP_SECONDS = 1.0;
    /* 165-minute orbit, matching the user-provided model assumptions. */
    constexpr double TCS_ORBIT_PERIOD_SECONDS = 165.0 * 60.0;
    /* Beta angle tilts the Sun vector out of the orbital plane. */
    constexpr double TCS_BETA_DEG = 20.0;
    /* Smooth eclipse ingress/egress over this many seconds. */
    constexpr double TCS_PENUMBRA_SECONDS = 180.0;
    constexpr double TCS_STEFAN_BOLTZMANN = 5.670374419e-8;
    constexpr double TCS_EARTH_GM_M3_PER_S2 = 3.986004418e14;
    constexpr double TCS_EARTH_RADIUS_M = 6378.137e3;
    constexpr double TCS_SOLAR_CONSTANT_W_PER_M2 = 1361.6;
    constexpr double TCS_EARTH_ALBEDO_FACTOR = 0.30;
    constexpr double TCS_EARTH_EFFECTIVE_TEMPERATURE_K = 255.0;
    constexpr double TCS_CUBESAT_LENGTH_M = 0.3405;
    constexpr double TCS_CUBESAT_WIDTH_M = 0.100;
    constexpr double TCS_CUBESAT_HEIGHT_M = 0.100;
    constexpr double TCS_ALPHA_PAINT = 0.15;
    constexpr double TCS_EPSILON_PAINT = 0.91;
    constexpr double TCS_PV_FRACTION = 0.80;
    constexpr double TCS_ALPHA_PV = 0.92;
    constexpr double TCS_EPSILON_PV = 0.85;
    constexpr double TCS_TOTAL_MASS_KG = 4.0;
    constexpr double TCS_SKIN_MASS_FRACTION = 0.30;
    constexpr double TCS_SPECIFIC_HEAT_J_PER_KG_K = 896.0;
    constexpr double TCS_CONTACT_H_W_PER_M2_K = 400.0;
    constexpr double TCS_CONTACT_AREA_M2 = 0.02;
    constexpr double TCS_HEATER_POWER_W = 25.0;
    constexpr double TCS_INTERNAL_POWER_SUNLIGHT_W = 8.0;
    constexpr double TCS_INTERNAL_POWER_ECLIPSE_W = 4.0;

    /*
    ** Attitude mode controls how much projected area sees the Sun.  NADIR uses
    ** the face normals below; the other modes are retained to reproduce hot,
    ** cold, or tumbling sensitivity cases without changing the integrator.
    */
    enum TcsAttitudeMode
    {
        TCS_ATTITUDE_NADIR,
        TCS_ATTITUDE_WORST_HOT,
        TCS_ATTITUDE_WORST_COLD,
        TCS_ATTITUDE_TUMBLING_MEAN
    };

    constexpr TcsAttitudeMode TCS_ATTITUDE_MODE = TCS_ATTITUDE_NADIR;

    /*
    ** State vector integrated by RK4.  Skin is the radiating external node;
    ** internal is the component/heater node reported to flight software.
    */
    struct ThermalState
    {
        double skin_temperature_k;
        double internal_temperature_k;
    };

    struct ThermalDerivatives
    {
        double d_skin_k_per_s;
        double d_internal_k_per_s;
    };

    struct ThermalModelParameters
    {
        double orbit_period_s;
        double beta_rad;
        double penumbra_s;
        double sigma;
        double earth_gm_m3_per_s2;
        double earth_radius_m;
        double solar_constant_w_per_m2;
        double earth_albedo_factor;
        double earth_effective_temperature_k;
        double total_external_area_m2;
        std::array<std::array<double, 3>, 6> face_normals;
        std::array<double, 6> face_areas_m2;
        double absorptivity;
        double emissivity;
        double skin_heat_capacity_j_per_k;
        double internal_heat_capacity_j_per_k;
        double thermal_conductance_w_per_k;
        double orbital_altitude_m;
        double orbital_semi_major_axis_m;
        double beta_critical_rad;
        double eclipse_fraction;
        double max_earth_view_factor;
    };

    double deg_to_rad(double degrees)
    {
        /* Keep trigonometry inputs in radians while leaving configuration in degrees. */
        return degrees * TCS_PI / 180.0;
    }

    double dot_product(const std::array<double, 3> &lhs, const std::array<double, 3> &rhs)
    {
        /* Face-normal projection helper used for both Sun and Earth view terms. */
        return lhs[0] * rhs[0] + lhs[1] * rhs[1] + lhs[2] * rhs[2];
    }

    void orbit_from_period(double orbit_period_s, double earth_gm_m3_per_s2, double earth_radius_m,
                           double &altitude_m, double &semi_major_axis_m)
    {
        /*
        ** Convert orbit period into semi-major axis using Kepler's third law.
        ** Altitude is then semi-major axis minus Earth radius, which is needed
        ** for eclipse geometry and Earth view factor.
        */
        semi_major_axis_m = std::pow(earth_gm_m3_per_s2 * std::pow(orbit_period_s / (2.0 * TCS_PI), 2.0), 1.0 / 3.0);
        altitude_m = semi_major_axis_m - earth_radius_m;
    }

    double eclipse_fraction_beta(double beta_rad, double earth_radius_m, double altitude_m, double &beta_critical_rad)
    {
        /*
        ** Estimate the fraction of each orbit spent in eclipse for the selected
        ** beta angle.  Above the critical beta angle the orbit never enters
        ** eclipse, so sunlight stays at 1.0 for the whole period.
        */
        beta_rad = std::fabs(beta_rad);
        beta_critical_rad = std::asin(earth_radius_m / (earth_radius_m + altitude_m));

        if (beta_rad >= beta_critical_rad)
        {
            return 0.0;
        }

        double eclipse_argument =
            std::sqrt(altitude_m * altitude_m + 2.0 * earth_radius_m * altitude_m) /
            ((earth_radius_m + altitude_m) * std::cos(beta_rad));
        eclipse_argument = std::max(-1.0, std::min(1.0, eclipse_argument));
        return std::acos(eclipse_argument) / TCS_PI;
    }

    ThermalModelParameters build_thermal_model(void)
    {
        ThermalModelParameters params{};

        /*
        ** Blend paint and solar-cell optical properties by surface coverage.
        ** This gives one effective absorptivity/emissivity pair for the simple
        ** model instead of assigning material properties per face.
        */
        const double alpha = TCS_PV_FRACTION * TCS_ALPHA_PV + (1.0 - TCS_PV_FRACTION) * TCS_ALPHA_PAINT;
        const double emissivity = TCS_PV_FRACTION * TCS_EPSILON_PV + (1.0 - TCS_PV_FRACTION) * TCS_EPSILON_PAINT;
        /*
        ** Split spacecraft mass into thermal capacitance for the skin node and
        ** internal node.  Both use aluminum specific heat as the simplifying
        ** assumption for heat capacity.
        */
        const double skin_mass_kg = TCS_SKIN_MASS_FRACTION * TCS_TOTAL_MASS_KG;
        const double internal_mass_kg = TCS_TOTAL_MASS_KG - skin_mass_kg;

        params.orbit_period_s = TCS_ORBIT_PERIOD_SECONDS;
        params.beta_rad = deg_to_rad(TCS_BETA_DEG);
        params.penumbra_s = TCS_PENUMBRA_SECONDS;
        params.sigma = TCS_STEFAN_BOLTZMANN;
        params.earth_gm_m3_per_s2 = TCS_EARTH_GM_M3_PER_S2;
        params.earth_radius_m = TCS_EARTH_RADIUS_M;
        params.solar_constant_w_per_m2 = TCS_SOLAR_CONSTANT_W_PER_M2;
        params.earth_albedo_factor = TCS_EARTH_ALBEDO_FACTOR;
        params.earth_effective_temperature_k = TCS_EARTH_EFFECTIVE_TEMPERATURE_K;
        params.total_external_area_m2 =
            2.0 * (TCS_CUBESAT_WIDTH_M * TCS_CUBESAT_HEIGHT_M +
                   TCS_CUBESAT_LENGTH_M * TCS_CUBESAT_HEIGHT_M +
                   TCS_CUBESAT_LENGTH_M * TCS_CUBESAT_WIDTH_M);
        /*
        ** Six axis-aligned face normals are enough for the simple CubeSat
        ** geometry.  The face order must match face_areas_m2 below.
        */
        params.face_normals = {{
            {{ 1.0,  0.0,  0.0}},
            {{-1.0,  0.0,  0.0}},
            {{ 0.0,  1.0,  0.0}},
            {{ 0.0, -1.0,  0.0}},
            {{ 0.0,  0.0,  1.0}},
            {{ 0.0,  0.0, -1.0}}
        }};
        params.face_areas_m2 = {{
            TCS_CUBESAT_WIDTH_M * TCS_CUBESAT_HEIGHT_M,
            TCS_CUBESAT_WIDTH_M * TCS_CUBESAT_HEIGHT_M,
            TCS_CUBESAT_LENGTH_M * TCS_CUBESAT_HEIGHT_M,
            TCS_CUBESAT_LENGTH_M * TCS_CUBESAT_HEIGHT_M,
            TCS_CUBESAT_LENGTH_M * TCS_CUBESAT_WIDTH_M,
            TCS_CUBESAT_LENGTH_M * TCS_CUBESAT_WIDTH_M
        }};
        /* Thermal conductance is h*A between the internal and skin nodes. */
        params.absorptivity = alpha;
        params.emissivity = emissivity;
        params.skin_heat_capacity_j_per_k = skin_mass_kg * TCS_SPECIFIC_HEAT_J_PER_KG_K;
        params.internal_heat_capacity_j_per_k = internal_mass_kg * TCS_SPECIFIC_HEAT_J_PER_KG_K;
        params.thermal_conductance_w_per_k = TCS_CONTACT_H_W_PER_M2_K * TCS_CONTACT_AREA_M2;

        orbit_from_period(params.orbit_period_s, params.earth_gm_m3_per_s2, params.earth_radius_m,
                          params.orbital_altitude_m, params.orbital_semi_major_axis_m);
        /*
        ** Precompute orbit-derived values once.  They are constant for the run,
        ** so every derivative evaluation can reuse them cheaply.
        */
        params.eclipse_fraction =
            eclipse_fraction_beta(params.beta_rad, params.earth_radius_m, params.orbital_altitude_m,
                                  params.beta_critical_rad);
        params.max_earth_view_factor =
            std::pow(params.earth_radius_m / (params.earth_radius_m + params.orbital_altitude_m), 2.0);

        return params;
    }

    const ThermalModelParameters &thermal_model(void)
    {
        /* Lazily build immutable parameters once and reuse them for all ticks. */
        static const ThermalModelParameters params = build_thermal_model();
        return params;
    }

    double sun_factor_penumbra(double tau_s, const ThermalModelParameters &params)
    {
        /*
        ** Return sunlight multiplier in [0, 1].  The old model used a sine wave
        ** ambient term; this model dims direct solar and albedo heat during
        ** eclipse while Earth IR remains present.
        */
        if (params.eclipse_fraction <= 0.0)
        {
            return 1.0;
        }

        const double eclipse_duration_s = params.eclipse_fraction * params.orbit_period_s;
        const double eclipse_start_s = 0.5 * params.orbit_period_s - 0.5 * eclipse_duration_s;
        const double eclipse_end_s = 0.5 * params.orbit_period_s + 0.5 * eclipse_duration_s;
        const double transition_s = std::min(params.penumbra_s, 0.5 * eclipse_duration_s);

        if (transition_s <= 0.0)
        {
            return (tau_s >= eclipse_start_s && tau_s <= eclipse_end_s) ? 0.0 : 1.0;
        }

        if ((tau_s < eclipse_start_s - transition_s / 2.0) || (tau_s > eclipse_end_s + transition_s / 2.0))
        {
            return 1.0;
        }

        if ((tau_s >= eclipse_start_s + transition_s / 2.0) && (tau_s <= eclipse_end_s - transition_s / 2.0))
        {
            return 0.0;
        }

        if ((tau_s >= eclipse_start_s - transition_s / 2.0) && (tau_s < eclipse_start_s + transition_s / 2.0))
        {
            /* Ingress: cosine ramp from full Sun down to eclipse. */
            const double x = (tau_s - (eclipse_start_s - transition_s / 2.0)) / transition_s;
            return 0.5 * (1.0 + std::cos(TCS_PI * x));
        }

        /* Egress: cosine ramp from eclipse back to full Sun. */
        const double x = (tau_s - (eclipse_end_s - transition_s / 2.0)) / transition_s;
        return 0.5 * (1.0 - std::cos(TCS_PI * x));
    }

    std::array<double, 3> sun_vector_to_sun(double tau_s, const ThermalModelParameters &params)
    {
        /*
        ** Sun vector in the spacecraft/orbit frame.  The orbital phase moves
        ** the vector around the spacecraft once per orbit and beta adds a fixed
        ** out-of-plane component.
        */
        const double orbital_phase_rad = 2.0 * TCS_PI * tau_s / params.orbit_period_s;
        return {{
            std::cos(params.beta_rad) * std::sin(orbital_phase_rad),
            std::sin(params.beta_rad),
            -std::cos(params.beta_rad) * std::cos(orbital_phase_rad)
        }};
    }

    double projected_area_to_sun(const ThermalModelParameters &params, const std::array<double, 3> &sun_vector)
    {
        /*
        ** Convert Sun direction into illuminated area.  For NADIR each face
        ** contributes area*cos(theta) only when the face points toward the Sun.
        */
        switch (TCS_ATTITUDE_MODE)
        {
            case TCS_ATTITUDE_NADIR:
            {
                double projected_area_m2 = 0.0;
                for (std::size_t face = 0; face < params.face_normals.size(); ++face)
                {
                    projected_area_m2 +=
                        params.face_areas_m2[face] * std::max(0.0, dot_product(params.face_normals[face], sun_vector));
                }
                return projected_area_m2;
            }

            case TCS_ATTITUDE_WORST_HOT:
                return std::max({TCS_CUBESAT_LENGTH_M * TCS_CUBESAT_WIDTH_M,
                                 TCS_CUBESAT_LENGTH_M * TCS_CUBESAT_HEIGHT_M,
                                 TCS_CUBESAT_WIDTH_M * TCS_CUBESAT_HEIGHT_M});

            case TCS_ATTITUDE_WORST_COLD:
                return std::min({TCS_CUBESAT_LENGTH_M * TCS_CUBESAT_WIDTH_M,
                                 TCS_CUBESAT_LENGTH_M * TCS_CUBESAT_HEIGHT_M,
                                 TCS_CUBESAT_WIDTH_M * TCS_CUBESAT_HEIGHT_M});

            case TCS_ATTITUDE_TUMBLING_MEAN:
                return params.total_external_area_m2 / 4.0;
        }

        return 0.0;
    }

    ThermalDerivatives thermal_derivatives(double simulation_time_s, const ThermalState &state, bool heater_enabled)
    {
        const ThermalModelParameters &params = thermal_model();
        /*
        ** Fold absolute simulation time into one orbit so the environment is
        ** periodic while _simulation_time_seconds can keep increasing forever.
        */
        double tau_s = std::fmod(simulation_time_s, params.orbit_period_s);
        if (tau_s < 0.0)
        {
            tau_s += params.orbit_period_s;
        }

        const double sun_factor = sun_factor_penumbra(tau_s, params);
        const std::array<double, 3> sun_vector = sun_vector_to_sun(tau_s, params);
        const double projected_solar_area_m2 = projected_area_to_sun(params, sun_vector);
        /*
        ** Direct solar heat absorbed by the current projected area.  Eclipse or
        ** penumbra scales this term through sun_factor.
        */
        const double solar_heat_w =
            params.absorptivity * params.solar_constant_w_per_m2 * projected_solar_area_m2 * sun_factor;

        const std::array<double, 3> earth_vector = {{0.0, 0.0, 1.0}};
        double albedo_heat_w = 0.0;
        double earth_ir_heat_w = 0.0;
        /*
        ** Earth terms are accumulated face-by-face.  Albedo is sunlight-driven
        ** and goes away in eclipse; Earth IR is thermal radiation from Earth and
        ** remains active regardless of sun_factor.
        */
        for (std::size_t face = 0; face < params.face_normals.size(); ++face)
        {
            const double cos_earth = std::max(0.0, dot_product(params.face_normals[face], earth_vector));
            const double face_view_factor = params.max_earth_view_factor * cos_earth;

            albedo_heat_w +=
                (params.absorptivity * params.solar_constant_w_per_m2 * params.earth_albedo_factor * face_view_factor) *
                params.face_areas_m2[face];
            earth_ir_heat_w +=
                (params.sigma * params.emissivity * std::pow(params.earth_effective_temperature_k, 4.0) *
                 face_view_factor) *
                params.face_areas_m2[face];
        }
        albedo_heat_w *= sun_factor;

        const double internal_heat_w =
            TCS_INTERNAL_POWER_ECLIPSE_W +
            (TCS_INTERNAL_POWER_SUNLIGHT_W - TCS_INTERNAL_POWER_ECLIPSE_W) * sun_factor;
        const double heater_heat_w = heater_enabled ? TCS_HEATER_POWER_W : 0.0;
        /*
        ** Skin loses heat by Stefan-Boltzmann radiation to space.  Internal and
        ** skin exchange heat through conductance; positive conduction means the
        ** internal node is warmer and sends heat out to the skin.
        */
        const double radiated_heat_w =
            params.emissivity * params.sigma * params.total_external_area_m2 *
            std::pow(state.skin_temperature_k, 4.0);
        const double conduction_heat_w =
            params.thermal_conductance_w_per_k * (state.internal_temperature_k - state.skin_temperature_k);

        ThermalDerivatives derivatives{};
        /*
        ** Energy balance:
        **   skin:     solar + albedo + Earth IR + conduction - radiation
        **   internal: internal electronics + heater - conduction
        ** Divide watts by heat capacity (J/K) to get K/s.
        */
        derivatives.d_skin_k_per_s =
            (solar_heat_w + albedo_heat_w + earth_ir_heat_w + conduction_heat_w - radiated_heat_w) /
            params.skin_heat_capacity_j_per_k;
        derivatives.d_internal_k_per_s =
            (internal_heat_w + heater_heat_w - conduction_heat_w) / params.internal_heat_capacity_j_per_k;
        return derivatives;
    }

    ThermalState rk4_step(double simulation_time_s, double step_s, const ThermalState &state, bool heater_enabled)
    {
        /*
        ** Fourth-order Runge-Kutta integration samples derivatives at the start,
        ** midpoint, midpoint, and end of the step.  This is why the model can
        ** use one-second steps without the drift/noise of a simple Euler update.
        */
        const ThermalDerivatives k1 = thermal_derivatives(simulation_time_s, state, heater_enabled);

        const ThermalState state_k2 = {
            state.skin_temperature_k + 0.5 * step_s * k1.d_skin_k_per_s,
            state.internal_temperature_k + 0.5 * step_s * k1.d_internal_k_per_s
        };
        const ThermalDerivatives k2 = thermal_derivatives(simulation_time_s + step_s / 2.0, state_k2, heater_enabled);

        const ThermalState state_k3 = {
            state.skin_temperature_k + 0.5 * step_s * k2.d_skin_k_per_s,
            state.internal_temperature_k + 0.5 * step_s * k2.d_internal_k_per_s
        };
        const ThermalDerivatives k3 = thermal_derivatives(simulation_time_s + step_s / 2.0, state_k3, heater_enabled);

        const ThermalState state_k4 = {
            state.skin_temperature_k + step_s * k3.d_skin_k_per_s,
            state.internal_temperature_k + step_s * k3.d_internal_k_per_s
        };
        const ThermalDerivatives k4 = thermal_derivatives(simulation_time_s + step_s, state_k4, heater_enabled);

        return {
            state.skin_temperature_k +
                (step_s / 6.0) *
                    (k1.d_skin_k_per_s + 2.0 * k2.d_skin_k_per_s + 2.0 * k3.d_skin_k_per_s + k4.d_skin_k_per_s),
            state.internal_temperature_k +
                (step_s / 6.0) * (k1.d_internal_k_per_s + 2.0 * k2.d_internal_k_per_s +
                                  2.0 * k3.d_internal_k_per_s + k4.d_internal_k_per_s)
        };
    }

    bool thermal_state_is_valid(const ThermalState &state)
    {
        /*
        ** Guardrails catch numerical blow-ups or impossible temperatures before
        ** they enter telemetry.  The caller restores the previous state if this
        ** check fails.
        */
        return std::isfinite(state.skin_temperature_k) && std::isfinite(state.internal_temperature_k) &&
               state.skin_temperature_k > 0.0 && state.internal_temperature_k > 0.0 &&
               state.skin_temperature_k < 1000.0 && state.internal_temperature_k < 1000.0;
    }

    float kelvin_to_telemetry_kelvin(double temperature_k)
    {
        /* Telemetry is already Kelvin; this cast documents the double-to-float boundary. */
        return static_cast<float>(temperature_k);
    }

    std::uint32_t float_to_u32_bits(float value)
    {
        std::uint32_t bits = 0;
        static_assert(sizeof(float) == sizeof(bits), "Expected 32-bit float telemetry fields.");
        /*
        ** Preserve IEEE-754 bits exactly for the UART packet.  The byte order is
        ** applied later in create_tcs_data() when the uint32 is shifted out.
        */
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
}

namespace Nos3
{
    REGISTER_HARDWARE_MODEL(TcsHardwareModel,"TCS");

    extern ItcLogger::Logger *sim_logger;

    TcsHardwareModel::TcsHardwareModel(const boost::property_tree::ptree& config) : SimIHardwareModel(config), 
    _tcs_dp(nullptr), _enabled(TCS_SIM_SUCCESS), _count(0), _config(0), _status(0), _simulation_time_seconds(0.0)
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
        _time_bus->add_time_tick_callback(std::bind(&TcsHardwareModel::time_tick_callback, this));
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
        /*
        ** Reset all thermal state in Kelvin.  Disabling the simulator calls
        ** this so the next ENABLE starts from a known, reproducible condition.
        */
        _simulation_time_seconds = 0.0;
        _skin_temperature_k = TCS_INITIAL_SKIN_TEMPERATURE_K;
        _internal_temperature_k = TCS_INITIAL_INTERNAL_TEMPERATURE_K;
        _lower_threshold_k = TCS_LOWER_THRESHOLD_K;
        _upper_threshold_k = TCS_UPPER_THRESHOLD_K;
        _heater_state = TCS_HEATER_STATE_OFF;
        _control_mode = TCS_CONTROL_MODE_AUTO;
    }


    void TcsHardwareModel::time_tick_callback(void)
    {
        /*
        ** Time ticks are the only place thermal state advances.  UART requests
        ** only serialize the current state, which keeps telemetry reads from
        ** changing the physics.
        */
        if (_enabled != TCS_SIM_SUCCESS)
        {
            return;
        }

        const double dt = static_cast<double>(_sim_microseconds_per_tick) / 1000000.0;
        if (dt > 0.0)
        {
            update_thermal_state(dt);
        }
    }


    void TcsHardwareModel::update_thermal_state(double dt)
    {
        /*
        ** Save the incoming state so invalid numerical results can be rolled
        ** back without resetting counters, configuration, or mode.
        */
        const double previous_internal_temperature_k = _internal_temperature_k;
        const double previous_skin_temperature_k = _skin_temperature_k;
        double remaining_seconds = dt;

        /* Integrate the user-provided two-node orbital model in 1-second chunks for stable heater hysteresis. */
        while (remaining_seconds > 0.0)
        {
            if (_control_mode == TCS_CONTROL_MODE_AUTO)
            {
                /*
                ** AUTO hysteresis:
                **   below lower threshold -> heater ON
                **   above upper threshold -> heater OFF
                **   inside the band       -> keep previous heater state
                */
                if (_internal_temperature_k < _lower_threshold_k)
                {
                    _heater_state = TCS_HEATER_STATE_ON;
                }
                else if (_internal_temperature_k > _upper_threshold_k)
                {
                    _heater_state = TCS_HEATER_STATE_OFF;
                }
            }

            const double step_seconds = std::min(remaining_seconds, TCS_INTEGRATION_STEP_SECONDS);
            const ThermalState current_state = {_skin_temperature_k, _internal_temperature_k};
            /*
            ** Use the current heater state for this sub-step.  If AUTO changes
            ** state at the start of a later one-second chunk, that new state is
            ** used for the next RK4 call.
            */
            const ThermalState next_state =
                rk4_step(_simulation_time_seconds, step_seconds, current_state,
                         _heater_state == TCS_HEATER_STATE_ON);

            if (!thermal_state_is_valid(next_state))
            {
                sim_logger->warning("TcsHardwareModel::update_thermal_state: invalid temperature state computed at t=%.3fs. Retaining previous thermal state.", _simulation_time_seconds);
                _internal_temperature_k = previous_internal_temperature_k;
                _skin_temperature_k = previous_skin_temperature_k;
                return;
            }

            _simulation_time_seconds += step_seconds;
            _skin_temperature_k = next_state.skin_temperature_k;
            _internal_temperature_k = next_state.internal_temperature_k;
            remaining_seconds -= step_seconds;
        }

        if (!std::isfinite(_internal_temperature_k) || !std::isfinite(_skin_temperature_k))
        {
            sim_logger->warning("TcsHardwareModel::update_thermal_state: non-finite temperature computed at t=%.3fs. Retaining previous thermal state.", _simulation_time_seconds);
            _internal_temperature_k = previous_internal_temperature_k;
            _skin_temperature_k = previous_skin_temperature_k;
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
        /*
        ** Convert model state into the UART telemetry packet.  Thresholds are
        ** rounded to integer Kelvin because the cFS packet fields are int16_t;
        ** temperatures remain IEEE-754 floats so OpenC3 can display decimals.
        */
        float current_temperature_k = kelvin_to_telemetry_kelvin(_internal_temperature_k);
        float skin_temperature_k = kelvin_to_telemetry_kelvin(_skin_temperature_k);
        std::uint16_t lower_threshold = static_cast<std::uint16_t>(std::lround(_lower_threshold_k));
        std::uint16_t upper_threshold = static_cast<std::uint16_t>(std::lround(_upper_threshold_k));
        std::uint32_t current_temperature = float_to_u32_bits(current_temperature_k);
        std::uint32_t skin_temperature = float_to_u32_bits(skin_temperature_k);

        /* Prepare data size */
        out_data.resize(22, 0x00);

        /* Streaming data header - 0xDEAD */
        out_data[0] = 0xDE;
        out_data[1] = 0xAD;
        
        /* Sequence count */
        out_data[2] = (_count >> 24) & 0x000000FF; 
        out_data[3] = (_count >> 16) & 0x000000FF; 
        out_data[4] = (_count >>  8) & 0x000000FF; 
        out_data[5] =  _count & 0x000000FF;
        
        /*
        ** Thermal payload is transmitted big-endian on the device UART link.
        ** This byte map must match TCS_RequestData() in tcs_device.c:
        **   06-09 internal/current temperature K
        **   10-11 lower threshold K
        **   12-13 upper threshold K
        **   14 heater state
        **   15 control mode
        **   16-19 skin temperature K
        */
        out_data[6]  = (current_temperature >> 24) & 0x000000FF;
        out_data[7]  = (current_temperature >> 16) & 0x000000FF;
        out_data[8]  = (current_temperature >> 8) & 0x000000FF;
        out_data[9]  = current_temperature & 0x000000FF;
        out_data[10] = (lower_threshold >> 8) & 0x00FF;
        out_data[11] = lower_threshold & 0x00FF;
        out_data[12] = (upper_threshold >> 8) & 0x00FF;
        out_data[13] = upper_threshold & 0x00FF;
        out_data[14] = _heater_state;
        out_data[15] = _control_mode;
        out_data[16] = (skin_temperature >> 24) & 0x000000FF;
        out_data[17] = (skin_temperature >> 16) & 0x000000FF;
        out_data[18] = (skin_temperature >> 8) & 0x000000FF;
        out_data[19] = skin_temperature & 0x000000FF;

        sim_logger->debug("TcsHardwareModel::create_tcs_data: internal=%.3fK skin=%.3fK lower=%uK upper=%uK heater=%u mode=%u.",
            static_cast<double>(current_temperature_k), _skin_temperature_k, static_cast<unsigned int>(lower_threshold),
            static_cast<unsigned int>(upper_threshold), _heater_state, _control_mode);

        /* Streaming data trailer - 0xBEEF */
        out_data[20] = 0xBE;
        out_data[21] = 0xEF;
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

                    case 4:
                        /*
                        ** Set control mode from the low payload byte.  The
                        ** cFS app sends a 32-bit payload, but mode only needs
                        ** 0 MANUAL or 1 AUTO, so byte 6 is the value.
                        */
                        if (in_data[6] <= TCS_CONTROL_MODE_AUTO)
                        {
                            _control_mode = in_data[6];
                            sim_logger->debug("TcsHardwareModel::uart_read_callback:  Control mode set to %u.", _control_mode);
                        }
                        else
                        {
                            valid = TCS_SIM_ERROR;
                            sim_logger->debug("TcsHardwareModel::uart_read_callback:  Invalid control mode %u received!", in_data[6]);
                        }
                        break;

                    case 5:
                        /*
                        ** Set manual heater state from the low payload byte.
                        ** The flight app forces MANUAL mode before sending
                        ** this opcode, while simulator backdoor commands also
                        ** reject heater changes unless already MANUAL.
                        */
                        if (in_data[6] <= TCS_HEATER_STATE_ON)
                        {
                            _heater_state = in_data[6];
                            sim_logger->debug("TcsHardwareModel::uart_read_callback:  Heater state set to %u.", _heater_state);
                        }
                        else
                        {
                            valid = TCS_SIM_ERROR;
                            sim_logger->debug("TcsHardwareModel::uart_read_callback:  Invalid heater state %u received!", in_data[6]);
                        }
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
