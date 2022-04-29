struct CheckpointSet_t;
struct Breakpoint_t;

int getBreakpointOperation(const Breakpoint_t & bp);
int addBreakpoint(const CheckpointSet_t & checkpointSet);
void logBreakpoint(const Breakpoint_t & bp);
void removeAllBReakpoints();
