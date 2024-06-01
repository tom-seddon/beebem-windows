/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2007  Mike Wyatt

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public
License along with this program; if not, write to the Free
Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA  02110-1301, USA.
****************************************************************/

// BeebWin preferences support

#include <algorithm>
#include <stdio.h>

#include "BeebWin.h"
#include "6502core.h"
#include "Arm.h"
#include "BeebMem.h"
#include "Disc1770.h"
#include "Disc8271.h"
#include "Econet.h"
#include "Ide.h"
#include "IP232.h"
#include "KeyMap.h"
#include "Main.h"
#include "Music5000.h"
#include "Resource.h"
#include "Rtc.h"
#include "Scsi.h"
#include "Serial.h"
#include "Sound.h"
#include "SoundStreamer.h"
#include "Speech.h"
#include "SprowCoPro.h"
#include "SysVia.h"
#include "Teletext.h"
#include "Tube.h"
#include "UserPortBreakoutBox.h"
#include "UserPortRTC.h"
#include "UserVia.h"
#include "Video.h"
#include "Z80mem.h"
#include "Z80.h"

/* Configuration file strings */
static const char *CFG_VIEW_WIN_SIZE = "WinSize";
static const char *CFG_VIEW_MONITOR = "Monitor";
static const char *CFG_SOUND_SAMPLE_RATE = "SampleRate";
static const char *CFG_SOUND_VOLUME = "SoundVolume";
static const char *CFG_SOUND_ENABLED = "SoundEnabled";
static const char *CFG_OPTIONS_STICKS = "Sticks";
static const char *CFG_OPTIONS_KEY_MAPPING = "KeyMapping";
static const char *CFG_OPTIONS_USER_KEY_MAP_FILE = "UserKeyMapFile";
static const char *CFG_OPTIONS_FREEZEINACTIVE = "FreezeWhenInactive";
static const char *CFG_OPTIONS_HIDE_CURSOR = "HideCursor";
static const char* CFG_OPTIONS_CAPTURE_MOUSE = "CaptureMouse";
static const char *CFG_SPEED_TIMING = "Timing";
static const char *CFG_AMX_ENABLED = "AMXMouseEnabled";
static const char *CFG_AMX_LRFORMIDDLE = "AMXMouseLRForMiddle";
static const char *CFG_AMX_SIZE = "AMXMouseSize";
static const char *CFG_AMX_ADJUST = "AMXMouseAdjust";
static const char *CFG_PRINTER_ENABLED = "PrinterEnabled";
static const char *CFG_PRINTER_PORT = "PrinterPort";
static const char *CFG_PRINTER_FILE = "PrinterFile";
static const char *CFG_MACHINE_TYPE = "MachineType";
static const char *CFG_TUBE_TYPE = "TubeType";

#define LED_COLOUR_TYPE (LEDByte&4)>>2
#define LED_SHOW_KB (LEDByte&1)
#define LED_SHOW_DISC (LEDByte&2)>>1

static int Clamp(int Value, int MinValue, int MaxValue)
{
	return std::min(std::max(Value, MinValue), MaxValue);
}

