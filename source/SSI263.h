#pragma once

#include "MockingboardDefs.h"

class SSI263
{
public:
	SSI263(UINT slot) : m_slot(slot)
	{
		m_type = SSI263AP;
		m_device = -1;	// undefined
		m_cardMode = PH_Mockingboard;
		m_hasSC01 = true;	// only for m_device==0
		m_pPhonemeData00 = NULL;

		ResetState(true);
	}
	~SSI263(void)
	{
		delete [] m_pPhonemeData00;
	}

	void ResetState(const bool powerCycle)
	{
		if (powerCycle)
		{
			m_currentActivePhoneme = -1;
			m_isVotraxPhoneme = false;	// SC01 has no RESET pin
			m_votraxPhoneme = 0;		// SC01 has no RESET pin
		}

		m_cyclesThisAudioFrame = 0;

		//

		m_lastUpdateCycle = 0;
		m_updateWasFullSpeed = false;

		m_pPhonemeData = NULL;
		m_phonemeLengthRemaining = 0;
		m_phonemeAccurateLengthRemaining = 0;
		m_phonemePlaybackAndDebugger = false;
		m_phonemeCompleteByFullSpeed = false;
		m_phonemeLeadoutLength = 0;

		//

		m_numSamplesError = 0;
		m_byteOffset = (uint32_t)-1;
		m_currSampleSum = 0;
		m_currNumSamples = 0;
		m_currSampleMod4 = 0;

		//

		// After a chip power-on, if the first thing done is CTL=0, then empirically it can be observed that:
		// . enableInts = 1 (mostly an SSI263 interrupt occurs, but not always)
		// . DR1:0 != b#00 (since enableInts is set to 1, except when no interrupt => DR1:0 == b#00)
		m_durationPhoneme = MODE_PHONEME_TRANSITIONED_INFLECTION;	// Typical function & phoneme=$00
		m_inflection = 0;
		m_rateInflection = 0;
		m_ctrlArtAmp = (powerCycle || m_type == SSI263AP) ? CONTROL_MASK : 0;				// Chip power-on, so CTL=1 (power-down / standby)
		m_filterFreq = powerCycle ? FILTER_FREQ_SILENCE : 0;		// Empirically observed at chip power-on (GH#1302)

		m_currentMode.mode = 0;
		m_currentMode.function = 0;		// Set at runtime when CTL=0
		m_currentMode.enableInts = 0;	// Set at runtime when CTL=0
		m_currentMode.D7 = 0;			// Since in power-down mode (as per SSI263 datasheet)

		//

		m_dbgFirst = true;
		m_dbgStartTime = 0;
	}

	void SetDevice(UINT device) { m_device = device; }
	void SetCardMode(PHASOR_MODE mode);
	void SetType(SSI263Type type) { m_type = type; }
	void SetSC01(SSI263Type type) { m_hasSC01 = (type == SC01); }

	void DSUninit(void);

	void Reset(const bool powerCycle, const bool isPhasorCard);

	BYTE Read(ULONG nExecutedCycles);
	void Write(BYTE nReg, BYTE nValue);

	void Mute(void);
	void Unmute(void);
	void SetVolume(uint32_t dwVolume, uint32_t dwVolumeMax);

	void PeriodicUpdate(UINT executedCycles);
	void Update(void);
	void SetSpeechIRQ(void);

	void Votrax_Write(BYTE nValue);
	void SetVotraxPhoneme(bool value) { m_isVotraxPhoneme = value; }

	void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper, UINT subunit);
	void LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, PHASOR_MODE mode, UINT version, UINT subunit);

