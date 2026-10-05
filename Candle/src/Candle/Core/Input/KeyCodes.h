#pragma once

#include <cstdint>
#include <format>

namespace Candle {

	using KeyCodeType = uint16_t;

	enum class KeyCode : KeyCodeType
	{
		Unknown = 0,

		A = 4,
		B = 5,
		C = 6,
		D = 7,
		E = 8,
		F = 9,
		G = 10,
		H = 11,
		I = 12,
		J = 13,
		K = 14,
		L = 15,
		M = 16,
		N = 17,
		O = 18,
		P = 19,
		Q = 20,
		R = 21,
		S = 22,
		T = 23,
		U = 24,
		V = 25,
		W = 26,
		X = 27,
		Y = 28,
		Z = 29,

		D1 = 30,
		D2 = 31,
		D3 = 32,
		D4 = 33,
		D5 = 34,
		D6 = 35,
		D7 = 36,
		D8 = 37,
		D9 = 38,
		D0 = 39,

		Enter          = 40,
		Escape         = 41,
		Backspace      = 42,
		Tab            = 43,
		Space          = 44,
		Minus          = 45,    /* - */
		Equal          = 46,    /* = */
		LeftBracket    = 47,    /* [ */
		RightBracket   = 48,    /* ] */
		Backslash      = 49,    /* \ */
		NonUSHash      = 50,    /* ISO key left of Enter; most OSes report it as Backslash */
		Semicolon      = 51,    /* ; */
		Apostrophe     = 52,    /* ' */
		GraveAccent    = 53,    /* ` */
		Comma          = 54,    /* , */
		Period         = 55,    /* . */
		Slash          = 56,    /* / */
		CapsLock       = 57,

		/* Function keys */
		F1  = 58,
		F2  = 59,
		F3  = 60,
		F4  = 61,
		F5  = 62,
		F6  = 63,
		F7  = 64,
		F8  = 65,
		F9  = 66,
		F10 = 67,
		F11 = 68,
		F12 = 69,

		/* Navigation */
		PrintScreen = 70,
		ScrollLock  = 71,
		Pause       = 72,
		Insert      = 73,
		Home        = 74,
		PageUp      = 75,
		Delete      = 76,
		End         = 77,
		PageDown    = 78,
		Right       = 79,
		Left        = 80,
		Down        = 81,
		Up          = 82,

		/* NumPad */
		NumLock        = 83,    /* SDL: NUMLOCKCLEAR */
		NumPadDivide   = 84,
		NumPadMultiply = 85,
		NumPadSubtract = 86,
		NumPadAdd      = 87,
		NumPadEnter    = 88,
		NumPad1        = 89,
		NumPad2        = 90,
		NumPad3        = 91,
		NumPad4        = 92,
		NumPad5        = 93,
		NumPad6        = 94,
		NumPad7        = 95,
		NumPad8        = 96,
		NumPad9        = 97,
		NumPad0        = 98,
		NumPadDecimal  = 99,    /* the . key; SDL: KP_PERIOD */

		NonUSBackslash = 100,   /* ISO key between Left Shift and Z */
		Menu           = 101,   /* context-menu key, as Ember's Menu; SDL: APPLICATION */
		Power          = 102,
		NumPadEqual    = 103,

		/* Extended function keys */
		F13 = 104,
		F14 = 105,
		F15 = 106,
		F16 = 107,
		F17 = 108,
		F18 = 109,
		F19 = 110,
		F20 = 111,
		F21 = 112,
		F22 = 113,
		F23 = 114,
		F24 = 115,

		/* Legacy and editing keys */
		Execute    = 116,
		Help       = 117,
		ShowMenu   = 118,       /* HID "Keyboard Menu", not the context-menu key; SDL: MENU */
		Select     = 119,
		Stop       = 120,
		Again      = 121,
		Undo       = 122,
		Cut        = 123,
		Copy       = 124,
		Paste      = 125,
		Find       = 126,
		Mute       = 127,
		VolumeUp   = 128,
		VolumeDown = 129,

		NumPadComma      = 133,
		NumPadEqualAS400 = 134,

		/* International */
		International1 = 135,   /* Ro */
		International2 = 136,   /* Katakana/Hiragana */
		International3 = 137,   /* Yen */
		International4 = 138,   /* Henkan */
		International5 = 139,   /* Muhenkan */
		International6 = 140,
		International7 = 141,
		International8 = 142,
		International9 = 143,
		Lang1          = 144,   /* Hangul/English toggle */
		Lang2          = 145,   /* Hanja conversion */
		Lang3          = 146,   /* Katakana */
		Lang4          = 147,   /* Hiragana */
		Lang5          = 148,   /* Zenkaku/Hankaku */
		Lang6          = 149,
		Lang7          = 150,
		Lang8          = 151,
		Lang9          = 152,

