#include "StdAfx.h"

#include "frontends/sdl/monitor/breakpoints.h"
#include "frontends/sdl/monitor/exception.h"
#include "frontends/sdl/monitor/commands.h"

#include "Debugger/Debug.h"
#include "CPU.h"
#include "Log.h"


namespace
{

  BreakpointSource_t getSource(const binarymonitor::CheckpointSet_t & checkpointSet)
  {
    switch (checkpointSet.operation)
    {
    case 4:
      return BP_SRC_REG_PC;
    case 1:
      return BP_SRC_MEM_READ_ONLY;
    case 2:
      return BP_SRC_MEM_WRITE_ONLY;
    case 1 + 2:
      return BP_SRC_MEM_RW;
    default:
      throw binarymonitor::BinaryException({binarymonitor::e_MON_RESPONSE_CHECKPOINT_INFO,
                                            binarymonitor::e_MON_ERR_INVALID_PARAMETER});
    }
  }

}

namespace binarymonitor
{

  int getBreakpointOperation(const Breakpoint_t & bp)
  {
    switch (bp.eSource)
    {
    case BP_SRC_REG_PC:
      return 4;
    case BP_SRC_MEM_READ_ONLY:
      return 1;
    case BP_SRC_MEM_WRITE_ONLY:
      return 2;
    case BP_SRC_MEM_RW:
      return 1 + 2;
    default:
      throw BinaryException({e_MON_RESPONSE_CHECKPOINT_INFO, e_MON_ERR_INVALID_PARAMETER});
    }
  }

  int addBreakpoint(const CheckpointSet_t & checkpointSet)
  {
    if (!checkpointSet.stop)
    {
      // unsupported as of now
      throw BinaryException({e_MON_RESPONSE_CHECKPOINT_INFO, e_MON_ERR_INVALID_PARAMETER});
    }

    const BreakpointSource_t source = getSource(checkpointSet);

#if 0
    for (size_t i = 0; i < MAX_BREAKPOINTS; ++i)
    {
      Breakpoint_t & bp = g_aBreakpoints[i];
      if (bp.eSource == source &&
          bp.eOperator == BP_OP_EQUAL &&
          bp.nAddress == checkpointSet.startAddress &&
          bp.nLength == checkpointSet.endAddress - checkpointSet.startAddress + 1)
      {
        bp.bSet = true;
        bp.bEnabled = checkpointSet.enabled;
        bp.bTemp = checkpointSet.temporary;
        return i;
      }
    }
#endif

    size_t i = 0;
    while ((i < MAX_BREAKPOINTS) && g_aBreakpoints[i].bSet)
    {
      ++i;
    }

    if (i >= MAX_BREAKPOINTS)
    {
      throw BinaryException({e_MON_RESPONSE_CHECKPOINT_INFO, e_MON_ERR_INVALID_PARAMETER});
    }

    Breakpoint_t & bp = g_aBreakpoints[i];
    bp.nAddress = checkpointSet.startAddress;
    bp.nLength = checkpointSet.endAddress - checkpointSet.startAddress + 1;
    bp.eSource = source;
    bp.bSet = true;
    bp.eOperator = BP_OP_EQUAL;
    bp.bEnabled = checkpointSet.enabled;
    bp.bTemp = checkpointSet.temporary;
    ++g_nBreakpoints;

    return i;
  }

  void logBreakpoint(const Breakpoint_t & bp)
  {
    if (bp.bSet)
    {
      LogOutput("BREAKPOINT [%04X-%04X] src: %2d op: %d enabled: %d temp: %d\n", bp.nAddress, bp.nAddress + bp.nLength - 1, bp.eSource, bp.eOperator, bp.bEnabled, bp.bTemp);
    }
  }

  void removeAllBReakpoints()
  {
    for (size_t i = 0; i < MAX_BREAKPOINTS; ++i)
    {
      Breakpoint_t & bp = g_aBreakpoints[i];
      deleteBreakpoint(bp);
    }
    g_nBreakpoints = 0;
  }

  void deleteBreakpoint(Breakpoint_t & bp)
  {
    if (bp.bSet)
    {
      --g_nBreakpoints;
    }
    bp.bEnabled = false;
    bp.bSet = false;
    bp.nLength = 0;
  }

}
