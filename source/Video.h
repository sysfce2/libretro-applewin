#pragma once

// in Windows it seems that the ALPHA value is irrelevant
// we have selected 0xFF since it works everywhere
// Windows
// SDL (SDL_PIXELFORMAT_ARGB8888)
// Qt (QImage::Format_ARGB32_Premultiplied)
#define ALPHA 0xFF  // fully opaque
#define ALPHA32_MASK (ALPHA << 24)
#define SETRGBCOLOR(r, g, b) {b, g, r, ALPHA}  // to create a RGBQUAD
#define OPAQUE_BLACK (0 | ALPHA32_MASK)  // Black is RGB 0 0 0


// Types ____________________________________________________________

// NOTE: KEEP IN SYNC: VideoType_e g_aVideoChoices g_apVideoModeDesc
// NOTE: Used/Serialized by: g_eVideoType
enum VideoType_e
{
	  VT_MONO_CUSTOM
	, VT_COLOR_IDEALIZED		// Color rendering from AppleWin 1.25 (GH#357)
	, VT_COLOR_VIDEOCARD_RGB    // Real RGB card rendering
	, VT_COLOR_MONITOR_NTSC		// NTSC or PAL
	, VT_COLOR_TV
	, VT_MONO_TV
	, VT_MONO_AMBER
	, VT_MONO_GREEN
	, VT_MONO_WHITE
	, NUM_VIDEO_MODES
	, VT_DEFAULT = VT_COLOR_TV
};

extern char g_aVideoChoices[];

enum VideoStyle_e
{
	VS_NONE=0,
	VS_HALF_SCANLINES=1,		// drop 50% scan lines for a more authentic look
	VS_COLOR_VERTICAL_BLEND=2,	// Color "TV Emu" rendering from AppleWin 1.25 (GH#616)
//	VS_TEXT_OPTIMIZED=4,
};

enum VideoRefreshRate_e
{
	VR_NONE,
	VR_50HZ,
	VR_60HZ
};

enum VideoFlag_e
{	                   //  76543210
	VF_80COL           = 0x00000001,
	VF_DHIRES          = 0x00000002,
	VF_HIRES           = 0x00000004,
	VF_80STORE         = 0x00000008,
	VF_MIXED           = 0x00000010,
	VF_PAGE2           = 0x00000020,		// Text or Hires
	VF_TEXT            = 0x00000040,
	VF_SHR             = 0x00000080,		// For VidHD's support for IIgs SHR video modes
	VF_80COL_AUX_EMPTY = 0x00000100,		// For 80COL when aux slot is empty (returns floating bus)

	VF_PAGE0           = 0x10000000,		// Debugger: Pseudo Page $00 (Poorman's heatmap)
	VF_PAGE3           = 0x20000000,		// Debugger: Pseudo Page $60 (Poorman's heatmap)
	VF_PAGE4           = 0x40000000,		// Debugger: Pseudo Page $80 (Poorman's heatmap)
	VF_PAGE5           = 0x80000000,		// Debugger: Pseudo Page $A0 (Poorman's heatmap)
	VF_PAGE6           = 0x00000200,		// Debugger: Pseudo Page $C0 (Poorman's heatmap) LC Bank 1/2
	VF_PAGE7           = 0x00000400,		// Debugger: Pseudo Page $D0 (Poorman's heatmap) LC Bank 2/-
	VF_PAGE8           = 0x00000800			// Debugger: Psuedo Page $E0 (Poorman's heatmap) LC Bank RAM
};

enum AppleFont_e
{
	// 40-Column mode is 1x Zoom (default)
	// 80-Column mode is ~0.75x Zoom (7 x 16)
	// Tiny mode is 0.5 zoom (7x8) for debugger
	APPLE_FONT_WIDTH  = 14, // in pixels
	APPLE_FONT_HEIGHT = 16, // in pixels

	// Each cell has a reserved aligned pixel area (grid spacing)
	APPLE_FONT_CELL_WIDTH  = 16,
	APPLE_FONT_CELL_HEIGHT = 16,

	// The bitmap contains 3 regions
	// Each region is 256x256 pixels = 16x16 chars
	APPLE_FONT_X_REGIONSIZE = 256, // in pixelx
	APPLE_FONT_Y_REGIONSIZE = 256, // in pixels

	// Starting Y offsets (pixels) for the regions
	APPLE_FONT_Y_APPLE_2PLUS =   0, // ][+
	APPLE_FONT_Y_APPLE_80COL = 256, // //e (inc. Mouse Text)
	APPLE_FONT_Y_APPLE_40COL = 512, // ][
};


// turn on struct member padding
#pragma pack(push,1)

// TODO: Replace with WinGDI.h / RGBQUAD
struct bgra_t
{
	uint8_t b;
	uint8_t g;
	uint8_t r;
	uint8_t a; // reserved on Win32
};