void BeebWin::LoadPreferences()
{
	Preferences::Result result = m_Preferences.Load(m_PrefsFile);

	if (result == Preferences::Result::Failed) {
		// No prefs file, will use defaults
		Report(MessageType::Error,
		       "Cannot open preferences file:\n  %s\n\nUsing default preferences",
		       m_PrefsFile);
	}
	else if (result == Preferences::Result::InvalidFormat) {
		Report(MessageType::Error,
		       "Invalid preferences file:\n  %s\n\nUsing default preferences",
		       m_PrefsFile);
	}

	char path[_MAX_PATH];
	char keyData[256];
	unsigned char flag;
	DWORD dword;
	int Value;

	// Remove obsolete prefs
	m_Preferences.EraseValue("UserKeyMapRow");
	m_Preferences.EraseValue("UserKeyMapCol");
	m_Preferences.EraseValue("ShowBottomCursorLine");
	m_Preferences.EraseValue("Volume");
	m_Preferences.EraseValue("UsePrimaryBuffer");
	m_Preferences.EraseValue("ShowFSP");

	m_Preferences.EraseValue("IP232localhost");
	m_Preferences.EraseValue("IP232custom");
	m_Preferences.EraseValue("IP232customport");
	m_Preferences.EraseValue("IP232customip");

	// Set file path defaults
	if (!m_Preferences.HasValue("DiscsPath")) {
		m_Preferences.SetStringValue("DiscsPath", "DiscIms");
	}

	if (!m_Preferences.HasValue("DiscsFilter")) {
		m_Preferences.SetDWORDValue("DiscsFilter", 0);
	}

	if (!m_Preferences.HasValue("TapesPath")) {
		m_Preferences.SetStringValue("TapesPath", "Tapes");
	}

	if (!m_Preferences.HasValue("StatesPath")) {
		m_Preferences.SetStringValue("StatesPath", "BeebState");
	}

	if (!m_Preferences.HasValue("AVIPath")) {
		m_Preferences.SetStringValue("AVIPath", "");
	}

	if (!m_Preferences.HasValue("ImagePath")) {
		m_Preferences.SetStringValue("ImagePath", "");
	}

	if (!m_Preferences.HasValue("HardDrivePath")) {
		m_Preferences.SetStringValue("HardDrivePath", "DiscIms");
	}

	MachineType = Model::B;

	unsigned char type = 0;

	if (m_Preferences.GetBinaryValue(CFG_MACHINE_TYPE, &type, 1))
	{
		if (type < MODEL_COUNT)
		{
			MachineType = static_cast<Model>(type);
		}
	}

	if (!m_Preferences.GetBoolValue("WriteProtectOnLoad", m_WriteProtectOnLoad))
		m_WriteProtectOnLoad = true;

	if (m_Preferences.GetDWORDValue("DisplayRenderer", dword))
	{
		switch (dword)
		{
			case 0: case 40217:          m_DisplayRenderer = DisplayRendererType::GDI; break;
			case 1: case 40218:          m_DisplayRenderer = DisplayRendererType::DirectDraw; break;
			case 2: case 40219: default: m_DisplayRenderer = DisplayRendererType::DirectX9; break;
		}
	}
	else
	{
		m_DisplayRenderer = DisplayRendererType::DirectX9;
	}

	if (!m_Preferences.GetBoolValue("DXSmoothing", m_DXSmoothing))
		m_DXSmoothing = true;

	if (!m_Preferences.GetBoolValue("DXSmoothMode7Only", m_DXSmoothMode7Only))
		m_DXSmoothMode7Only = false;

	if (m_Preferences.GetDWORDValue("DDFullScreenMode", dword))
	{
		switch (dword)
		{
			case 0: case 40102: default: m_DDFullScreenMode = DirectXFullScreenMode::ScreenResolution; break;
			case 1: case 40099:          m_DDFullScreenMode = DirectXFullScreenMode::_640x480; break;
			case 2: case 40279:          m_DDFullScreenMode = DirectXFullScreenMode::_720x576; break;
			case 3: case 40280:          m_DDFullScreenMode = DirectXFullScreenMode::_800x600; break;
			case 4: case 40100:          m_DDFullScreenMode = DirectXFullScreenMode::_1024x768; break;
			case 5: case 40288:          m_DDFullScreenMode = DirectXFullScreenMode::_1280x720; break;
			case 6: case 40101:          m_DDFullScreenMode = DirectXFullScreenMode::_1280x1024; break;
			case 7: case 40221:          m_DDFullScreenMode = DirectXFullScreenMode::_1280x768; break;
			case 8: case 40222:          m_DDFullScreenMode = DirectXFullScreenMode::_1280x960; break;
			case 9: case 40223:          m_DDFullScreenMode = DirectXFullScreenMode::_1440x900; break;
			case 10: case 40224:         m_DDFullScreenMode = DirectXFullScreenMode::_1600x1200; break;
			case 11: case 40289:         m_DDFullScreenMode = DirectXFullScreenMode::_1920x1080; break;
			case 12: case 40294:         m_DDFullScreenMode = DirectXFullScreenMode::_2560x1440; break;
			case 13: case 40295:         m_DDFullScreenMode = DirectXFullScreenMode::_3840x2160; break;
		}
	}
	else
	{
		m_DDFullScreenMode = DirectXFullScreenMode::ScreenResolution;
	}

	TranslateDDSize();

	if (m_Preferences.GetDWORDValue(CFG_VIEW_WIN_SIZE, dword))
	{
		switch (dword)
		{
			case 40005:          m_XWinSize = 320; m_YWinSize = 256; break;
			case 40006: default: m_XWinSize = 640; m_YWinSize = 512; break;
			case 40007:          m_XWinSize = 800; m_YWinSize = 600; break;
			case 40008:          m_XWinSize = 1024; m_YWinSize = 768; break;
			case 40009:          m_XWinSize = 1024; m_YWinSize = 512; break;
			case 40225:          m_XWinSize = 1280; m_YWinSize = 1024; break;
			case 40226:          m_XWinSize = 1440; m_YWinSize = 1080; break;
			case 40227:          m_XWinSize = 1600; m_YWinSize = 1200; break;
			case 40281: // Custom
				if (m_Preferences.GetDWORDValue("WinSizeX", dword))
				{
					m_XWinSize = dword;
				}
				else
				{
					m_XWinSize = 640;
				}

				if (m_Preferences.GetDWORDValue("WinSizeY", dword))
				{
					m_YWinSize = dword;
				}
				else
				{
					m_YWinSize = 512;
				}
				break;
		}
	}
	else
	{
		if (m_Preferences.GetDWORDValue("WinSizeX", dword))
		{
			m_XWinSize = dword;
		}
		else
		{
			m_XWinSize = 640;
		}

		if (m_Preferences.GetDWORDValue("WinSizeY", dword))
		{
			m_YWinSize = dword;
		}
		else
		{
			m_YWinSize = 512;
		}
	}

	if (!m_Preferences.GetBoolValue("FullScreen", m_isFullScreen))
		m_isFullScreen = false;

	if (!m_Preferences.GetBoolValue("MaintainAspectRatio", m_MaintainAspectRatio))
		m_MaintainAspectRatio = true;

	if (!m_Preferences.GetBoolValue("ShowFPS", m_ShowSpeedAndFPS))
	{
		// This option was named "ShowFSP" in BeebEm v4.18 and earlier
		if (!m_Preferences.GetBoolValue("ShowFSP", m_ShowSpeedAndFPS))
		{
			m_ShowSpeedAndFPS = true;
		}
	}

	if (m_Preferences.GetBinaryValue(CFG_VIEW_MONITOR, &flag, 1))
	{
		switch (flag)
		{
			case 0: default: m_PaletteType = PaletteType::RGB; break;
			case 1:          m_PaletteType = PaletteType::BW; break;
			case 2:          m_PaletteType = PaletteType::Amber; break;
			case 3:          m_PaletteType = PaletteType::Green; break;
		}
	}

	if (!m_Preferences.GetBoolValue("HideMenuEnabled", m_HideMenuEnabled))
		m_HideMenuEnabled = false;

	unsigned char LEDByte = 0;

	if (m_Preferences.GetBinaryValue("LED Information", &LEDByte, 1))
	{
		if (LEDByte == 0 || LEDByte == 1)
		{
			m_DiscLedColour = static_cast<LEDColour>(LED_COLOUR_TYPE);
		}
	}

	LEDs.ShowDisc = (LED_SHOW_DISC != 0) && MachineType != Model::MasterET;
	LEDs.ShowKB = LED_SHOW_KB;

	if (m_Preferences.GetDWORDValue("MotionBlur", dword))
	{
		switch (dword)
		{
			case 0: case 40177: default: m_MotionBlur = 0; break;
			case 2: case 40178:          m_MotionBlur = 2; break;
			case 4: case 40179:          m_MotionBlur = 4; break;
			case 8: case 40180:          m_MotionBlur = 8; break;
		}
	}
	else
	{
		m_MotionBlur = 0;
	}

	if (!m_Preferences.GetBinaryValue("MotionBlurIntensities", m_BlurIntensities, 8))
	{
		memcpy(m_BlurIntensities, DefaultBlurIntensities, sizeof(m_BlurIntensities));
	}

	if (!m_Preferences.GetBoolValue("TextViewEnabled", m_TextViewEnabled))
		m_TextViewEnabled = false;

	if (m_Preferences.GetDWORDValue(CFG_SPEED_TIMING, dword))
	{
		if (dword >= 0 && dword < 3)
		{
			m_TimingType = static_cast<TimingType>(dword);

			if (m_Preferences.GetDecimalValue("Speed", Value))
			{
				if (m_TimingType == TimingType::FixedFPS)
				{
					if (Value == 50 || Value == 25 || Value == 10 || Value == 5 || Value == 1)
					{
						m_TimingSpeed = Value;
					}
					else
					{
						m_TimingSpeed = 50;
					}
				}
				else if (m_TimingType == TimingType::FixedSpeed)
				{
					if (Value == 10000 || Value == 5000 || Value == 1000 || Value == 500 ||
					    Value == 200 || Value == 150 || Value == 125 || Value == 110 ||
					    Value == 90 || Value == 75 || Value == 50 || Value == 25 || Value == 10)
					{
						m_TimingSpeed = Value;
					}
					else
					{
						m_TimingSpeed = 100;
					}
				}
			}
			else
			{
				m_TimingSpeed = 100;
			}
		}
		else
		{
			switch (dword)
			{
				case 40024: m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 100; break;
				case 40025: m_TimingType = TimingType::FixedFPS;   m_TimingSpeed = 50; break;
				case 40026: m_TimingType = TimingType::FixedFPS;   m_TimingSpeed = 25; break;
				case 40027: m_TimingType = TimingType::FixedFPS;   m_TimingSpeed = 10; break;
				case 40028: m_TimingType = TimingType::FixedFPS;   m_TimingSpeed = 5; break;
				case 40029: m_TimingType = TimingType::FixedFPS;   m_TimingSpeed = 1; break;
				case 40151: m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 10000; break;
				case 40154: m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 500; break;
				case 40155: m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 200; break;
				case 40156: m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 150; break;
				case 40157: m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 125; break;
				case 40158: m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 110; break;
				case 40159: m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 90; break;
				case 40160: m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 75; break;
				case 40161: m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 5000; break;
				case 40162: m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 25; break;
				case 40163: m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 1000; break;
				case 40164: m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 50; break;
				case 40165: m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 10; break;
			}
		}
	}
	else
	{
		m_TimingType = TimingType::FixedSpeed;
		m_TimingSpeed = 100;
	}

	TranslateTiming();

	if (m_Preferences.GetDWORDValue("SoundConfig::Selection", dword))
	{
		switch (dword)
		{
			case 0: default: SelectedSoundStreamer = SoundStreamerType::XAudio2; break;
			case 1:          SelectedSoundStreamer = SoundStreamerType::DirectSound; break;
		}
	}
	else
	{
		SelectedSoundStreamer = SoundStreamerType::XAudio2;
	}

	if (!m_Preferences.GetBoolValue(CFG_SOUND_ENABLED, SoundDefault))
		SoundDefault = true;

	if (!m_Preferences.GetBoolValue("SoundChipEnabled", SoundChipEnabled))
		SoundChipEnabled = true;

	if (m_Preferences.GetDecimalValue(CFG_SOUND_SAMPLE_RATE, Value))
	{
		if (Value == 11025 || Value == 22050 || Value == 44100)
		{
			m_SampleRate = Value;
		}
		else
		{
			m_SampleRate = 44100;
		}
	}
	else if (m_Preferences.GetDWORDValue(CFG_SOUND_SAMPLE_RATE, dword))
	{
		switch (dword)
		{
			case 40016:          m_SampleRate = 11025; break;
			case 40015:          m_SampleRate = 22050; break;
			case 40014: default: m_SampleRate = 44100; break;
		}
	}
	else
	{
		m_SampleRate = 44100;
	}

	if (m_Preferences.GetDecimalValue(CFG_SOUND_VOLUME, Value))
	{
		if (Value == 25 || Value == 50 || Value == 75 || Value == 100)
		{
			m_SoundVolume = Value;
		}
		else
		{
			m_SoundVolume = 100;
		}
	}
	else if (m_Preferences.GetDWORDValue(CFG_SOUND_VOLUME, dword))
	{
		switch (dword)
		{
			case 40017:          m_SoundVolume = 75; break;
			case 40018:          m_SoundVolume = 50; break;
			case 40019:          m_SoundVolume = 25; break;
			case 40021: default: m_SoundVolume = 100; break;
		}
	}
	else
	{
		m_SoundVolume = 100;
	}

	if (!m_Preferences.GetBoolValue("RelaySoundEnabled", RelaySoundEnabled))
		RelaySoundEnabled = false;
	if (!m_Preferences.GetBoolValue("TapeSoundEnabled", TapeSoundEnabled))
		TapeSoundEnabled = false;
	if (!m_Preferences.GetBoolValue("DiscDriveSoundEnabled", DiscDriveSoundEnabled))
		DiscDriveSoundEnabled = true;
	if (!m_Preferences.GetBoolValue("Part Samples", PartSamples))
		PartSamples = true;
	if (!m_Preferences.GetBoolValue("ExponentialVolume", SoundExponentialVolume))
		SoundExponentialVolume = true;
	if (!m_Preferences.GetBoolValue("TextToSpeechEnabled", m_TextToSpeechEnabled))
		m_TextToSpeechEnabled = false;
	if (!m_Preferences.GetBoolValue("TextToSpeechAutoSpeak", m_SpeechWriteChar))
		m_SpeechWriteChar = true;
	if (!m_Preferences.GetBoolValue("TextToSpeechSpeakPunctuation", m_SpeechSpeakPunctuation))
		m_SpeechSpeakPunctuation = false;

	DWORD TextToSpeechRate;
	if (m_Preferences.GetDWORDValue("TextToSpeechRate", TextToSpeechRate))
	{
		m_SpeechRate = Clamp(TextToSpeechRate, -10, 10);
	}
	else
	{
		m_SpeechRate = 0;
	}

	if (!m_Preferences.GetBoolValue("Music5000Enabled", Music5000Enabled))
		Music5000Enabled = false;

	if (m_Preferences.GetDWORDValue(CFG_OPTIONS_STICKS, dword))
	{
		switch (dword)
		{
			case 1: case 40030: m_JoystickOption = JoystickOption::Joystick; break;
			case 2: case 40205: m_JoystickOption = JoystickOption::AnalogueMouseStick; break;
			case 3: case 40206: m_JoystickOption = JoystickOption::DigitalMouseStick; break;
			case 0: default:    m_JoystickOption = JoystickOption::Disabled; break;
		}
	}
	else
	{
		m_JoystickOption = JoystickOption::Disabled;
	}

	if (!m_Preferences.GetBoolValue(CFG_OPTIONS_FREEZEINACTIVE, m_FreezeWhenInactive))
		m_FreezeWhenInactive = true;

	if (!m_Preferences.GetBoolValue(CFG_OPTIONS_HIDE_CURSOR, m_HideCursor))
		m_HideCursor = false;

	if (!m_Preferences.GetBoolValue(CFG_OPTIONS_CAPTURE_MOUSE, m_CaptureMouse))
		m_CaptureMouse = false;

	if (m_Preferences.GetDWORDValue(CFG_OPTIONS_KEY_MAPPING, dword))
	{
		switch (dword)
		{
			case 0: case 40060:          m_KeyboardMapping = KeyboardMappingType::User; break;
			case 1: case 40034:          m_KeyboardMapping = KeyboardMappingType::Default; break;
			case 2: case 40035: default: m_KeyboardMapping = KeyboardMappingType::Logical; break;
		}
	}
	else
	{
		m_KeyboardMapping = KeyboardMappingType::Logical;
	}

	bool readDefault = true;
	if (m_Preferences.GetStringValue(CFG_OPTIONS_USER_KEY_MAP_FILE, m_UserKeyMapPath))
	{
		strcpy(path, m_UserKeyMapPath);
		GetDataPath(m_UserDataPath, path);
		if (ReadKeyMap(path, &UserKeyMap))
			readDefault = false;
	}
	if (readDefault)
	{
		strcpy(m_UserKeyMapPath, "DefaultUser.kmap");
		strcpy(path, m_UserKeyMapPath);
		GetDataPath(m_UserDataPath, path);
		ReadKeyMap(path, &UserKeyMap);
	}

	if (!m_Preferences.GetBoolValue("KeyMapAS", m_KeyMapAS))
		m_KeyMapAS = false;

	if (!m_Preferences.GetBoolValue("KeyMapFunc", m_KeyMapFunc))
		m_KeyMapFunc = false;

	TranslateKeyMapping();

	if (!m_Preferences.GetBoolValue("DisableKeysBreak", m_DisableKeysBreak))
		m_DisableKeysBreak = false;

	if (!m_Preferences.GetBoolValue("DisableKeysEscape", m_DisableKeysEscape))
		m_DisableKeysEscape = false;

	if (!m_Preferences.GetBoolValue("DisableKeysShortcut", m_DisableKeysShortcut))
		m_DisableKeysShortcut = false;

	if (!m_Preferences.GetBoolValue("AutoSavePrefsCMOS", m_AutoSavePrefsCMOS))
		m_AutoSavePrefsCMOS = false;

	if (!m_Preferences.GetBoolValue("AutoSavePrefsFolders", m_AutoSavePrefsFolders))
		m_AutoSavePrefsFolders = false;

	if (!m_Preferences.GetBoolValue("AutoSavePrefsAll", m_AutoSavePrefsAll))
		m_AutoSavePrefsAll = false;

	if (m_Preferences.GetBinaryValue("BitKeys", keyData, 8))
	{
		for (int key=0; key<8; ++key)
			BitKeys[key] = keyData[key];
	}

	if (!m_Preferences.GetBoolValue(CFG_AMX_ENABLED, AMXMouseEnabled))
		AMXMouseEnabled = false;

	if (m_Preferences.GetDWORDValue(CFG_AMX_LRFORMIDDLE, dword))
		AMXLRForMiddle = dword != 0;
	else
		AMXLRForMiddle = true;

	if (m_Preferences.GetDWORDValue(CFG_AMX_SIZE, dword))
	{
		switch (dword)
		{
			case 0: case IDM_AMX_160X256:          m_AMXSize = AMXSizeType::_160x256; break;
			case 1: case IDM_AMX_320X256: default: m_AMXSize = AMXSizeType::_320x256; break;
			case 2: case IDM_AMX_640X256:          m_AMXSize = AMXSizeType::_640x256; break;
		}
	}
	else
	{
		m_AMXSize = AMXSizeType::_320x256;
	}

	if (m_Preferences.GetDecimalValue(CFG_AMX_ADJUST, Value))
	{
		if (Value == 0 || Value == 10 || Value == 30 || Value == 50 ||
		    Value == -10 || Value == -30 || Value == -50)
		{
			m_AMXAdjust = Value;
		}
		else
		{
			m_AMXAdjust = 30;
		}
	}
	else if (m_Preferences.GetDWORDValue(CFG_AMX_ADJUST, dword))
	{
		switch (dword)
		{
			case 40072:          m_AMXAdjust = 50; break;
			case 40073: default: m_AMXAdjust = 30; break;
			case 40074:          m_AMXAdjust = 10; break;
			case 40075:          m_AMXAdjust = -10; break;
			case 40076:          m_AMXAdjust = -30; break;
			case 40077:          m_AMXAdjust = -50; break;
		}
	}
	else
	{
		m_AMXAdjust = 30;
	}

	if (!m_Preferences.GetBoolValue(CFG_PRINTER_ENABLED, PrinterEnabled))
		PrinterEnabled = false;

	if (m_Preferences.GetDWORDValue(CFG_PRINTER_PORT, dword))
	{
		switch (dword)
		{
			case 0: case 40081:          m_PrinterPort = PrinterPortType::File; break;
			case 1: case 40244:          m_PrinterPort = PrinterPortType::Clipboard; break;
			case 2: case 40082: default: m_PrinterPort = PrinterPortType::Lpt1; break;
			case 3: case 40083:          m_PrinterPort = PrinterPortType::Lpt2; break;
			case 4: case 40084:          m_PrinterPort = PrinterPortType::Lpt3; break;
			case 5: case 40085:          m_PrinterPort = PrinterPortType::Lpt4; break;
		}
	}

	if (!m_Preferences.GetStringValue(CFG_PRINTER_FILE, m_PrinterFileName))
		m_PrinterFileName[0] = 0;

	TranslatePrinterPort();

	if (!m_Preferences.GetBinaryValue("Tape Clock Speed", &TapeState.ClockSpeed, 2))
		TapeState.ClockSpeed = 5600;
	if (!m_Preferences.GetBoolValue("UnlockTape", TapeState.Unlock))
		TapeState.Unlock = false;

	if (!m_Preferences.GetBoolValue("SerialPortEnabled", SerialPortEnabled))
		SerialPortEnabled = false;
	bool TouchScreenEnabled;
	if (!m_Preferences.GetBoolValue("TouchScreenEnabled", TouchScreenEnabled))
		TouchScreenEnabled = false;

	bool IP232Enabled;
	bool IP232localhost;
	bool IP232custom;

	if (!m_Preferences.GetBoolValue("IP232Enabled", IP232Enabled))
		IP232Enabled = false;
	if (!m_Preferences.GetBoolValue("IP232localhost", IP232localhost))
		IP232localhost = false;
	if (!m_Preferences.GetBoolValue("IP232custom", IP232custom))
		IP232custom = false;

	if (TouchScreenEnabled)
	{
		SerialDestination = SerialType::TouchScreen;
	}
	else if (IP232Enabled || IP232localhost || IP232custom)
	{
		SerialDestination = SerialType::IP232;
	}
	else
	{
		SerialDestination = SerialType::SerialPort;
	}

	char IPAddress[MAX_PATH];

	if (m_Preferences.GetStringValue("IP232Address", IPAddress))
	{
		strcpy(IP232Address, IPAddress);
	}
	else if (m_Preferences.GetStringValue("IP232customip", IPAddress) && IP232custom)
	{
		strcpy(IP232Address, IPAddress);
	}
	else
	{
		strcpy(IP232Address, "127.0.0.1");
	}

	if (m_Preferences.GetDWORDValue("IP232Port", dword))
	{
		IP232Port = dword;
	}
	else if (m_Preferences.GetDWORDValue("IP232customport", dword) && IP232custom)
	{
		IP232Port = dword;
	}
	else
	{
		IP232Port = 25232;
	}

	if (!m_Preferences.GetBoolValue("IP232Mode", IP232Mode))
		if (!m_Preferences.GetBoolValue("IP232mode", IP232Mode))
			IP232Mode = false;

	if (!m_Preferences.GetBoolValue("IP232Raw", IP232Raw))
		if (!m_Preferences.GetBoolValue("IP232raw", IP232Raw))
			IP232Raw = false;


	if (m_Preferences.GetStringValue("SerialPort", SerialPortName))
	{
		// For backwards compatibility with Preferences.cfg files from
		// BeebEm 4.18 and earlier, which stored the port number as a
		// binary value
		if (strlen(SerialPortName) == 2 &&
		    isxdigit(SerialPortName[0]) &&
		    isxdigit(SerialPortName[1]))
		{
			int Port;
			sscanf(SerialPortName, "%x", &Port);
			sprintf(SerialPortName, "COM%d", Port);
		}
	}
	else
	{
		strcpy(SerialPortName, "COM2");
	}

	if (!m_Preferences.GetBoolValue("EconetEnabled", EconetEnabled))
		EconetEnabled = false;

	#if ENABLE_SPEECH

	if (!m_Preferences.GetBoolValue("SpeechEnabled", SpeechDefault))
		SpeechDefault = false;

	#endif

	if (!m_Preferences.GetBinaryValue("SWRAMWritable", RomWritePrefs, 16))
	{
		for (int slot = 0; slot < 16; ++slot)
			RomWritePrefs[slot] = true;
	}

	if (!m_Preferences.GetBoolValue("SWRAMBoard", SWRAMBoardEnabled))
		SWRAMBoardEnabled = false;

	if (m_Preferences.GetBinaryValue(CFG_TUBE_TYPE, &type, 1))
	{
		TubeType = static_cast<Tube>(type);
	}
	else
	{
		// For backwards compatibility with BeebEm 4.14 or earlier:
		unsigned char TubeEnabled = 0;
		unsigned char AcornZ80 = 0;
		unsigned char TorchTube = 0;
		unsigned char Tube186Enabled = 0;
		unsigned char ArmTube = 0;

		m_Preferences.GetBinaryValue("TubeEnabled", &TubeEnabled, 1);
		m_Preferences.GetBinaryValue("AcornZ80", &AcornZ80, 1);
		m_Preferences.GetBinaryValue("TorchTube", &TorchTube, 1);
		m_Preferences.GetBinaryValue("Tube186Enabled", &Tube186Enabled, 1);
		m_Preferences.GetBinaryValue("ArmTube", &ArmTube, 1);

		if (TubeEnabled)
		{
			TubeType = Tube::Acorn65C02;
		}
		else if (AcornZ80)
		{
			TubeType = Tube::AcornZ80;
		}
		else if (TorchTube)
		{
			TubeType = Tube::TorchZ80;
		}
		else if (Tube186Enabled)
		{
			TubeType = Tube::Master512CoPro;
		}
		else if (ArmTube)
		{
			TubeType = Tube::AcornArm;
		}
		else
		{
			TubeType = Tube::None;
		}
	}

	if (!m_Preferences.GetBoolValue("Basic Hardware", BasicHardwareOnly))
		BasicHardwareOnly = false;
	if (!m_Preferences.GetBoolValue("Teletext Half Mode", TeletextHalfMode))
		TeletextHalfMode = false;

	if (!m_Preferences.GetBoolValue("TeletextAdapterEnabled", TeletextAdapterEnabled))
		TeletextAdapterEnabled = false;


	TeletextSource = TeletextSourceType::IP;

	unsigned char TeletextAdapterSource = 0;

	if (m_Preferences.GetBinaryValue("TeletextAdapterSource", &TeletextAdapterSource, 1))
	{
		if (TeletextAdapterSource < static_cast<unsigned char>(TeletextSourceType::Last))
		{
			TeletextSource = static_cast<TeletextSourceType>(TeletextAdapterSource);
		}
	}
	else
	{
		bool TeletextLocalhost = false;
		bool TeletextCustom = false;

		m_Preferences.GetBoolValue("TeletextLocalhost", TeletextLocalhost);
		m_Preferences.GetBoolValue("TeletextCustom", TeletextCustom);

		if (!(TeletextLocalhost || TeletextCustom))
			TeletextSource = TeletextSourceType::File;
	}

	char key[20];

	for (int ch = 0; ch < TELETEXT_CHANNEL_COUNT; ch++)
	{
		sprintf(key, "TeletextFile%d", ch);

		if (!m_Preferences.GetStringValue(key, TeletextFileName[ch]))
		{
			char DiscsPath[MAX_PATH];
			m_Preferences.GetStringValue("DiscsPath", DiscsPath);
			GetDataPath(m_UserDataPath, DiscsPath);

			char TeletextFile[256];
			sprintf(TeletextFile, "%s\\txt%d.dat", DiscsPath, ch);

			TeletextFileName[ch] = TeletextFile;
		}

		std::string TeletextIPAddress;

		sprintf(key, "TeletextIP%d", ch);

		if (m_Preferences.GetStringValue(key, TeletextIPAddress))
		{
			TeletextIP[ch] = TeletextIPAddress;
		}
		else
		{
			sprintf(key, "TeletextCustomIP%d", ch);

			if (m_Preferences.GetStringValue(key, TeletextIPAddress))
			{
				TeletextIP[ch] = TeletextIPAddress;
			}
			else
			{
				TeletextIP[ch] = "127.0.0.1";
			}
		}

		sprintf(key, "TeletextPort%d", ch);

		if (m_Preferences.GetDWORDValue(key, dword))
		{
			TeletextPort[ch] = (u_short)dword;
		}
		else
		{
			sprintf(key, "TeletextCustomPort%d", ch);

			if (m_Preferences.GetDWORDValue(key, dword))
			{
				TeletextPort[ch] = (u_short)dword;
			}
			else
			{
				TeletextPort[ch] = (u_short)(TELETEXT_BASE_PORT + ch);
			}
		}
	}

	dword = 0;
	m_Preferences.GetDWORDValue("KeyboardLinks", dword);
	KeyboardLinks = (unsigned char)dword;

	if (!m_Preferences.GetBoolValue("FloppyDriveEnabled", Disc8271Enabled))
		Disc8271Enabled = true;
	Disc1770Enabled = Disc8271Enabled;

	if (!m_Preferences.GetBoolValue("SCSIDriveEnabled", SCSIDriveEnabled))
		SCSIDriveEnabled = false;

	if (!m_Preferences.GetBoolValue("IDEDriveEnabled", IDEDriveEnabled))
		IDEDriveEnabled = false;

	if (!m_Preferences.GetBoolValue("UserPortRTCEnabled", UserPortRTCEnabled))
	{
		if (!m_Preferences.GetBoolValue("RTCEnabled", UserPortRTCEnabled))
		{
			UserPortRTCEnabled = false;
		}
	}

	if (!m_Preferences.GetBinaryValue("UserPortRTCRegisters", UserPortRTCRegisters, sizeof(UserPortRTCRegisters)))
	{
		ZeroMemory(UserPortRTCRegisters, sizeof(UserPortRTCRegisters));
	}

	if (m_Preferences.GetDWORDValue("CaptureResolution", dword))
	{
		switch (dword)
		{
			case 0: case 40185:          m_VideoCaptureResolution = VideoCaptureResolution::Display;
			case 1: case 40186: default: m_VideoCaptureResolution = VideoCaptureResolution::_640x512;
			case 2: case 40187:          m_VideoCaptureResolution = VideoCaptureResolution::_320x256;
		}
	}
	else
	{
		m_VideoCaptureResolution = VideoCaptureResolution::_640x512;
	}

	if (m_Preferences.GetDecimalValue("CaptureFrameSkip", Value))
	{
		if (Value >= 0 && Value <= 5)
		{
			m_AviFrameSkip = Value;
		}
		else
		{
			m_AviFrameSkip = 1;
		}
	}
	else if (m_Preferences.GetDWORDValue("FrameSkip", dword))
	{
		switch (dword)
		{
			case 40188:          m_AviFrameSkip = 0; break;
			case 40189: default: m_AviFrameSkip = 1; break;
			case 40190:          m_AviFrameSkip = 2; break;
			case 40191:          m_AviFrameSkip = 3; break;
			case 40192:          m_AviFrameSkip = 4; break;
			case 40193:          m_AviFrameSkip = 5; break;
		}
	}
	else
	{
		m_AviFrameSkip = 1;
	}

	if (m_Preferences.GetDWORDValue("BitmapCaptureResolution", dword))
	{
		switch (dword)
		{
			case 0: case 40262:          m_BitmapCaptureResolution = BitmapCaptureResolution::Display; break;
			case 1: case 40263:          m_BitmapCaptureResolution = BitmapCaptureResolution::_1280x1024; break;
			case 2: case 40264: default: m_BitmapCaptureResolution = BitmapCaptureResolution::_640x512; break;
			case 3: case 40265:          m_BitmapCaptureResolution = BitmapCaptureResolution::_320x256; break;
		}
	}
	else
	{
		m_BitmapCaptureResolution = BitmapCaptureResolution::_640x512;
	}

	if (m_Preferences.GetDWORDValue("BitmapCaptureFormat", dword))
	{
		switch (dword)
		{
			case 0: case 40266: default: m_BitmapCaptureFormat = BitmapCaptureFormat::Bmp; break;
			case 1: case 40267:          m_BitmapCaptureFormat = BitmapCaptureFormat::Jpeg; break;
			case 2: case 40268:          m_BitmapCaptureFormat = BitmapCaptureFormat::Gif; break;
			case 3: case 40269:          m_BitmapCaptureFormat = BitmapCaptureFormat::Png; break;
		}
	}
	else
	{
		m_BitmapCaptureFormat = BitmapCaptureFormat::Bmp;
	}

	RECT rect;
	if (m_Preferences.GetBinaryValue("WindowPos", &rect, sizeof(rect)))
	{
		m_XWinPos = rect.left;
		m_YWinPos = rect.top;

		// Pos can get corrupted if two BeebEm's exited at same time
		RECT scrrect;
		SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&scrrect, 0);
		if (m_XWinPos < 0)
			m_XWinPos = 0;
		if (m_XWinPos > scrrect.right - 80)
			m_XWinPos = 0;
		if (m_YWinPos < 0)
			m_YWinPos = 0;
		if (m_YWinPos > scrrect.bottom - 80)
			m_YWinPos = 0;
	}
	else
	{
		m_XWinPos = -1;
		m_YWinPos = -1;
	}

	// CMOS RAM now in prefs file
	unsigned char Data[50];

	if (m_Preferences.GetBinaryValue("CMOSRam", Data, 50))
	{
		RTCSetCMOSData(Model::Master128, Data, 50);
	}
	else
	{
		RTCSetCMOSDefaults(Model::Master128);
	}

	if (m_Preferences.GetBinaryValue("CMOSRamMasterET", Data, 50))
	{
		RTCSetCMOSData(Model::MasterET, Data, 50);
	}
	else
	{
		RTCSetCMOSDefaults(Model::MasterET);
	}

	// Set FDC defaults if not already set
	for (int machine = 0; machine < static_cast<int>(Model::Master128); ++machine)
	{
		char CfgName[256];
		sprintf(CfgName, "FDCDLL%d", machine);

		if (!m_Preferences.HasValue(CfgName))
		{
			// Default B+ to Acorn FDC
			if (machine == static_cast<int>(Model::BPlus))
				strcpy(path, "Hardware\\Acorn1770.dll");
			else
				strcpy(path, "None");
			m_Preferences.SetStringValue(CfgName, path);
		}
	}

	// Update prefs version
	m_Preferences.SetStringValue("PrefsVersion", "2.1");

	// Windows key enable/disable still comes from registry
	int binsize = 24;
	if (RegGetBinaryValue(HKEY_LOCAL_MACHINE,CFG_KEYBOARD_LAYOUT,
						  CFG_SCANCODE_MAP,keyData,&binsize) && binsize==24)
		m_DisableKeysWindows = true;
	else
		m_DisableKeysWindows = false;

	if (!m_Preferences.GetBoolValue("WriteInstructionCounts", m_WriteInstructionCounts))
		m_WriteInstructionCounts = false;
}

