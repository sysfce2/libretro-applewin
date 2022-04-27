struct CheckpointSet_t;
struct Breakpoint_t;

int getOperation(const Breakpoint_t & bp);
int addBreakpoint(const CheckpointSet_t & checkpointSet);
bool isPCBreakpointHit(const Breakpoint_t & bp);
int findPCBreakpointHit();
void logBreakpoint(const Breakpoint_t & bp);