struct WinBmpHeader_t
{
	// BITMAPFILEHEADER     // Addr Size
	uint8_t  nCookie[2]      ; // 0x00 0x02 BM
	uint32_t nSizeFile       ; // 0x02 0x04 0 = ignore
	uint16_t nReserved1      ; // 0x06 0x02
	uint16_t nReserved2      ; // 0x08 0x02
	uint32_t nOffsetData     ; // 0x0A 0x04
	//                      ==      0x0D (14)

	// BITMAPINFOHEADER
	uint32_t nStructSize     ; // 0x0E 0x04 biSize
	uint32_t nWidthPixels    ; // 0x12 0x04 biWidth
	uint32_t nHeightPixels   ; // 0x16 0x04 biHeight
	uint16_t nPlanes         ; // 0x1A 0x02 biPlanes
	uint16_t nBitsPerPixel   ; // 0x1C 0x02 biBitCount
	uint32_t nCompression    ; // 0x1E 0x04 biCompression 0 = BI_RGB
	uint32_t nSizeImage      ; // 0x22 0x04 0 = ignore
	uint32_t nXPelsPerMeter  ; // 0x26 0x04
	uint32_t nYPelsPerMeter  ; // 0x2A 0x04
	uint32_t nPaletteColors  ; // 0x2E 0x04
	uint32_t nImportantColors; // 0x32 0x04
	//                      ==      0x28 (40)

	// RGBQUAD
	// pixelmap
};

struct WinCIEXYZ
{
	uint32_t r; // fixed point 2.30
	uint32_t g; // fixed point 2.30
	uint32_t b; // fixed point 2.30
};

struct WinBmpHeader4_t
{
	// BITMAPFILEHEADER        // Addr Size
	uint8_t  nCookie[2]      ; // 0x00 0x02 BM
	uint32_t nSizeFile       ; // 0x02 0x04 0 = ignore
	uint16_t nReserved1      ; // 0x06 0x02
	uint16_t nReserved2      ; // 0x08 0x02
	uint32_t nOffsetData     ; // 0x0A 0x04
	//                            ==== 0x0D (14)

	// BITMAPINFOHEADER
	uint32_t nStructSize     ; // 0x0E 0x04 biSize
	uint32_t nWidthPixels    ; // 0x12 0x04 biWidth
	uint32_t nHeightPixels   ; // 0x16 0x04 biHeight
	uint16_t nPlanes         ; // 0x1A 0x02 biPlanes
	uint16_t nBitsPerPixel   ; // 0x1C 0x02 biBitCount
	uint32_t nCompression    ; // 0x1E 0x04 biCompression 0 = BI_RGB
	uint32_t nSizeImage      ; // 0x22 0x04 0 = ignore
	uint32_t nXPelsPerMeter  ; // 0x26 0x04
	uint32_t nYPelsPerMeter  ; // 0x2A 0x04
	uint32_t nPaletteColors  ; // 0x2E 0x04
	uint32_t nImportantColors; // 0x32 0x04
	//                            ==== 0x28 (40)

	//BITMAPV4HEADER new fields
	uint32_t nRedMask        ; // 0x36 0x04
	uint32_t nGreenMask      ; // 0x3A 0x04
	uint32_t nBlueMask       ; // 0x3E 0x04
	uint32_t nAlphaMask      ; // 0x42 0x04
	uint32_t nType           ; // 0x46 0x04

	uint32_t Rx, Ry, Rz      ; // 0x4A 0x0C
	uint32_t Gx, Gy, Gz      ; // 0x56 0x0C
	uint32_t Bx, By, Bz      ; // 0x62 0x0C

	uint32_t nRedGamma       ; // 0x6E 0x04
	uint32_t nGreenGamma     ; // 0x72 0x04
	uint32_t nBlueGamma      ; // 0x76 0x04
};

#pragma pack(pop)
//

class Video
{
public:
	Video(void)
	{
		g_pFramebufferbits = NULL; // last drawn frame (initialized in WinVideoInitialize)
		g_nAltCharSetOffset = 0;
		g_uVideoMode = VF_TEXT;
		g_eVideoType = VT_DEFAULT;
		g_eVideoStyle = VS_HALF_SCANLINES;
		g_bVideoScannerNTSC = true;
		g_nMonochromeRGB = RGB(0xC0,0xC0,0xC0);
		g_videoRomSize = 0;
		g_videoRomRockerSwitch = false;
		m_hasVidHD = false;
	}

	~Video(void){}

	void Initialize(uint8_t* frameBuffer, bool resetState); // Do not call directly. Call FrameBase::Initialize()
	void Destroy(void); // Call FrameBase::Destroy()

	uint8_t* GetFrameBuffer(void) { return g_pFramebufferbits; }

	// size of the video buffer stored in g_pFramebufferbits
	UINT GetFrameBufferBorderlessWidth(void);
	UINT GetFrameBufferBorderlessHeight(void);
	UINT GetFrameBufferBorderWidth(void);
	UINT GetFrameBufferBorderHeight(void);
	UINT GetFrameBufferWidth(void);
	UINT GetFrameBufferHeight(void);
	UINT GetFrameBufferCentringOffsetX(void);
	UINT GetFrameBufferCentringOffsetY(void);
	int GetFrameBufferCentringValue(void);

