class BinaryClient
{
public:
  BinaryClient(const int socket);
  ~BinaryClient();

  bool process();

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

  struct Response
  {
    uint8_t stx;
    uint8_t version;
    uint32_t length;
    uint8_t type;
    uint8_t error;
    uint32_t request;
  };
#pragma pack(pop)

  int mySocket;
  bool myAlive;

  Command myCommand;
  std::vector<uint8_t> myPayloadIn;

  Response myResponse;
  std::vector<uint8_t> myPayloadOut;

  ssize_t readData(char * dest, size_t len);

  bool readCommand();
  bool readPayload();
  void processCommand();
  void sendReply(const uint8_t type, const uint32_t request, const uint8_t error);
  
  void sendResourceStringReply(const uint32_t request, const uint8_t error, const char * value);
  void sendResourceIntReply(const uint32_t request, const uint8_t error, const uint32_t value);

  void sendError(const uint8_t type, const uint8_t error);
  void writeString(const char * value);
  void writeInt32(const uint32_t value);
  void writeInt16(const uint16_t value);
  void writeInt8(const uint8_t value);

  void cmdViceInfo();
  void cmdResourceGet();
  void cmdRegistersAvailable();
  void cmdDisplayGet();
  void cmdBanksAvailable();
};

class BinaryMonitor
{
public:
  BinaryMonitor();
  ~BinaryMonitor();

  void process();

private:
  int mySocket;
  std::vector<std::shared_ptr<BinaryClient>> myClients;
};