private:
	bool IsPhonemeActive(void)
	{
		// If SSI263.CONTROL=1 then "m_currentActivePhoneme >= 0" is still true
		// Also valid regardless of m_isVotraxPhoneme state
		return m_currentActivePhoneme >= 0;
	}
	void Play(unsigned int nPhoneme);
	void Stop(void);
	void UpdateIRQ(void);
	void RepeatPhoneme(void);
	void UpdateAccurateLength(void);
	void SetDeviceModeAndInts(void);

	UINT64 GetLastCumulativeCycles(void);
	void UpdateIFR(BYTE nDevice, BYTE clr_mask, BYTE set_mask);
	BYTE GetPCR(BYTE nDevice);

	bool Init(void);
	bool DSInit(void);

	void SC01_SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	void SC01_LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

	static const BYTE m_Votrax2SSI263[/*64*/];

	static const unsigned short m_kNumChannels = 1;
	static const uint32_t m_kDSBufferByteSize = MAX_SAMPLES * sizeof(short) * m_kNumChannels;
	short m_mixBufferSSI263[m_kDSBufferByteSize / sizeof(short)];
	VOICE SSI263SingleVoice;

	//

	// Duration/Phonome
	static const BYTE DURATION_MODE_MASK = 0xC0;
	static const BYTE DURATION_MODE_SHIFT = 6;
	static const BYTE PHONEME_MASK = 0x3F;

	static const BYTE MODE_PHONEME_TRANSITIONED_INFLECTION = 0xC0;
	static const BYTE MODE_PHONEME_IMMEDIATE_INFLECTION = 0x80;
	static const BYTE MODE_FRAME_IMMEDIATE_INFLECTION = 0x40;
	static const BYTE MODE_IRQ_DISABLED = 0x00;	// disable interrupts, but retains one of the 3 modes

	// Rate/Inflection
	static const BYTE RATE_MASK = 0xF0;
	static const BYTE INFLECTION_MASK_H = 0x08;	// I11
	static const BYTE INFLECTION_MASK_L = 0x07;	// I2..I0

	// Ctrl/Art/Amp
	static const BYTE ARTICULATION_MASK = 0x70;
	static const BYTE AMPLITUDE_MASK = 0x0F;
	static const BYTE CONTROL_MASK = 0x80;

	// Filter frequency range
	static const BYTE FILTER_FREQ_SILENCE = 0xFF;

	SSI263Type m_type;
	UINT m_slot;
	BYTE m_device;	// SSI263 device# which is generating phoneme-complete IRQ (and only required whilst Mockingboard isn't a class)
	PHASOR_MODE m_cardMode;
	bool m_hasSC01;
	short* m_pPhonemeData00;

	// ctor/power-cycle: Set to -1
	// Play(): Set to [$00-$3F] on a write to DURPHON register.
	// UpdateIRQ(): OR'd with kPhonemeLeadoutFlag when phoneme ends.
	// RepeatPhoneme(): AND'd with PHONEME_MASK, if SSI263.CONTROL==0 call Play() again.
	// SSI263.CONTROL 1->0 will call Play().
	// NB. Can be used to detect overlapping phonemes in Play().
	// NB. For SSI263 (and SC01) once >=0 then this remains the case (even when SSI263.CONTROL=1).
	static const UINT kPhonemeLeadoutFlag = 0x100;
	int m_currentActivePhoneme;				// -1 (if none) or SSI263/SC01 phoneme (& can be OR'd with kPhonemeLeadoutFlag)

	bool m_isVotraxPhoneme;
	BYTE m_votraxPhoneme;
	UINT m_cyclesThisAudioFrame;

	//

	UINT64 m_lastUpdateCycle;
	bool m_updateWasFullSpeed;

	const short* m_pPhonemeData;
	UINT m_phonemeLengthRemaining;			// length in samples, decremented as space becomes available in the ring-buffer
	UINT m_phonemeAccurateLengthRemaining;	// length in samples, decremented by cycles executed
	bool m_phonemePlaybackAndDebugger;
	bool m_phonemeCompleteByFullSpeed;
	UINT m_phonemeLeadoutLength;			// length in samples, decremented after \m_phonemeLengthRemaining\ goes to zero. Delay until phoneme repeats

	//

	int m_numSamplesError;
	uint32_t m_byteOffset;
	int m_currSampleSum;
	int m_currNumSamples;
	UINT m_currSampleMod4;

	// Regs:
	BYTE m_durationPhoneme;
	BYTE m_inflection;		// I10..I3
	BYTE m_rateInflection;
	BYTE m_ctrlArtAmp;
	BYTE m_filterFreq;

	union
	{
		struct
		{
			BYTE D7 : 1;			// b0=D7 pin (for IRQ)
			BYTE reserved : 4;
			BYTE enableInts : 1;	// b5 = enable A/!R (ie. interrupts)
			BYTE function : 2;		// b7:6 = function
		};
		BYTE mode;
	} m_currentMode;

	// Debug
	bool m_dbgFirst;
	UINT64 m_dbgStartTime;
};
