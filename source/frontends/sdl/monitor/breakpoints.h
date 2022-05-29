struct Breakpoint_t;

namespace binarymonitor
{

  struct CheckpointSet_t;

  int getBreakpointOperation(const Breakpoint_t & bp);
  int addBreakpoint(const CheckpointSet_t & checkpointSet);
  void logBreakpoint(const Breakpoint_t & bp);
  void removeAllBReakpoints();
  void deleteBreakpoint(Breakpoint_t & bp);
  void clearAllBreakpointHits();

}