	COLORREF GetMonochromeRGB(void) { return g_nMonochromeRGB; }
	void SetMonochromeRGB(COLORREF colorRef) { g_nMonochromeRGB = colorRef; }

	void VideoReinitialize(bool bInitVideoScannerAddress);
	void VideoResetState(void);
	void VideoRefreshBuffer(uint32_t uRedrawWholeScreenVideoMode, bool bRedrawWholeScreen);
	void ClearFrameBuffer(void);
	void ClearSHRResidue(void);

	enum VideoScanner_e {VS_FullAddr, VS_PartialAddrV, VS_PartialAddrH};
	WORD VideoGetScannerAddress(uint32_t nCycles, VideoScanner_e videoScannerAddr = VS_FullAddr);
	bool VideoGetVblBarEx(const uint32_t dwCyclesThisFrame);
	bool VideoGetVblBar(const uint32_t uExecutedCycles);

	bool VideoGetSW80COL(void);
	bool VideoGetSWDHIRES(void);
	bool VideoGetSWHIRES(void);
	bool VideoGetSW80STORE(void);
	bool VideoGetSWMIXED(void);
	bool VideoGetSWPAGE2(void);
	bool VideoGetSWTEXT(void);
	bool VideoGetSWAltCharSet(void);
	bool VideoGet80COLAUXEMPTY(void);

	void VideoSaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	void VideoLoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT version);

	enum VideoScreenShot_e
	{
		SCREENSHOT_560x384 = 0,
		SCREENSHOT_280x192
	};
	void Video_SetBitmapHeader(WinBmpHeader_t *pBmp, int nWidth, int nHeight, int nBitsPerPixel);

	void Video_MakeScreenShot(FILE* pFile, const VideoScreenShot_e ScreenShotType);

	BYTE VideoSetMode(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG uExecutedCycles);

	bool ReadVideoRomFile(const char* pRomFile);
	UINT GetVideoRom(const BYTE*& pVideoRom);
	bool GetVideoRomRockerSwitch(void);
	void SetVideoRomRockerSwitch(bool state);
	bool IsVideoRom4K(void);

	void Config_Load_Video(void);
	void Config_Save_Video(void);

	uint32_t GetVideoMode(void);
	void SetVideoMode(uint32_t videoMode);
	VideoType_e GetVideoType(void);
	void SetVideoType(VideoType_e newVideoType);
	void IncVideoType(void);
	void DecVideoType(void);
	VideoStyle_e GetVideoStyle(void);
	void SetVideoStyle(VideoStyle_e newVideoStyle);
	bool IsVideoStyle(VideoStyle_e mask);

	VideoRefreshRate_e GetVideoRefreshRate(void);
	void SetVideoRefreshRate(VideoRefreshRate_e rate);

	const char* VideoGetAppWindowTitle(void);
	const char* GetVideoChoices(void) { return g_aVideoChoices; }

	bool HasVidHD(void) { return m_hasVidHD; }
	void SetVidHD(bool hasVidHD) { m_hasVidHD = hasVidHD; }

	static const UINT kVideoRomSize2K = 1024*2;
	static const UINT kVideoRomSize4K = kVideoRomSize2K*2;

protected:
	uint8_t *g_pFramebufferbits;

private:
	void SetFrameBuffer(uint8_t* frameBuffer) { g_pFramebufferbits = frameBuffer; }
	const std::string& VideoGetSnapshotStructName(void);

	int g_nAltCharSetOffset;
	uint32_t g_uVideoMode;		// Current Video Mode (this is the last set one as it may change mid-scan line!)
	uint32_t g_eVideoType;			// saved to Registry
	VideoStyle_e g_eVideoStyle;
	bool g_bVideoScannerNTSC;	// NTSC video scanning (or PAL)
	COLORREF g_nMonochromeRGB;	// saved to Registry
	bool m_hasVidHD;

	static const UINT kVideoRomSize8K = kVideoRomSize4K*2;
	static const UINT kVideoRomSize16K = kVideoRomSize8K*2;
	static const UINT kVideoRomSizeMax = kVideoRomSize16K;
	BYTE g_videoRom[kVideoRomSizeMax];
	UINT g_videoRomSize;
	bool g_videoRomRockerSwitch;

	static const char g_aVideoChoices[];

	static const char m_szModeDesc0[];
	static const char m_szModeDesc1[];
	static const char m_szModeDesc2[];
	static const char m_szModeDesc3[];
	static const char m_szModeDesc4[];
	static const char m_szModeDesc5[];
	static const char m_szModeDesc6[];
	static const char m_szModeDesc7[];
	static const char m_szModeDesc8[];
	static const char* const g_apVideoModeDesc[NUM_VIDEO_MODES];

	static const UINT kVideoHeightII = 192*2;
	static const UINT kVideoHeightIIgs = 200*2;

	static const UINT kVideoWidthII = 280*2;
	static const UINT kVideoWidthIIgs = 320*2;
};
