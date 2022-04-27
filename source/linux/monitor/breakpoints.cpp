#include "StdAfx.h"

#include "linux/monitor/commands.h"

#include "Debugger/Debug.h"
#include "CPU.h"
#include "Log.h"


namespace
{

  BreakpointSource_t getSource(const CheckpointSet_t & checkpointSet)
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
      return NUM_BREAKPOINT_SOURCES;
    }
  }
}

int getOperation(const Breakpoint_t & bp)
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
    return 0;
  }
}

int addBreakpoint(const CheckpointSet_t & checkpointSet)
{
  const BreakpointSource_t source = getSource(checkpointSet);

  // there are some issues with memory breakpoints
  // keep them disabled
  const bool enabled = checkpointSet.enabled && (BP_SRC_REG_PC == source);

  for (size_t i = 0; i < MAX_BREAKPOINTS; ++i)
  {
    Breakpoint_t & bp = g_aBreakpoints[i];
    if (bp.bSet && 
        bp.eSource == source && 
        bp.eOperator == BP_OP_EQUAL && 
        bp.nAddress == checkpointSet.startAddress &&
        bp.nLength == checkpointSet.endAddress - checkpointSet.startAddress + 1)
    {
      bp.bEnabled = enabled;
      bp.bTemp = checkpointSet.temporary;
      return i;
    }
  }

  size_t i = 0;
  while ((i < MAX_BREAKPOINTS) && g_aBreakpoints[i].bSet)
  {
    ++i;
  }

  if (i >= MAX_BREAKPOINTS)
  {
    return -1;
  }

  Breakpoint_t & bp = g_aBreakpoints[i];
  bp.nAddress = checkpointSet.startAddress;
  bp.nLength = checkpointSet.endAddress - checkpointSet.startAddress + 1;
  bp.eSource = source;
  bp.bSet = true;
  bp.eOperator = BP_OP_EQUAL;
  bp.bEnabled = enabled;
  bp.bTemp = checkpointSet.temporary;
  ++g_nBreakpoints;

  return i;
}

bool isPCBreakpointHit(const Breakpoint_t & bp)
{
  if (bp.bSet && bp.eSource == BP_SRC_REG_PC)
  {
  	if ((regs.pc >= bp.nAddress) && (regs.pc < bp.nAddress + bp.nLength))
    {
      return true;
    }
  }
  return false;
}

int findPCBreakpointHit()
{
  for (size_t i = 0; i < MAX_BREAKPOINTS; ++i)
  {
    const Breakpoint_t & bp = g_aBreakpoints[i];
    if (isPCBreakpointHit(bp))
    {
      return i;
    }
  }
  return -1;
}

void logBreakpoint(const Breakpoint_t & bp)
{
  if (bp.bSet)
  {
    LogOutput("BREAKPOINT [%04X-%04X] src: %2d op: %d enabled: %d temp: %d\n", bp.nAddress, bp.nAddress + bp.nLength - 1, bp.eSource, bp.eOperator, bp.bEnabled, bp.bTemp);
  }
}