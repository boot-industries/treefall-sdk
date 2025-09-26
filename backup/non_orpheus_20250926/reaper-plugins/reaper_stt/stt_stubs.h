#pragma once
#include <stdint.h>
struct RECT { int left, top, right, bottom; };
struct MSG { int dummy; };
struct ACCEL { uint8_t fVirt; uint16_t key; uint16_t cmd; };
typedef uint32_t DWORD;
typedef int32_t UINT;
typedef long long WDL_INT64;
typedef int LRESULT;
typedef long INT_PTR;
typedef void* HMENU;
typedef void* HDC;
typedef void* HFONT;
struct GUID { uint32_t Data1; uint16_t Data2; uint16_t Data3; uint8_t Data4[8]; };
struct LICE_IFont { int dummy; };
struct LICE_IBitmap { int dummy; };
