// ======================================================================
// \title  TcsSim.hpp
// \author jstar
// \brief  hpp file for TcsSim component implementation class
// ======================================================================

#ifndef Components_TcsSim_HPP
#define Components_TcsSim_HPP

#include "tcs_src/TcsSimComponentAc.hpp"
#include "tcs_src/TcsSim_ActiveStateEnumAc.hpp"

extern "C"{
#include "tcs_device.h"
#include "libuart.h"
}
  

#define TCS_DEVICE_DISABLED 0
#define TCS_DEVICE_ENABLED  1

typedef struct
{
    uint8_t                     CommandErrorCount;
    uint8_t                     CommandCount;
    uint8_t                     DeviceErrorCount;
    uint8_t                     DeviceCount;
    uint8_t                     DeviceEnabled;
} __attribute__((packed)) TCS_Hk_tlm_t;
#define TCS_HK_TLM_LNGTH sizeof(TCS_Hk_tlm_t)


namespace Components {

  class TcsSim :
    public TcsSimComponentBase
  {

    public:

    uart_info_t TcsUart; 
    TCS_Device_HK_tlm_t TcsHK; 
    TCS_Device_Data_tlm_t TcsData;
    int32_t status = OS_SUCCESS;

    TCS_Hk_tlm_t HkTelemetryPkt;

      // ----------------------------------------------------------------------
      // Component construction and destruction
      // ----------------------------------------------------------------------

      //! Construct TcsSim object
      TcsSim(
          const char* const compName //!< The component name
      );

      //! Destroy TcsSim object
      ~TcsSim();

    PRIVATE:

      // ----------------------------------------------------------------------
      // Handler implementations for commands
      // ----------------------------------------------------------------------


      void REQUEST_HOUSEKEEPING_cmdHandler(
        FwOpcodeType opCode, 
        U32 cmdSeq
      ) override;

      void NOOP_cmdHandler(
        FwOpcodeType opCode, 
        U32 cmdSeq
      )override;

       void TCS_SEQ_cmdHandler(
        FwOpcodeType opCode, 
        U32 cmdSeq
      )override;

      void ENABLE_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq
      )override;

      void DISABLE_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq
      )override;

      void RESET_COUNTERS_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq
      )override;

      void CONFIGURE_cmdHandler(
        FwOpcodeType opCode,
        U32 cmdSeq,
        const U32 config
      )override;

      inline TcsSim_ActiveState get_active_state(uint8_t DeviceEnabled);

  };

}

#endif
