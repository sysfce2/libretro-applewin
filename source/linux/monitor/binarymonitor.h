class BinaryMonitor
{
public:
  BinaryMonitor();
  ~BinaryMonitor();

  void process();

private:
#pragma pack(push)
#pragma pack(1)
  struct Command
  {
    uint8_t stx;
    uint8_t version;
    uint32_t length;
    uint32_t request;
    uint8_t type;

    void reset();
  };
#pragma pack(pop)

  Command myCommand;
  std::vector<uint8_t> myPayload;

  int myServerSocket;
  int myConnectionSocket;

  ssize_t readData(char * dest, size_t len);

  bool readCommand();
  bool readPayload();
};