/****************************************************************************/
void BeebWin::SavePreferences(bool saveAll)
{
	unsigned char LEDByte = 0;
	unsigned char flag;
	char keyData[256];

	if (saveAll)
	{
		m_Preferences.SetBinaryValue(CFG_MACHINE_TYPE, &MachineType, 1);
		m_Preferences.SetBoolValue("WriteProtectOnLoad", m_WriteProtectOnLoad);
		m_Preferences.SetDWORDValue("DisplayRenderer", (DWORD)m_DisplayRenderer);
		m_Preferences.SetBoolValue("DXSmoothing", m_DXSmoothing);
		m_Preferences.SetBoolValue("DXSmoothMode7Only", m_DXSmoothMode7Only);
		m_Preferences.SetDWORDValue("DDFullScreenMode", (DWORD)m_DDFullScreenMode);
		m_Preferences.EraseValue(CFG_VIEW_WIN_SIZE);
		m_Preferences.SetDWORDValue("WinSizeX", m_XLastWinSize);
		m_Preferences.SetDWORDValue("WinSizeY", m_YLastWinSize);
		m_Preferences.SetBoolValue("FullScreen", m_isFullScreen);
		m_Preferences.SetBoolValue("MaintainAspectRatio", m_MaintainAspectRatio);
		m_Preferences.SetBoolValue("ShowFPS", m_ShowSpeedAndFPS);
		m_Preferences.SetBinaryValue(CFG_VIEW_MONITOR, &m_PaletteType, 1);
		m_Preferences.SetBoolValue("HideMenuEnabled", m_HideMenuEnabled);
		LEDByte = static_cast<unsigned char>(
			(m_DiscLedColour == LEDColour::Green ? 4 : 0) |
			(LEDs.ShowDisc ? 2 : 0) |
			(LEDs.ShowKB ? 1 : 0)
		);
		m_Preferences.SetBinaryValue("LED Information", &LEDByte, 1);
		m_Preferences.SetDWORDValue("MotionBlur", (DWORD)m_MotionBlur);
		m_Preferences.SetBinaryValue("MotionBlurIntensities", m_BlurIntensities, 8);
		m_Preferences.SetBoolValue("TextViewEnabled", m_TextViewEnabled);

		m_Preferences.SetDWORDValue(CFG_SPEED_TIMING, (DWORD)m_TimingType);
		m_Preferences.SetDecimalValue("Speed", m_TimingSpeed);

		m_Preferences.SetDWORDValue("SoundConfig::Selection", (DWORD)SelectedSoundStreamer);
		m_Preferences.SetBoolValue(CFG_SOUND_ENABLED, SoundDefault);
		m_Preferences.SetBoolValue("SoundChipEnabled", SoundChipEnabled);
		m_Preferences.SetDecimalValue(CFG_SOUND_SAMPLE_RATE, m_SampleRate);
		m_Preferences.SetDecimalValue(CFG_SOUND_VOLUME, m_SoundVolume);
		m_Preferences.SetBoolValue("RelaySoundEnabled", RelaySoundEnabled);
		m_Preferences.SetBoolValue("TapeSoundEnabled", TapeSoundEnabled);
		m_Preferences.SetBoolValue("DiscDriveSoundEnabled", DiscDriveSoundEnabled);
		m_Preferences.SetBoolValue("Part Samples", PartSamples);
		m_Preferences.SetBoolValue("ExponentialVolume", SoundExponentialVolume);
		m_Preferences.SetBoolValue("TextToSpeechEnabled", m_TextToSpeechEnabled);
		m_Preferences.SetDWORDValue("TextToSpeechRate", m_SpeechRate);
		m_Preferences.SetBoolValue("TextToSpeechAutoSpeak", m_SpeechWriteChar);
		m_Preferences.SetBoolValue("TextToSpeechSpeakPunctuation", m_SpeechSpeakPunctuation);
		m_Preferences.SetBoolValue("Music5000Enabled", Music5000Enabled);

		m_Preferences.SetDWORDValue(CFG_OPTIONS_STICKS, (DWORD)m_JoystickOption);
		m_Preferences.SetBoolValue(CFG_OPTIONS_FREEZEINACTIVE, m_FreezeWhenInactive);
		m_Preferences.SetBoolValue(CFG_OPTIONS_HIDE_CURSOR, m_HideCursor);
		m_Preferences.SetBoolValue(CFG_OPTIONS_CAPTURE_MOUSE, m_CaptureMouse);
		m_Preferences.SetDWORDValue(CFG_OPTIONS_KEY_MAPPING, (DWORD)m_KeyboardMapping);
		m_Preferences.SetBoolValue("KeyMapAS", m_KeyMapAS);
		flag = m_KeyMapFunc;
		m_Preferences.SetBinaryValue("KeyMapFunc", &flag, 1);

		m_Preferences.SetBoolValue("DisableKeysBreak", m_DisableKeysBreak);
		m_Preferences.SetBoolValue("DisableKeysEscape", m_DisableKeysEscape);
		m_Preferences.SetBoolValue("DisableKeysShortcut", m_DisableKeysShortcut);

		for (int key = 0; key < 8; ++key)
		{
			keyData[key] = static_cast<char>(BitKeys[key]);
		}

		m_Preferences.SetBinaryValue("BitKeys", keyData, 8);

		m_Preferences.SetStringValue(CFG_OPTIONS_USER_KEY_MAP_FILE, m_UserKeyMapPath);

		m_Preferences.SetBoolValue(CFG_AMX_ENABLED, AMXMouseEnabled);
		m_Preferences.SetDWORDValue(CFG_AMX_LRFORMIDDLE, AMXLRForMiddle);
		m_Preferences.SetDWORDValue(CFG_AMX_SIZE, (DWORD)m_AMXSize);
		m_Preferences.SetDecimalValue(CFG_AMX_ADJUST, m_AMXAdjust);

		m_Preferences.SetBoolValue(CFG_PRINTER_ENABLED, PrinterEnabled);
		m_Preferences.SetDWORDValue(CFG_PRINTER_PORT, (DWORD)m_PrinterPort);
		m_Preferences.SetStringValue(CFG_PRINTER_FILE, m_PrinterFileName);

		m_Preferences.SetBinaryValue("Tape Clock Speed", &TapeState.ClockSpeed, 2);
		m_Preferences.SetBoolValue("UnlockTape", TapeState.Unlock);
		m_Preferences.SetBoolValue("SerialPortEnabled", SerialPortEnabled);
		m_Preferences.SetBoolValue("TouchScreenEnabled", SerialDestination == SerialType::TouchScreen);
		m_Preferences.SetBoolValue("IP232Enabled", SerialDestination == SerialType::IP232);
		m_Preferences.SetStringValue("IP232Address", IP232Address);
		m_Preferences.SetDWORDValue("IP232Port", IP232Port);
		m_Preferences.EraseValue("IP232mode");
		m_Preferences.SetBoolValue("IP232Mode", IP232Mode);
		m_Preferences.EraseValue("IP232raw");
		m_Preferences.SetBoolValue("IP232Raw", IP232Raw);

		m_Preferences.SetStringValue("SerialPort", SerialPortName);

		m_Preferences.SetBoolValue("EconetEnabled", EconetEnabled); // Rob

		#if ENABLE_SPEECH
		m_Preferences.SetBoolValue("SpeechEnabled", SpeechDefault);
		#else
		m_Preferences.SetBoolValue("SpeechEnabled", false);
		#endif

		for (int slot = 0; slot < 16; ++slot)
			RomWritePrefs[slot] = RomWritable[slot];
		m_Preferences.SetBinaryValue("SWRAMWritable", RomWritePrefs, 16);
		m_Preferences.SetBoolValue("SWRAMBoard", SWRAMBoardEnabled);

		m_Preferences.SetBinaryValue(CFG_TUBE_TYPE, &TubeType, 1);

		m_Preferences.SetBoolValue("Basic Hardware", BasicHardwareOnly);
		m_Preferences.SetBoolValue("Teletext Half Mode", TeletextHalfMode);
		m_Preferences.SetBoolValue("TeletextAdapterEnabled", TeletextAdapterEnabled);
		m_Preferences.SetBinaryValue("TeletextAdapterSource", &TeletextSource, 1);

		char key[20];

		for (int ch = 0; ch < TELETEXT_CHANNEL_COUNT; ch++)
		{
			sprintf(key, "TeletextFile%d", ch);
			m_Preferences.SetStringValue(key, TeletextFileName[ch]);
			sprintf(key, "TeletextPort%d", ch);
			m_Preferences.SetDWORDValue(key, TeletextPort[ch]);
			sprintf(key, "TeletextIP%d", ch);
			m_Preferences.SetStringValue(key, TeletextIP[ch]);
		}

		m_Preferences.SetDWORDValue("KeyboardLinks", KeyboardLinks);

		m_Preferences.SetBoolValue("FloppyDriveEnabled", Disc8271Enabled);
		m_Preferences.SetBoolValue("SCSIDriveEnabled", SCSIDriveEnabled);
		m_Preferences.SetBoolValue("IDEDriveEnabled", IDEDriveEnabled);
		m_Preferences.SetBoolValue("UserPortRTCEnabled", UserPortRTCEnabled);

		m_Preferences.SetDWORDValue("CaptureResolution", (DWORD)m_VideoCaptureResolution);
		m_Preferences.SetDecimalValue("CaptureFrameSkip", m_AviFrameSkip);
		m_Preferences.EraseValue("FrameSkip");

		m_Preferences.SetDWORDValue("BitmapCaptureResolution", (DWORD)m_BitmapCaptureResolution);
		m_Preferences.SetDWORDValue("BitmapCaptureFormat", (DWORD)m_BitmapCaptureFormat);

		RECT rect;
		GetWindowRect(m_hWnd,&rect);
		m_Preferences.SetBinaryValue("WindowPos", &rect, sizeof(rect));
	}

	// CMOS RAM now in prefs file
	if (saveAll || m_AutoSavePrefsCMOS)
	{
		m_Preferences.SetBinaryValue("CMOSRam", RTCGetCMOSData(Model::Master128), 50);
		m_Preferences.SetBinaryValue("CMOSRamMasterET", RTCGetCMOSData(Model::MasterET), 50);

		m_Preferences.SetBinaryValue("UserPortRTCRegisters", UserPortRTCRegisters, sizeof(UserPortRTCRegisters));
	}

	m_Preferences.SetBoolValue("AutoSavePrefsCMOS", m_AutoSavePrefsCMOS);
	m_Preferences.SetBoolValue("AutoSavePrefsFolders", m_AutoSavePrefsFolders);
	m_Preferences.SetBoolValue("AutoSavePrefsAll", m_AutoSavePrefsAll);

	m_Preferences.SetBoolValue("WriteInstructionCounts", m_WriteInstructionCounts);

	if (m_Preferences.Save(m_PrefsFile) == Preferences::Result::Success) {
		m_AutoSavePrefsChanged = false;
	}
	else {
		Report(MessageType::Error,
		       "Failed to write preferences file:\n  %s",
		       m_PrefsFile);
	}
}