		AltErase   = 153,
		SysReq     = 154,
		Cancel     = 155,
		Clear      = 156,
		Prior      = 157,
		Enter2     = 158,       /* SDL: RETURN2 */
		Separator  = 159,
		Out        = 160,
		Oper       = 161,
		ClearAgain = 162,
		CrSel      = 163,
		ExSel      = 164,

		/* Extended NumPad */
		NumPad00                = 176,
		NumPad000               = 177,
		ThousandsSeparator      = 178,
		DecimalSeparator        = 179,
		CurrencyUnit            = 180,
		CurrencySubunit         = 181,
		NumPadLeftParen         = 182,
		NumPadRightParen        = 183,
		NumPadLeftBrace         = 184,
		NumPadRightBrace        = 185,
		NumPadTab               = 186,
		NumPadBackspace         = 187,
		NumPadA                 = 188,
		NumPadB                 = 189,
		NumPadC                 = 190,
		NumPadD                 = 191,
		NumPadE                 = 192,
		NumPadF                 = 193,
		NumPadXor               = 194,
		NumPadPower             = 195,
		NumPadPercent           = 196,
		NumPadLess              = 197,
		NumPadGreater           = 198,
		NumPadAmpersand         = 199,
		NumPadDoubleAmpersand   = 200,
		NumPadVerticalBar       = 201,
		NumPadDoubleVerticalBar = 202,
		NumPadColon             = 203,
		NumPadHash              = 204,
		NumPadSpace             = 205,
		NumPadAt                = 206,
		NumPadExclamation       = 207,
		NumPadMemStore          = 208,
		NumPadMemRecall         = 209,
		NumPadMemClear          = 210,
		NumPadMemAdd            = 211,
		NumPadMemSubtract       = 212,
		NumPadMemMultiply       = 213,
		NumPadMemDivide         = 214,
		NumPadPlusMinus         = 215,
		NumPadClear             = 216,
		NumPadClearEntry        = 217,
		NumPadBinary            = 218,
		NumPadOctal             = 219,
		NumPadDecimalBase       = 220,  /* number-base selector, not the . key; SDL: KP_DECIMAL */
		NumPadHexadecimal       = 221,

		/* Modifier keys */
		LeftControl  = 224,
		LeftShift    = 225,
		LeftAlt      = 226,
		LeftSuper    = 227,         /* Windows key */
		RightControl = 228,
		RightShift   = 229,
		RightAlt     = 230,         /* AltGr on many layouts */
		RightSuper   = 231,

		Mode = 257,                 /* AltGr / Kana switch where not reported as RightAlt */

		/* Media and consumer-control keys */
		Sleep              = 258,
		Wake               = 259,
		ChannelIncrement   = 260,
		ChannelDecrement   = 261,
		MediaPlay          = 262,
		MediaPause         = 263,
		MediaRecord        = 264,
		MediaFastForward   = 265,
		MediaRewind        = 266,
		MediaNextTrack     = 267,
		MediaPreviousTrack = 268,
		MediaStop          = 269,
		MediaEject         = 270,
		MediaPlayPause     = 271,
		MediaSelect        = 272,

		/* Application-control keys (SDL: AC_*) */
		AppNew        = 273,
		AppOpen       = 274,
		AppClose      = 275,
		AppExit       = 276,
		AppSave       = 277,
		AppPrint      = 278,
		AppProperties = 279,
		AppSearch     = 280,
		AppHome       = 281,
		AppBack       = 282,
		AppForward    = 283,
		AppStop       = 284,
		AppRefresh    = 285,
		AppBookmarks  = 286,

		/* Mobile keys */
		SoftLeft  = 287,
		SoftRight = 288,
		Call      = 289,
		EndCall   = 290,

		/* 400-499 are scancodes SDL assigns at runtime to unmapped keys: valid, but unnamed. */

		/* One past the largest scancode, for array sizing; equals SDL_SCANCODE_COUNT. */
		Count = 512
	};

}

template<>
struct std::formatter<Candle::KeyCode> : std::formatter<int>
{
	auto format(Candle::KeyCode keyCode, std::format_context& ctx) const
	{
		return std::formatter<int>::format(static_cast<int>(keyCode), ctx);
	}
};
